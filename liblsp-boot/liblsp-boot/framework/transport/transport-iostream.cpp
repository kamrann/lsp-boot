
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
#include <chrono>
#include <format>
#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <thread>
#endif
#endif

module lsp_boot.transport;

import lsp_boot.work_queue; // really clang?
import lsp_boot.ext_mod_wrap.boost.json;
import lsp_boot.utility;

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

		constexpr auto initial_newline_char = '\r';

		char failed_newline[3] = { 0 };

		auto const consume_newline = [&] {
			char c1 = in.get();
			char c2 = in.get();
			if (c1 == '\r' && c2 == '\n')
			{
				return true;
			}
			else
			{
				failed_newline[0] = c1;
				failed_newline[1] = c2;
				return false;
			}
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

			static constexpr auto expected_content_prefix_str = "Content-"sv;
			static constexpr auto expected_content_length_str = "Length"sv;
			static constexpr auto expected_content_type_str = "Type"sv;

			char content_prefix[8];
			in.read(content_prefix, 8);
			if (in.fail() || !compare(content_prefix, expected_content_prefix_str))
			{
				err << std::format("Error reading header, expected '{}'\nEncountered '{}'", expected_content_prefix_str, std::string_view{ content_prefix, 8 }) << std::endl;
				return std::nullopt;
			}
			if (in.peek() == 'L')
			{
				char clength[6];
				in.read(clength, 6);
				if (in.fail() || !compare(clength, expected_content_length_str))
				{
					err << std::format("Error reading header, expected '{}'\nEncountered '{}'", expected_content_length_str, std::string_view{ clength, 6 }) << std::endl;
					return std::nullopt;
				}
				if (!consume_header_field_separator())
				{
					err << std::format("Error reading expected header field separator") << std::endl;
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
					err << std::format("Error reading header, expected '{}'\nEncountered '{}'", expected_content_type_str, std::string_view{ ctype, 4 }) << std::endl;
					return std::nullopt;
				}
				if (!consume_header_field_separator())
				{
					err << std::format("Error reading expected header field separator") << std::endl;
					return std::nullopt;
				}

				while (in.peek() != initial_newline_char)
				{
					header.content_type += in.get();
				}
			}
			else
			{
				err << std::format("Error reading header; unexpected input: '{}'", in.peek()) << std::endl;
				return std::nullopt;
			}

			if (!consume_newline())
			{
				err << std::format("Error reading expected header newline; encountered [{}, {}]", int(failed_newline[0]), int(failed_newline[1])) << std::endl;
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
			boost::system::error_code ec;
			auto const header_content = std::string_view{ content.get(), hdr.content_length };
			auto value = boost::json::parse(header_content, ec);
			if (ec)
			{
				err << std::format("Failure parsing received JSON: {}\nInput was:\n{}", ec.message(), header_content) << std::endl;
				return std::nullopt;
			}
			return ReceivedMessage{
				.msg{ value.as_object() },
				.received_time = timestamp,
			};
			});
	}

#if not defined(LSP_BOOT_DISABLE_THREADS)
	auto StreamConnection::process_output() -> void
	{
		while (!shutdown)
		{
			if (auto msg = out_queue.pop_with_abort([this] { return shutdown.load(); }); msg.has_value())
			{
				send_message(std::move(*msg));
			}
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
		out_queue.notify();
		return in.good() || in.eof() ? 0 : -1;
	}
#endif

	auto StreamConnection::update_outgoing() -> void
	{
		while (true)
		{
			if (auto msg = out_queue.try_pop(); msg.has_value())
			{
				send_message(std::move(*msg));
			}
			else
			{
				break;
			}
		}
	}

	//auto StreamConnection::update_non_blocking() -> bool
	//{
	//	update_outgoing();

	//	if (not in.good())
	//	{
	//		return false;
	//	}

	//	apparently a pain...
	//}

	auto StreamConnection::update() -> bool
	{
		// Dispatch anything waiting in the output queue
		update_outgoing();

		// Read any input
		if (not in.good())
		{
			return false;
		}

		if (auto msg = read_message(); msg.has_value())
		{
			in_queue.push(std::move(*msg));
		}
		else
		{
			err << std::format("{}. Exiting...",
				in.eof() ? "StreamConnection reached end of input"sv : "StreamConnection message read error"sv) << std::endl;
			return false;
		}
		return true;
	}
}
