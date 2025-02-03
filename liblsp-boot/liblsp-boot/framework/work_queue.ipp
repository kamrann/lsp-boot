
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <chrono>
#endif

export module lsp_boot.work_queue;

import lsp_boot.synced_queue;
import lsp_boot.lsp;

namespace lsp_boot
{
	export struct ReceivedMessage
	{
		lsp::RawMessage msg;
		std::chrono::system_clock::time_point received_time;
	};

	export using PendingInputQueue = SyncedQueue< ReceivedMessage >;
	export using OutputQueue = SyncedQueue< lsp::RawMessage >;
}
