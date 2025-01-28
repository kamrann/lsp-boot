
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

	auto Server::dispatch_request(std::string_view const method, RawMessage&& msg) -> void
	{
		auto request_id = msg.at("id"sv);

		auto result = [&]() -> std::optional< boost::json::value > {
			if (method == "initialize"sv)
			{
				return handle(requests::Initialize(std::move(msg)));
			}
			// @todo: "shutdown"
			//else if (method == "textDocument/documentHighlight"sv)
			//{
			//	return handle_document_highlight(msg.at("params"));
			//}
			else if (method == "textDocument/semanticTokens/full"sv)
			{
				return handle(requests::SemanticTokens(std::move(msg)));
			}
			else if (method == "textDocument/documentSymbol"sv)
			{
				return handle(requests::DocumentSymbols(std::move(msg)));
			}
			else
			{
				return std::nullopt;
			}
			}();

		auto temp_todo_err = result.transform([&](auto&& result) {
			auto response = boost::json::object{
				{ "id"sv, std::move(request_id) },
				{ "result"sv, std::move(result) },
			};
			out_queue.push(std::move(response));
			return true;
			}).value_or(false);
	}

	auto Server::dispatch_notification(std::string_view const method, RawMessage&& msg) -> void
	{
		if (method == "initialized"sv)
		{
			// client initialization completed
		}
		else if (method == "textDocument/didOpen"sv)
		{
			handle(notifications::DidOpenTextDocument(std::move(msg))); // @todo: need to check protocol. possible that we only ever want params beyond here. msg.at("params"));
		}
		else if (method == "textDocument/didChange"sv)
		{
			handle(notifications::DidChangeTextDocument(std::move(msg)));
		}
	}

	auto Server::dispatch_message(RawMessage&& msg) -> void
	{
		if (auto id_it = msg.find("id"sv); id_it != msg.end())
		{
			if (auto method_it = msg.find("method"sv); method_it != msg.end())
			{
				dispatch_request(method_it->value().as_string(), std::move(msg));
			}
		}
		else if (auto method_it = msg.find("method"sv); method_it != msg.end())
		{
			dispatch_notification(method_it->value().as_string(), std::move(msg));
		}
	}

	auto Server::run() -> void
	{
		while (true)
		{
			auto msg = in_queue.pop();
			dispatch_message(std::move(msg));
			impl_pump();
		}
	}
}
