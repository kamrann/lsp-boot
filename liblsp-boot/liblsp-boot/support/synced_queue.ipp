
module;

#if defined(lsp_boot_enable_import_std)
import std;
#else
#include <queue>
#include <mutex>
#include <condition_variable>
#endif

export module lsp_boot.synced_queue;

namespace lsp_boot
{
	export template < typename T >
	class SyncedQueue
	{
	public:
		auto push(T&& value) -> void
		{
			{
				auto lock = std::scoped_lock{ mtx };
				queue.push(std::move(value));
			}
			cvar.notify_one();
		}

		auto pop() -> T
		{
			auto lock = std::unique_lock{ mtx };
			cvar.wait(lock, [this] {
				return !queue.empty();
				});
			auto value = std::move(queue.front());
			queue.pop();
			return value;
		}

	private:
		std::queue< T > queue;
		mutable std::mutex mtx;
		std::condition_variable cvar;
	};
}
