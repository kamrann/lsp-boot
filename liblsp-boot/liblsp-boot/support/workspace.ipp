
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <optional>
#include <string>
#include <string_view>
#include <functional>
#endif

export module lsp_boot.workspace;

namespace lsp_boot
{
    export struct WorkspaceFolder
	{
		std::string name;
		std::string uri;

		std::function< std::optional< std::string >(std::string_view) > uri_to_path_fn;
		std::function< std::optional< std::string >(std::string_view) > path_to_uri_fn;

		auto uri_to_path(std::string_view const uri) const
		{
			return uri_to_path_fn ? uri_to_path_fn(uri) : std::nullopt;
		}

		auto path_to_uri(std::string_view const p) const
		{
			return path_to_uri_fn ? path_to_uri_fn(p) : std::nullopt;
		}

		auto root_path() const
		{
			return uri_to_path(uri);
		}
	};
}
