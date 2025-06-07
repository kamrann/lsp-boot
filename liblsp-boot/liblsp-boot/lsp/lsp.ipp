
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
#include <ranges>
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
		constexpr auto character = "character"sv;
		constexpr auto code = "code"sv;
		constexpr auto code_description = "codeDescription"sv;
		constexpr auto content_changes = "contentChanges"sv;
		constexpr auto contents = "contents"sv;
		constexpr auto data = "data"sv;
		constexpr auto diagnostics = "diagnostics"sv;
		constexpr auto end = "end"sv;
		constexpr auto href = "href"sv;
		constexpr auto id = "id"sv;
		constexpr auto kind = "kind"sv;
		constexpr auto label = "label"sv;
		constexpr auto line = "line"sv;
		constexpr auto location = "location"sv;
		constexpr auto message = "message"sv;
		constexpr auto method = "method"sv;
		constexpr auto name = "name"sv;
		constexpr auto params = "params"sv;
		constexpr auto position = "position"sv;
		constexpr auto range = "range"sv;
		constexpr auto related_information = "relatedInformation"sv;
		constexpr auto result = "result"sv;
		constexpr auto root_path = "rootPath"sv;
		constexpr auto selection_range = "selectionRange"sv;
		constexpr auto severity = "severity"sv;
		constexpr auto source = "source"sv;
		constexpr auto start = "start"sv;
		constexpr auto tags = "tags"sv;
		constexpr auto text = "text"sv;
		constexpr auto text_document = "textDocument"sv;
		constexpr auto token_modifiers = "tokenModifiers"sv;
		constexpr auto token_types = "tokenTypes"sv;
		constexpr auto uri = "uri"sv;
		constexpr auto value = "value"sv;
		constexpr auto workspace_folders = "workspaceFolders"sv;
	}

	export using DocumentURI = std::string;
	export using DocumentContent = std::string;

	export struct Location
	{
		std::uint32_t line = 0;
		std::uint32_t character = 0;

		auto operator<=> (Location const& rhs) const = default;

		static auto from_json(json::value const& js) -> Location
		{
			auto const& obj = js.as_object();
			auto const line = json::value_to< std::uint32_t >(obj.at(keys::line));
			auto const character = json::value_to< std::uint32_t >(obj.at(keys::character));
			return Location{
				.line = line,
				.character = character,
			};
		}

		static auto try_from_json(json::value const& js) -> std::optional< Location >
		{
			if (auto const obj = js.if_object(); obj != nullptr)
			{
				if (auto const line_attr = obj->if_contains(keys::line); line_attr != nullptr)
				{
					if (auto const line = json::try_value_to< std::uint32_t >(*line_attr))
					{
						if (auto const character_attr = obj->if_contains(keys::character); character_attr != nullptr)
						{
							if (auto const character = json::try_value_to< std::uint32_t >(*character_attr))
							{
								return Location{
									.line = *line,
									.character = *character,
								};
							}
						}
					}
				}
			}
			return std::nullopt;
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

		static auto from_json(json::value const& js) -> Range
		{
			auto const& obj = js.as_object();
			return Range{
				Location::from_json(obj.at(keys::start)),
				Location::from_json(obj.at(keys::end)),
			};
		}

		static auto try_from_json(json::value const& js) -> std::optional< Range >
		{
			if (auto const obj = js.if_object(); obj != nullptr)
			{
				if (auto const start_attr = obj->if_contains(keys::start); start_attr != nullptr)
				{
					if (auto const end_attr = obj->if_contains(keys::end); end_attr != nullptr)
					{
						if (auto start_loc = Location::try_from_json(*start_attr); start_loc.has_value())
						{
							if (auto end_loc = Location::try_from_json(*end_attr); end_loc.has_value())
							{
								return Range{ *start_loc, *end_loc };
							}
						}
					}
				}
			}

			return std::nullopt;
		}

		explicit operator json::value() const
		{
			return {
				{ keys::start, json::value(start) },
				{ keys::end, json::value(end) },
			};
		}
	};

	export struct DiagnosticSeverity
	{
		static constexpr auto error = 1;
		static constexpr auto warning = 2;
		static constexpr auto information = 3;
		static constexpr auto hint = 4;
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
		namespace_ = 0,
		type,
		class_,
		enum_,
		interface,
		struct_,
		type_parameter,
		parameter,
		variable,
		property,
		enum_member,
		event,
		function,
		method,
		macro,
		keyword,
		modifier,
		comment,
		string,
		number,
		regexp,
		operator_,
	};

	export constexpr auto semantic_token_types = std::to_array< std::string_view >({
		"namespace",
		"type",
		"class",
		"enum",
		"interface",
		"struct",
		"typeParameter",
		"parameter",
		"variable",
		"property",
		"enumMember",
		"event",
		"function",
		"method",
		"macro",
		"keyword",
		"modifier",
		"comment",
		"string",
		"number",
		"regexp",
		"operator",

		//"cppMacro",
		//"cppEnumerator",
		//"cppGlobalVariable",
		//"cppLocalVariable",
		//"cppParameter",
		//"cppType",
		//"cppRefType",
		//"cppValueType",
		//"cppFunction",
		//"cppMemberFunction",
		//"cppMemberField",
		//"cppStaticMemberFunction",
		//"cppStaticMemberField",
		//"cppProperty",
		//"cppEvent",
		//"cppClassTemplate",
		//"cppGenericType",
		//"cppFunctionTemplate",
		//"cppNamespace",
		//"cppLabel",
		//"cppUserDefinedLiteralRaw",
		//"cppUserDefinedLiteralNumber",
		//"cppUserDefinedLiteralString",
		//"cppOperator",
		//"cppMemberOperator",
		//"cppNewDelete",
		});

	export enum SemanticTokenModifier
	{
		declaration = 0,
		definition,
		readonly,
		static_,
		deprecated,
		abstract,
		async,
		modification,
		documentation,
		defaultLibrary,
	};

	export constexpr auto semantic_token_modifiers = std::to_array< std::string_view >({
		"declaration",
		"definition",
		"readonly",
		"static",
		"deprecated",
		"abstract",
		"async",
		"modification",
		"documentation",
		"defaultLibrary",
		});


	export using RawMessage = json::object;

	export auto message_params(RawMessage const& msg) -> boost::json::value const*
	{
		return msg.if_contains(keys::params);
	}

	// @note: short term approach is to just use the boost.json types directly, but adding wrapper to allow for static type differentiation.
	// code gen of proper types from the LSP is probably the way to go.

	export template < auto msg_id, FixedString msg_name >
	class JsonMessage
	{
	public:
		using RawMessage = json::object;

		static constexpr auto kind = msg_id;
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

		auto params() const -> boost::json::value const*
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
			hover,
			semantic_tokens_full,
			semantic_tokens_range,

			semantic_tokens_refresh,
		};

		// Client to Server
		using Initialize = JsonMessage< Kinds::initialize, "initialize" >;
		using Shutdown = JsonMessage< Kinds::shutdown, "shutdown" >;
		using DocumentSymbols = JsonMessage< Kinds::document_symbols, "textDocument/documentSymbol" >;
		using InlayHint = JsonMessage< Kinds::inlay_hint, "textDocument/inlayHint" >;
		using Hover = JsonMessage< Kinds::hover, "textDocument/hover" >;
		using SemanticTokensFull = JsonMessage< Kinds::semantic_tokens_full, "textDocument/semanticTokens/full" >;
		using SemanticTokensRange = JsonMessage< Kinds::semantic_tokens_range, "textDocument/semanticTokens/range" >;

		// Server to Client
		using SemanticTokensRefresh = JsonMessage< Kinds::semantic_tokens_range, "workspace/semanticTokens/refresh" >;
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
			did_change_configuration,

			// From server
			publish_diagnostics,
		};

		// From client
		using Initialized = JsonMessage< Kinds::initialized, "initialized" >;
		using Exit = JsonMessage< Kinds::exit, "exit" >;
		using DidOpenTextDocument = JsonMessage< Kinds::did_open_text_document, "textDocument/didOpen" >;
		using DidChangeTextDocument = JsonMessage< Kinds::did_change_text_document, "textDocument/didChange" >;
		using DidCloseTextDocument = JsonMessage< Kinds::did_close_text_document, "textDocument/didClose" >;
		using DidChangeConfiguration = JsonMessage< Kinds::did_change_configuration, "workspace/didChangeConfiguration" >;

		// From server
		using PublishDiagnostics = JsonMessage< Kinds::publish_diagnostics, "textDocument/publishDiagnostics" >;
	}

	export using Request = std::variant<
		requests::Initialize,
		requests::Shutdown,
		requests::DocumentSymbols,
		requests::InlayHint,
		requests::Hover,
		requests::SemanticTokensFull,
		requests::SemanticTokensRange
	>;

	export using Notification = std::variant<
		notifications::Initialized,
		notifications::Exit,
		notifications::DidOpenTextDocument,
		notifications::DidChangeTextDocument,
		notifications::DidCloseTextDocument,
		notifications::DidChangeConfiguration
	>;
}
