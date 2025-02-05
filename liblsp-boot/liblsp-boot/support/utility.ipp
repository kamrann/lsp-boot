
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <cstddef>
#include <concepts>
#include <utility>
#include <array>
#include <string_view>
#include <thread>
#endif

#include <version>

export module lsp_boot.utility;

namespace lsp_boot
{
    export template< std::size_t len >
    struct FixedString
    {
        std::array< char, len > buffer{};
        
        template < std::size_t arg_len >
        constexpr FixedString(char const (&str)[arg_len])
        {
            for (std::size_t i = 0; i < std::min(arg_len, len); ++i)
            {
                buffer[i] = str[i];
            }
        }
        
        constexpr operator std::string_view() const
        {
            return std::string_view(buffer);
        }

        //// not mandatory anymore
        //auto operator<=>(const FixedString&) const = default;
    };

    // @NOTE: Use of -1 is assuming always used with string literals.
    // Passing a non-null terminated array of characters will result in truncation of the last character.
    template< std::size_t len >
    FixedString(char const (&)[len]) -> FixedString< len - 1 >;


#if defined(__cpp_lib_jthread)
    export using Thread = std::jthread;
#else
    export struct Thread
    {
        std::thread t;

        Thread(std::invocable auto&& f, auto&&... args) : t(std::forward< decltype(f) >(f), std::forward< decltype(args) >(args)...)
        {
        }

        ~Thread()
        {
            t.join();
        }
    };
#endif
}
