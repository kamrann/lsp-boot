
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <cstddef>
#include <concepts>
#include <utility>
#include <array>
#include <string_view>
#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <thread>
#endif
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


    export struct LineRange
    {
        static auto from_start_and_count(std::size_t const start, std::size_t const count)
        {
            return LineRange{ start, start + count };
        }

        static auto from_start_and_end_exclusive(std::size_t const start, std::size_t const end)
        {
            return LineRange{ start, end };
        }

        static auto from_start_and_end_inclusive(std::size_t const start, std::size_t const end)
        {
            return LineRange{ start, end + 1 };
        }

        auto start() const
        {
            return start_;
        }

        auto end() const
        {
            return end_;
        }

        auto size() const
        {
            return end_ - start_;
        }

    private:
        LineRange(std::size_t const start_index, std::size_t const end_index) : start_{ start_index }, end_{ end_index }
        {
        }

        std::size_t start_;
        std::size_t end_; // exclusive
    };


#if not defined(LSP_BOOT_DISABLE_THREADS)
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
#endif
}
