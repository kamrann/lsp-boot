
module;

// workaround: MSVC modules template specialization and boost::system::error_code
#if defined(_MSC_VER) && !defined(__clang__)
#include <boost/json/value_to.hpp>
#endif

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <optional>
#include <string_view>
#include <string>
#include <ranges>
#include <chrono>
#include <format>
#endif

module lsp_boot.server;

using namespace std::string_view_literals;

namespace lsp_boot
{
	using namespace lsp;

	auto Server::dispatch_request(std::string_view const method, lsp::RawMessage&& msg) -> InternalMessageResult
	{
		auto request_id = msg.at(keys::id);

		log("Dispatching request: id={}, method={}", boost::json::serialize(request_id), method);
		log([&](auto out) {
			return std::ranges::copy(boost::json::serialize(msg), out).out;
			});

		// @todo: not sure how best to appoach this, but as we currently return the request result synchronously we need to ensure that
		// any pending notifications or prior requests that could potentially affect our result have already been processed.
		// this would be fairly complex to try to handle based on what notifications there were and in what order they came, so for now
		// we simply enforce synchronization before each request.
		// may want to consider at least making the response async so we can move the pump invocation off the server dispatching thread.
		impl.pump();

		auto result = [&]() -> RequestResult {
			// @todo: consider dispatching via type list
			if (method == requests::Initialize::name)
			{
				return handle_request(requests::Initialize(std::move(msg)));
			}
			else if (method == requests::Shutdown::name)
			{
				return handle_request(requests::Shutdown(std::move(msg)));
			}
			//else if (method == requests::DocumentHighlight::name)
			//{
			//	return handle_request(requests::DocumentHighlight(std::move(msg)));
			//}
			else if (method == requests::InlayHint::name)
			{
				return handle_request(requests::InlayHint(std::move(msg)));
			}
			else if (method == requests::Hover::name)
			{
				return handle_request(requests::Hover(std::move(msg)));
			}
			else if (method == requests::SemanticTokensFull::name)
			{
				return handle_request(requests::SemanticTokensFull(std::move(msg)));
			}
			else if (method == requests::SemanticTokensRange::name)
			{
				return handle_request(requests::SemanticTokensRange(std::move(msg)));
			}
			else if (method == requests::DocumentSymbols::name)
			{
				return handle_request(requests::DocumentSymbols(std::move(msg)));
			}
			else
			{
				return make_not_implemented_result();
			}
			}();

		// @todo: currently server implementation is able to send notifications itself, but responses are expected to be returned synchronously and handled here.
		// feels inconsistent, and likely will want to allow an implementation to generate an async response - though could do that with similar approach using a future.
		auto response = [&] {
			auto json = boost::json::object{
				{ "jsonrpc", "2.0" },
				{ keys::id, std::move(request_id) },
			};

			if (result.has_value())
			{
				static constexpr auto max_log_chars = 128;
				log([&](auto out) {
					return std::format_to(out, "Queueing response [success]: result={:s}...", boost::json::serialize(result->json) | std::views::take(max_log_chars));
					});

				json["result"] = std::move(result->json);
			}
			else
			{
				log([&](auto out) {
					return std::format_to(out, "Queueing response [failure]: error={}", boost::json::serialize(result.error()));
					});

				json["error"] = std::move(result).error();
			}

			return json;
			}();
		out_queue.push(std::move(response));

		return {};
	}

	auto Server::dispatch_notification(std::string_view const method, lsp::RawMessage&& msg) -> InternalMessageResult
	{
		log("Dispatching notification: method={}", method);
		log([&](auto out) {
			return std::ranges::copy(boost::json::serialize(msg), out).out;
			});

		if (method == notifications::Initialized::name)
		{
			// client initialization completed
			handle_notification(notifications::Initialized(std::move(msg)));
		}
		else if (method == notifications::Exit::name)
		{
			return {
				.exit = true,
			};
		}
		else if (method == notifications::DidChangeConfiguration::name)
		{
			handle_notification(notifications::DidChangeConfiguration(std::move(msg)));
		}
		else if (method == notifications::DidOpenTextDocument::name)
		{
			handle_notification(notifications::DidOpenTextDocument(std::move(msg)));
		}
		else if (method == notifications::DidChangeTextDocument::name)
		{
			handle_notification(notifications::DidChangeTextDocument(std::move(msg)));
		}
		else if (method == notifications::DidCloseTextDocument::name)
		{
			handle_notification(notifications::DidCloseTextDocument(std::move(msg)));
		}

		return {};
	}

	auto Server::dispatch_message(ReceivedMessage&& msg) -> DispatchResult
	{
		auto const dispatch_start_timestamp = std::chrono::system_clock::now();

		auto&& json_msg = msg.msg;
		auto const id_it = json_msg.find(keys::id);
		auto const method_it = json_msg.find(keys::method);

		auto const extract_document_id = [&]() -> std::string_view {
			// @todo: intention is to include the readable name of the document. to do so will need to move top level control of the active documents
			// from the implementation into the base server. which probably does make sense to do.
			try
			{
				return message_params(json_msg)
					.at(keys::text_document).as_object()
					.at(keys::uri).as_string();
			}
			catch (...)
			{
				return "?"sv;
			}
			};

		auto identifier = std::format("{}:{} [{}]",
			id_it != json_msg.end() ? std::to_string(value_to< std::uint64_t >(id_it->value())) : "?",
			method_it != json_msg.end() ? std::string_view{ method_it->value().as_string() } : "?"sv,
			extract_document_id());

		auto const result = [&] {
			if (id_it != json_msg.end())
			{
				if (method_it != json_msg.end())
				{
					return dispatch_request(method_it->value().as_string(), std::move(json_msg));
				}
			}
			else if (method_it != json_msg.end())
			{
				return dispatch_notification(method_it->value().as_string(), std::move(json_msg));
			}
			return InternalMessageResult{};
			}();

		auto const dispatch_completion_timestamp = std::chrono::system_clock::now();

		return {
			.identifier = identifier,
			.received = msg.received_time,
			.dispatch_start = dispatch_start_timestamp,
			.dispatch_end = dispatch_completion_timestamp,
			.result = std::move(result)
		};
	}

	auto Server::run() -> void
	{
		while (!shutdown)
		{
			if (auto msg = in_queue.pop_with_abort([this] { return shutdown.load(); }); msg.has_value())
			{
				try
				{
					auto const result = dispatch_message(std::move(*msg));
					postprocess_message(result);

					if (result.result.exit)
					{
						break;
					}
				}
				catch (...)
				{
					break;
				}
			}
		}
	}

	auto Server::request_shutdown() -> void
	{
		shutdown = true;
		in_queue.notify();
	}

	auto Server::postprocess_message(DispatchResult const& result) const -> void
	{
		if (metrics)
		{
			metrics(result.metrics());
		}
	}

	auto Server::send_notification_impl(lsp::RawMessage&& msg) const -> void
	{
		out_queue.push(std::move(msg));
	}

	auto Server::log_impl(LogOutputCallbackView callback) const -> void
	{
		if (logger)
		{
			logger(std::move(callback));
		}
	}
}
