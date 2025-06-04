
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <optional>
#include <ostream>
#include <istream>
#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <atomic>
#endif
#endif

export module lsp_boot.transport:iostream;

import :core;

import lsp_boot.work_queue;
import lsp_boot.ext_mod_wrap.boost.json;

namespace lsp_boot
{
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

#if not defined(LSP_BOOT_DISABLE_THREADS)
		auto listen() -> int;
#endif
	// single-threaded
	auto update() -> bool;

	private:
		auto read_message_header() -> std::optional< MessageHeader >;
		auto read_message() -> std::optional< ReceivedMessage >;
		auto send_message(MessageContent&& message) -> void;

#if not defined(LSP_BOOT_DISABLE_THREADS)
		auto process_output() -> void;
#endif

	private:
		PendingInputQueue& in_queue;
		OutputQueue& out_queue;
		std::istream& in;
		std::ostream& out;
		std::ostream& err;
#if not defined(LSP_BOOT_DISABLE_THREADS)
		std::atomic< bool > shutdown = false;
#endif
	};
}
