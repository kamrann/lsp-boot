
module;

// workaround: MSVC modules template specialization and boost::system::error_code
#if defined(_MSC_VER) && !defined(__clang__)
#include <boost/json/value_to.hpp>
#endif

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <cstdint>
#include <utility>
#include <optional>
#include <variant>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#endif

export module lsp_boot.lsp;

import lsp_boot.utility;
import lsp_boot.ext_mod_wrap.boost.json;

namespace lsp_boot::lsp
{
	namespace json = boost::json;

	export namespace keys
	{
		using namespace std::string_view_literals;

		constexpr auto capabilities = "capabilities"sv;
		constexpr auto params = "params"sv;
		constexpr auto id = "id"sv;
		constexpr auto result = "result"sv;
		constexpr auto method = "method"sv;
		constexpr auto text_document = "textDocument"sv;
		constexpr auto uri = "uri"sv;
		constexpr auto range = "range"sv;
		constexpr auto selection_range = "selectionRange"sv;
		constexpr auto position = "position"sv;
		constexpr auto start = "start"sv;
		constexpr auto end = "end"sv;
		constexpr auto line = "line"sv;
		constexpr auto character = "character"sv;
		constexpr auto name = "name"sv;
		constexpr auto kind = "kind"sv;
		constexpr auto text = "text"sv;
		constexpr auto content_changes = "contentChanges"sv;
		constexpr auto message = "message"sv;
		constexpr auto data = "data"sv;
		constexpr auto label = "label"sv;
		constexpr auto diagnostics = "diagnostics"sv;
		constexpr auto token_types = "tokenTypes"sv;
		constexpr auto token_modifiers = "tokenModifiers"sv;
	}

	export using DocumentURI = std::string;
	export using DocumentContent = std::string;

	export struct Location
	{
		std::uint32_t line = 0;
		std::uint32_t character = 0;

		static auto from_json(json::value const& js) -> std::optional< Location >
		{
			auto const& obj = js.as_object();
			auto const line = json::value_to< std::uint32_t >(obj.at(keys::line));
			auto const character = json::value_to< std::uint32_t >(obj.at(keys::character));
			return Location{
				.line = line,
				.character = character,
			};
		}

		explicit operator json::value() const
		{
			return {
				{ keys::line, json::value(line) },
				{ keys::character, json::value(character) },
			};
		}
	};

	export struct Range
	{
		Location start;
		Location end;

		static auto from_json(json::value const& js) -> std::optional< Range >
		{
			auto const& obj = js.as_object();
			return Location::from_json(obj.at(keys::start)).and_then([&](auto&& start) {
				return Location::from_json(obj.at(keys::end)).transform([&](auto&& end) {
					return Range{ start, end };
					});
				});
		}

		explicit operator json::value() const
		{
			return {
				{ keys::start, json::value(start) },
				{ keys::end, json::value(end) },
			};
		}
	};

	export enum SymbolKind
	{
		//File = 1,
		//Module = 2,
		//Namespace = 3,
		//Package = 4,
		sk_class = 5,
		//Method = 6,
		//Property = 7,
		//Field = 8,
		//Constructor = 9,
		//Enum = 10,
		//Interface = 11,
		sk_function = 12,
		sk_variable = 13,
		sk_constant = 14,
		sk_string = 15,
		sk_number = 16,
		sk_boolean = 17,
		sk_array = 18,
		//Object = 19,
		//Key = 20,
		//Null = 21,
		//EnumMember = 22,
		//Struct = 23,
		//Event = 24,
		//Operator = 25,
		//TypeParameter = 26,
	};

	export enum SemanticTokenType
	{
		keyword = 0,
		type,
		enum_,
		enum_member,
		parameter,
		variable,
		function,
		string,
		regexp,
		number,
		comment,
	};

	export constexpr auto semantic_token_types = std::to_array< std::string_view >({
		"keyword",
		"type",
		"enum",
		"enumMember",
		"parameter",
		"variable",
		"function",
		"string",
		"regexp",
		"number",
		"comment",
		});

	/**
	 * @tokens Range of tuples (line_index, character_offset, length, semantic_token_type), with the character offset being relative to the start of the line.
	 */
	export auto generate_semantic_token_deltas(std::ranges::sized_range auto&& tokens) -> std::vector< unsigned int >
	{
		std::vector< unsigned int > tokens_data;
		tokens_data.reserve(std::ranges::size(tokens) * 5);

		unsigned int prev_line = 0;
		unsigned int prev_offset = 0;
		for (auto const& [line_index, char_offset, len, stt] : tokens)
		{
			auto const line_delta = unsigned(line_index - prev_line);
			bool const is_new_line = line_delta > 0;
			unsigned const char_delta = is_new_line ? char_offset : (char_offset - prev_offset);
			tokens_data.append_range(std::array< unsigned int, 5 >{
				line_delta, // line delta
				char_delta, // char delta
				unsigned(len), // token length
				unsigned(stt), // token type index
				0 // modifiers mask
			});
			if (is_new_line)
			{
				prev_line = line_index;
			}
			prev_offset = char_offset;
		}

		return tokens_data;
	}


	export using RawMessage = json::object;

	export auto message_params(RawMessage const& msg) -> auto const&
	{
		return msg.at(keys::params);
	}

	// @note: short term approach is to just use the boost.json types directly, but adding wrapper to allow for static type differentiation.
	// code gen of proper types from the LSP is probably the way to go.

	export template < auto msg_id, FixedString msg_name >
	class JsonMessage
	{
	public:
		using RawMessage = json::object;

		static constexpr auto raw_name = msg_name;
		static constexpr auto name = std::string_view{ raw_name };

		JsonMessage(RawMessage&& raw) : js{ std::move(raw) }
		{
		}

		auto operator* () const& -> RawMessage const&
		{
			return js;
		}
		auto operator* () && -> RawMessage&&
		{
			return std::move(js);
		}
		auto operator-> () const -> RawMessage const*
		{
			return &js;
		}
		auto operator-> () -> RawMessage*
		{
			return &js;
		}

		auto params() const -> auto const&
		{
			return message_params(js);
		}

	private:
		RawMessage js;
	};


	export namespace requests
	{
		enum class Kinds
		{
			initialize,
			shutdown,
			document_symbols,
			inlay_hint,
			semantic_tokens,
		};

		using Initialize = JsonMessage< Kinds::initialize, "initialize" >;
		using Shutdown = JsonMessage< Kinds::shutdown, "shutdown" >;
		using DocumentSymbols = JsonMessage< Kinds::document_symbols, "textDocument/documentSymbol" >;
		using InlayHint = JsonMessage< Kinds::inlay_hint, "textDocument/inlayHint" >;
		using SemanticTokens = JsonMessage< Kinds::semantic_tokens, "textDocument/semanticTokens/full" >;
	}

	export namespace notifications
	{
		enum class Kinds
		{
			// From client
			initialized,
			exit,
			did_open_text_document,
			did_change_text_document,
			did_close_text_document,

			// From server
			publish_diagnostics,
		};

		// From client
		using Initialized = JsonMessage< Kinds::initialized, "initialized" >;
		using Exit = JsonMessage< Kinds::exit, "exit" >;
		using DidOpenTextDocument = JsonMessage< Kinds::did_open_text_document, "textDocument/didOpen" >;
		using DidChangeTextDocument = JsonMessage< Kinds::did_change_text_document, "textDocument/didChange" >;
		using DidCloseTextDocument = JsonMessage< Kinds::did_close_text_document, "textDocument/didClose" >;

		// From server
		using PublishDiagnostics = JsonMessage< Kinds::publish_diagnostics, "textDocument/publishDiagnostics" >;
	}

	export using Request = std::variant<
		requests::Initialize,
		requests::Shutdown,
		requests::DocumentSymbols,
		requests::InlayHint,
		requests::SemanticTokens
	>;

	export using Notification = std::variant<
		notifications::Initialized,
		notifications::Exit,
		notifications::DidOpenTextDocument,
		notifications::DidChangeTextDocument,
		notifications::DidCloseTextDocument
	>;
}
