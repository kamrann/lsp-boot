
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <cstddef>
#include <optional>
#include <string>
#include <ostream>
#include <istream>
#endif

export module lsp_boot.transport;

import lsp_boot.work_queue;

import ext_mod_wrap.boost.json;

namespace lsp_boot
{
	struct MessageHeader
	{
		std::size_t content_length;
		std::string content_type;
	};

	using MessageContent = boost::json::object;

	export class StreamConnection
	{
	public:
		StreamConnection(
			PendingInputQueue& pending_input_queue,
			OutputQueue& output_queue,
			std::istream& input,
			std::ostream& output,
			std::ostream& error)
			: in_queue{ pending_input_queue }, out_queue{ output_queue }, in{ input }, out{ output }, err{ error }
		{
		}

		auto listen() -> int;

	private:
		auto read_message_header() -> std::optional< MessageHeader >;
		auto read_message() -> std::optional< MessageContent >;
		auto send_message(MessageContent&& message) -> void;

		auto process_output() -> void;

	private:
		PendingInputQueue& in_queue;
		OutputQueue& out_queue;
		std::istream& in;
		std::ostream& out;
		std::ostream& err;
	};
}
