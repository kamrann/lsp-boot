
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <queue>
#include <optional>
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

		auto try_pop() -> std::optional< T >
		{
			auto lock = std::scoped_lock{ mtx };
			if (!queue.empty())
			{
				auto value = std::move(queue.front());
				queue.pop();
				return value;
			}
			return std::nullopt;
		}

	private:
		std::queue< T > queue;
		mutable std::mutex mtx;
		std::condition_variable cvar;
	};
}
