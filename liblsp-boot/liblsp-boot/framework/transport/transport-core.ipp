
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <cstddef>
#include <string>
#endif

export module lsp_boot.transport:core;

import ext_mod_wrap.boost.json;

namespace lsp_boot
{
	struct MessageHeader
	{
		std::size_t content_length;
		std::string content_type;
	};

	using MessageContent = boost::json::object;
}
