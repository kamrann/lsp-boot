
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
#endif

export module lsp_boot.lsp;

import lsp_boot.ext_mod_wrap.boost.json;

namespace lsp_boot::lsp
{
	namespace json = boost::json;

	export using DocumentURI = std::string;
	export using DocumentContent = std::string;

	export struct Location
	{
		std::uint32_t line = 0;
		std::uint32_t character = 0;

		static auto from_json(json::value const& js) -> std::optional< Location >
		{
			auto const& obj = js.as_object();
			auto const line = json::value_to< std::uint32_t >(obj.at("line"));
			auto const character = json::value_to< std::uint32_t >(obj.at("character"));
			return Location{
				.line = line,
				.character = character,
			};
		}

		explicit operator json::value() const
		{
			return {
				{ "line", json::value(line) },
				{ "character", json::value(character) },
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
			return Location::from_json(obj.at("start")).and_then([&](auto&& start) {
				return Location::from_json(obj.at("end")).transform([&](auto&& end) {
					return Range{ start, end };
					});
				});
		}

		explicit operator json::value() const
		{
			return {
				{ "start", json::value(start) },
				{ "end", json::value(end) },
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
		parameter,
		variable,
		function,
		string,
		number,
	};

	export constexpr auto semantic_token_types = std::to_array< std::string_view >({
		"keyword",
		"type",
		"parameter",
		"variable",
		"function",
		"string",
		"number",
		});

	export using RawMessage = json::object;

	// @note: short term approach is to just use the boost.json types directly, but adding wrapper to allow for static type differentiation.
	// code gen of proper types from the LSP is probably the way to go.

	export template < auto MsgId >
	class JsonMessage
	{
	public:
		using RawMessage = json::object;

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

		using Initialize = JsonMessage< Kinds::initialize >;
		using Shutdown = JsonMessage< Kinds::shutdown >;
		using DocumentSymbols = JsonMessage< Kinds::document_symbols >;
		using InlayHint = JsonMessage< Kinds::inlay_hint >;
		using SemanticTokens = JsonMessage< Kinds::semantic_tokens >;
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
		using Initialized = JsonMessage< Kinds::initialized >;
		using Exit = JsonMessage< Kinds::exit >;
		using DidOpenTextDocument = JsonMessage< Kinds::did_open_text_document >;
		using DidChangeTextDocument = JsonMessage< Kinds::did_change_text_document >;
		using DidCloseTextDocument = JsonMessage< Kinds::did_close_text_document >;

		// From server
		using PublishDiagnostics = JsonMessage< Kinds::publish_diagnostics >;
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
