
module;

#include <boost/json/value_to.hpp>

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <optional>
#include <string_view>
#include <format>
#endif

module lsp_boot.server;

using namespace std::string_view_literals;

namespace lsp_boot
{
	using namespace lsp;

	auto Server::dispatch_request(std::string_view const method, lsp::RawMessage&& msg) -> InternalMessageResult
	{
		auto request_id = msg.at("id"sv);

		auto result = [&]() -> std::optional< RequestResult > {
			if (method == "initialize"sv)
			{
				return handle_request(requests::Initialize(std::move(msg)));
			}
			else if (method == "shutdown"sv)
			{
				return handle_request(requests::Shutdown(std::move(msg)));
			}
			//else if (method == "textDocument/documentHighlight"sv)
			//{
			//	return handle_document_highlight(msg.at("params"));
			//}
			else if (method == "textDocument/inlayHint"sv)
			{
				return handle_request(requests::InlayHint(std::move(msg)));
			}
			else if (method == "textDocument/semanticTokens/full"sv)
			{
				return handle_request(requests::SemanticTokens(std::move(msg)));
			}
			else if (method == "textDocument/documentSymbol"sv)
			{
				return handle_request(requests::DocumentSymbols(std::move(msg)));
			}
			else
			{
				return std::nullopt;
			}
			}();

		// @todo: currently server implementation is able to send notifications itself, but responses are expected to be returned synchronously and handled here.
		// feels inconsistent, and likely will want to allow an implementation to generate an async response - though could do that with similar approach using a future.
		auto temp_todo_err = std::move(result).transform([&](RequestResult&& result) {
			auto response = boost::json::object{
				{ "id"sv, std::move(request_id) },
				{ "result"sv, std::move(result) },
			};
			out_queue.push(std::move(response));
			return true;
			}).value_or(false);

		return {};
	}

	auto Server::dispatch_notification(std::string_view const method, lsp::RawMessage&& msg) -> InternalMessageResult
	{
		if (method == "initialized"sv)
		{
			// client initialization completed
			handle_notification(notifications::Initialized(std::move(msg)));
		}
		else if (method == "exit"sv)
		{
			return {
				.exit = true,
			};
		}
		else if (method == "textDocument/didOpen"sv)
		{
			handle_notification(notifications::DidOpenTextDocument(std::move(msg)));
		}
		else if (method == "textDocument/didChange"sv)
		{
			handle_notification(notifications::DidChangeTextDocument(std::move(msg)));
		}

		return {};
	}

	auto Server::dispatch_message(ReceivedMessage&& msg) -> DispatchResult
	{
		auto const dispatch_start_timestamp = std::chrono::system_clock::now();

		auto&& json_msg = msg.msg;
		auto const id_it = json_msg.find("id"sv);
		auto const method_it = json_msg.find("method"sv);

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
			.identifier = std::format("{}:{}",
				id_it != json_msg.end() ? value_to< std::string >(id_it->value()) : "?",
				method_it != json_msg.end() ? method_it->value().as_string() : "?"),
			.received = msg.received_time,
			.dispatch_start = dispatch_start_timestamp,
			.dispatch_end = dispatch_completion_timestamp,
			.result = std::move(result)
		};
	}

	auto Server::run() -> void
	{
		while (true)
		{
			auto msg = in_queue.pop();
			auto const result = dispatch_message(std::move(msg));
			postprocess_message(result);
			
			if (result.result.exit)
			{
				break;
			}
			
			impl.pump(); // @todo: placeholder, probably don't want to be doing this on the dispatching thread.
		}
	}

	auto Server::postprocess_message(DispatchResult const& result) const -> void
	{
		if (metrics)
		{
			metrics(result.metrics());
		}
	}
}
