
module;

#include <boost/json.hpp>

#if defined(lsp_boot_enable_import_std)
import std;
#else
#include <utility>
#include <optional>
#include <string_view>
#include <ostream>
#include <istream>
#include <memory>
#include <thread>
#endif

module lsp_boot.transport;

import ext_mod_wrap.boost.json;

using namespace std::string_view_literals;

namespace lsp_boot
{
	auto StreamConnection::send_message(MessageContent&& message) -> void
	{
		auto content = boost::json::serialize(message);
		out
			<< "Content-Length: "
			<< content.length()
			<< "\r\n\r\n"
			<< content
			<< std::flush;
	}

	auto StreamConnection::read_message_header() -> std::optional< MessageHeader >
	{
		auto const compare = [](char const* const buffer, std::string_view const sv) {
			return sv == std::string_view(buffer, sv.size());
			};

		MessageHeader header;

		// @note: apparently the std library will auto-normalize line endings, removing \r. 
		// binary mode would likely avoid this, but since it's text by default we can just look for \n instead.
		constexpr auto newline_char = '\n'; // '\r';

		auto const consume_newline = [&] {
			//char c1 = in.get();
			char c2 = in.get();
			return /*c1 == '\r' &&*/ c2 == '\n';
			};
		auto const consume_header_field_separator = [&] {
			char sep[2];
			in.read(sep, 2);
			return !in.fail() && compare(sep, ": "sv);
			};

		while (true)
		{
			if (in.peek() == newline_char)
			{
				return consume_newline() ? std::optional{ std::move(header) } : std::nullopt;
			}

			// @todo: rewrite this. can just use getline for each field.

			char content_prefix[8];
			in.read(content_prefix, 8);
			if (in.fail() || !compare(content_prefix, "Content-"sv))
			{
				return std::nullopt;
			}
			if (in.peek() == 'L')
			{
				char clength[6];
				in.read(clength, 6);
				if (in.fail() || !compare(clength, "Length"sv))
				{
					return std::nullopt;
				}
				if (!consume_header_field_separator())
				{
					return std::nullopt;
				}

				in >> header.content_length;
			}
			else if (in.peek() == 'T')
			{
				char ctype[4];
				in.read(ctype, 4);
				if (in.fail() || !compare(ctype, "Type"sv) || !header.content_type.empty())
				{
					return std::nullopt;
				}
				if (!consume_header_field_separator())
				{
					return std::nullopt;
				}

				while (in.peek() != newline_char)
				{
					header.content_type += in.get();
				}
			}
			else
			{
				return std::nullopt;
			}

			if (!consume_newline())
			{
				return std::nullopt;
			}
		}

		return header;
	}

	auto StreamConnection::read_message() -> std::optional< MessageContent >
	{
		return read_message_header().and_then([&](MessageHeader&& hdr) -> std::optional< MessageContent > {
			//lsp_assert(hdr.content_length > 0);
			auto content = std::make_unique< char[] >(hdr.content_length);
			in.read(content.get(), hdr.content_length);
			try
			{
				auto value = boost::json::parse(std::string_view{ content.get(), hdr.content_length });
				return std::optional{ value.as_object() };
			}
			catch (...)
			{
				return std::nullopt;
			}
			});
	}

	auto StreamConnection::process_output() -> void
	{
		while (true)
		{
			auto msg = out_queue.pop();
			send_message(std::move(msg));
		}
	}

	auto StreamConnection::listen() -> int
	{
		std::thread out_thread([this] { process_output(); });

		while (true) // @todo: listen for shutdown msg
		{
			if (auto msg = read_message(); msg.has_value())
			{
				in_queue.push(std::move(*msg));
			}
			else
			{
				err << "Something went wrong" << std::endl;
			}
		}
		return 0;
	}
}
