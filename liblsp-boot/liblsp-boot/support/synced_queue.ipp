
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <queue>
#include <optional>
#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <mutex>
#include <condition_variable>
#endif
#endif

export module lsp_boot.synced_queue;

namespace lsp_boot
{
#if not defined(LSP_BOOT_DISABLE_THREADS)
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

		auto pop_with_abort(auto should_abort) -> std::optional< T >
		{
			auto lock = std::unique_lock{ mtx };
			cvar.wait(lock, [this, should_abort] {
				return !queue.empty() || should_abort();
				});
			if (!queue.empty())
			{
				auto value = std::move(queue.front());
				queue.pop();
				return value;
			}
			else
			{
				return std::nullopt;
			}
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

		auto notify() -> void
		{
			cvar.notify_all();
		}

	private:
		std::queue< T > queue;
		mutable std::mutex mtx;
		std::condition_variable cvar;
	};
#else
	export template < typename T >
	class SyncedQueue
	{
	public:
		auto push(T&& value) -> void
		{
			queue.push(std::move(value));
		}

		auto try_pop() -> std::optional< T >
		{
			if (!queue.empty())
			{
				auto value = std::move(queue.front());
				queue.pop();
				return value;
			}
			return std::nullopt;
		}

		auto notify() -> void
		{}

	private:
		std::queue< T > queue;
	};
#endif
}
