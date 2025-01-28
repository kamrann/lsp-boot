
export module lsp_boot.work_queue;

import lsp_boot.synced_queue;
import lsp_boot.lsp;

namespace lsp_boot
{
	export using PendingInputQueue = SyncedQueue< lsp::RawMessage >;
	export using OutputQueue = SyncedQueue< lsp::RawMessage >;
}
