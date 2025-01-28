
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <optional>
#include <string_view>
#endif

module lsp_boot.server;

using namespace std::string_view_literals;

namespace lsp_boot
{
	using namespace lsp;

	auto Server::dispatch_request(std::string_view const method, RawMessage&& msg) -> InternalMessageResult
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

	auto Server::dispatch_notification(std::string_view const method, RawMessage&& msg) -> InternalMessageResult
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

	auto Server::dispatch_message(RawMessage&& msg) -> InternalMessageResult
	{
		if (auto id_it = msg.find("id"sv); id_it != msg.end())
		{
			if (auto method_it = msg.find("method"sv); method_it != msg.end())
			{
				return dispatch_request(method_it->value().as_string(), std::move(msg));
			}
		}
		else if (auto method_it = msg.find("method"sv); method_it != msg.end())
		{
			return dispatch_notification(method_it->value().as_string(), std::move(msg));
		}

		return {};
	}

	auto Server::run() -> void
	{
		while (true)
		{
			auto msg = in_queue.pop();
			if (auto result = dispatch_message(std::move(msg)); result.exit)
			{
				break;
			}
			
			impl.pump(); // @todo: placeholder, probably don't want to be doing this on the dispatching thread.
		}
	}
}
