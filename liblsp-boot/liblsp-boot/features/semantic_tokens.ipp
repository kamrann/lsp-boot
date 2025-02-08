
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <concepts>
#include <cstdint>
#include <array>
#include <vector>
#include <ranges>
#include <algorithm>
#endif

export module lsp_boot.semantic_tokens;

import lsp_boot.lsp;

namespace lsp_boot
{
	export template < std::ranges::range Modifiers >
		requires std::same_as< std::ranges::range_value_t< Modifiers >, lsp::SemanticTokenModifier >
	constexpr auto encode_semantic_token_modifiers(Modifiers&& modifiers)
	{
		return std::ranges::fold_left(modifiers, std::uint64_t{ 0 }, [](std::uint64_t accum, lsp::SemanticTokenModifier const mod) {
			return accum | (std::uint64_t{ 1 } << mod);
			});
	}

	export template < std::same_as< lsp::SemanticTokenModifier >... Modifiers >
		requires (sizeof...(Modifiers) > 0)
	constexpr auto encode_semantic_token_modifiers(Modifiers... modifiers)
	{
		return encode_semantic_token_modifiers(std::array{ modifiers... });
	}

	/**
	 * @tokens Range of tuples (line_index, character_offset, length, semantic_token_type, semantic_token_modifiers), with the character offset being relative to the start of the line.
	 * @return Tokens encoded as a vector of unsigned integers, as per LSP TokenFormat.relative (https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens).
	 */
	export auto generate_semantic_token_deltas(std::ranges::sized_range auto&& tokens) -> std::vector< unsigned int >
	{
		std::vector< unsigned int > tokens_data;
		tokens_data.reserve(std::ranges::size(tokens) * 5);

		unsigned int prev_line = 0;
		unsigned int prev_offset = 0;
		for (auto const& [line_index, char_offset, len, stt, mods] : tokens)
		{
			auto const line_delta = unsigned(line_index - prev_line);
			bool const is_new_line = line_delta > 0;
			unsigned const char_delta = is_new_line ? char_offset : (char_offset - prev_offset);
			tokens_data.append_range(std::array< unsigned int, 5 >{
				line_delta, // line delta
				char_delta, // char delta
				unsigned(len), // token length
				unsigned(stt), // token type index
				unsigned(mods), // modifiers mask
			});
			if (is_new_line)
			{
				prev_line = line_index;
			}
			prev_offset = char_offset;
		}

		return tokens_data;
	}
}
