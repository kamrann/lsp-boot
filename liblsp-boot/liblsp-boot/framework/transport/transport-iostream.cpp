
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <optional>
#include <string_view>
#include <ostream>
#include <istream>
#include <memory>
#include <thread>
#include <chrono>
#include <format>
#include <thread>
#endif
#include <version>

module lsp_boot.transport;

import lsp_boot.work_queue; // really clang?
import lsp_boot.ext_mod_wrap.boost.json;

using namespace std::string_view_literals;

namespace lsp_boot
{
#if defined(__cpp_lib_jthread)
	using Thread = std::jthread;
#else
	using Thread = std::thread;
#endif

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

		constexpr auto initial_newline_char = '\r';

		auto const consume_newline = [&] {
			char c1 = in.get();
			char c2 = in.get();
			return c1 == '\r' && c2 == '\n';
			};
		auto const consume_header_field_separator = [&] {
			char sep[2];
			in.read(sep, 2);
			return !in.fail() && compare(sep, ": "sv);
			};

		while (true)
		{
			if (in.peek() == initial_newline_char)
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

				while (in.peek() != initial_newline_char)
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

	auto StreamConnection::read_message() -> std::optional< ReceivedMessage >
	{
		return read_message_header().and_then([&](MessageHeader&& hdr) -> std::optional< ReceivedMessage > {
			//lsp_assert(hdr.content_length > 0);
			auto content = std::make_unique< char[] >(hdr.content_length);
			in.read(content.get(), hdr.content_length);
			auto timestamp = std::chrono::system_clock::now();
			try
			{
				auto value = boost::json::parse(std::string_view{ content.get(), hdr.content_length });
				return ReceivedMessage{
					.msg{ value.as_object() },
					.received_time = timestamp,
				};
			}
			catch (...)
			{
				return std::nullopt;
			}
			});
	}

	auto StreamConnection::process_output() -> void
	{
		while (!shutdown)
		{
			auto msg = out_queue.pop();
			send_message(std::move(msg));
		}
	}

	auto StreamConnection::listen() -> int
	{
		Thread out_thread([this] {
			process_output();

			err << "StreamConnection output processor shutting down..." << std::endl;
			});

		while (in.good())
		{
			if (auto msg = read_message(); msg.has_value())
			{
				in_queue.push(std::move(*msg));
			}
			else
			{
				err << std::format("{}. Exiting...",
					in.eof() ? "StreamConnection reached end of input"sv : "StreamConnection message read error"sv) << std::endl;
				break;
			}
		}
		err << "StreamConnection shutting down..." << std::endl;
		shutdown = true;
		return in.good() || in.eof() ? 0 : -1;
	}
}
