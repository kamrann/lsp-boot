
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <type_traits>
#include <utility>
#include <string>
#include <string_view>
#include <functional>
#include <variant>
#include <expected>
#include <span>
#include <queue>
#include <iterator>
#include <memory>
#include <chrono>
#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <atomic>
#endif
#endif

export module lsp_boot.server;

import lsp_boot.lsp;
import lsp_boot.work_queue;
import lsp_boot.utility;

import lsp_boot.ext_mod_wrap.boost.json;

namespace lsp_boot
{
	//template < notifications::Kinds kind >
	//class NotificationResult
	//{
	//	
	//};
	//
	//template < requests::Kinds kind >
	//class RequestResult
	//{
	//	boost::json::value response_js;
	//};
	//
	//template < notifications::Kinds kind >
	//using NotificationHandler = std::function< NotificationResult< kind >(JsonMessage< kind >&&) >;
	//
	//template < requests::Kinds kind >
	//using RequestHandler = std::function< RequestResult< kind >(JsonMessage< kind >&&) >;


	//using ServerImplementation = std::variant<
	//	NotificationHandler< notifications::DidOpenTextDocument >,
	//	NotificationHandler< notifications::DidChangeTextDocument >,
	//	NotificationHandler< notifications::DidCloseTextDocument >
	//>;

	export using ServerInternalTask = std::function< void() >;

	export struct MessageMetrics
	{
		std::string identifier;
		std::chrono::microseconds response;
		std::chrono::microseconds dispatch;
	};

	using MetricsSink = std::function< void(MessageMetrics const&) >;

	using LogOutputIter = std::ostreambuf_iterator< char >;
	struct LogOutputContext
	{
		LogOutputContext(std::ostream& s) : stream{ s }
		{
		}

		auto iter() const
		{
			return LogOutputIter{ stream };
		}

		auto write(std::ranges::contiguous_range auto&& rg) const
		{
			auto span = std::span{ std::forward< decltype(rg) >(rg) };
			stream.write(span.data(), span.size());
			return *this;
		}

		auto write(std::ranges::range auto&& rg) const
		{
			std::ranges::copy(std::forward< decltype(rg) >(rg), iter());
			return *this;
		}

		std::ostream& stream;
	};
	export using LogOutputCallbackView = std::function< LogOutputContext(LogOutputContext) >&&; // Ideally would be function_view
	using LoggingSink = std::function< void(LogOutputCallbackView) >;

	export class ServerImplAPI
	{
	public:
		/**
		 * Send an LSP notification to the client.
		 */
		auto send_notification(lsp::RawMessage&& msg) const -> void
		{
			send_notification_impl(std::move(msg));
		}

		/**
		 * Queues a task to be run before any new incoming message is processed, but after dispatching outgoing messages.
		 */
		auto queue_internal_task(ServerInternalTask&& task) -> void
		{
			queue_internal_task_impl(std::move(task));
		}

		auto get_status() const -> boost::json::object
		{
			return get_status_impl();
		}

		/**
		 * Server side logger.
		 */
		template < std::invocable< LogOutputContext > Callback >
		auto log(Callback&& callback) const -> void
		{
			static_assert(std::convertible_to< std::invoke_result_t< Callback, LogOutputContext >, LogOutputContext >);
			log_impl(std::forward< Callback >(callback));
		}

		/**
		 * Server side logger simplified interface taking pre-evaluated arguments.
		 */
		template < typename... Args >
		auto log(std::basic_format_string< char, std::type_identity_t< Args >... > fmt_str, Args&&... args) const -> void
		{
			// @todo: currently this is assuming the logging sink is synchronous...
			log_impl([&](LogOutputContext out) {
				std::format_to(out.iter(), fmt_str, std::forward< Args >(args)...);
				return out;
				});
		}

	private:
		virtual auto send_notification_impl(lsp::RawMessage&&) const -> void = 0;
		virtual auto queue_internal_task_impl(ServerInternalTask&&) -> void = 0;
		virtual auto get_status_impl() const -> boost::json::object = 0;
		virtual auto log_impl(LogOutputCallbackView) const -> void = 0;
	};

	export class Server : private ServerImplAPI
	{
	public:
		struct RequestSuccessResult
		{
			boost::json::value json;
		};
		struct NotificationSuccessResult
		{
		};
		using ResponseError = boost::json::object;

		using RequestResult = std::expected< RequestSuccessResult, ResponseError >;
		using NotificationResult = std::expected< NotificationSuccessResult, ResponseError >;
		
		using ExternalPump = std::function< void() >;
		using ServerPump = std::function< void() >;

		struct ServerImplementation
		{
			std::shared_ptr< void const > type_erased;
			std::function< RequestResult(lsp::Request&&) > handle_request;
			std::function< NotificationResult(lsp::Notification&&) > handle_notification;
			std::function< void() > pump;
		};

		// @todo: concept for the type returned by the Init.
		// currently assumed to have op() overloads for message handlers, and pump().
		template < typename ImplementationInit >
		Server(
			PendingInputQueue& pending_input_queue,
			OutputQueue& output_queue,
			ExternalPump external_pump,
			ImplementationInit&& implementation_init,
			LoggingSink logging_sink = {},
			MetricsSink metrics_sink = {})
			: in_queue{ pending_input_queue }, out_queue{ output_queue }, ext_pump{ std::move(external_pump) }, logger{ logging_sink }, metrics{ std::move(metrics_sink) }
		{
			impl = wrap_implementation(std::forward< ImplementationInit >(implementation_init));
		}

#if not defined(LSP_BOOT_DISABLE_THREADS)
		auto run() -> void;
		auto request_shutdown() -> void;
#endif

		enum class UpdateResult
		{
			processed,
			idle,
			shutdown,
		};

		// single-threaded api. returns false to signal shutdown.
		auto update() -> UpdateResult;

	private:
		static auto make_not_implemented_result()
		{
			return std::unexpected(ResponseError{
				{ "code", -32803 }, // request failed. @todo: make error codes enum.
				{ "message", "Not implemented" },
			});
		}

		template < auto kind, FixedString name >
		static auto make_default_result(lsp::JsonMessage< kind, name > const&)
		{
			if constexpr (std::same_as< decltype(kind), lsp::requests::Kinds >)
			{
				// Any requests that the implementation does not handle currently considered to be errors.
				// Need to look deeper into what the expectations are though.
				return make_not_implemented_result();
			}
			else
			{
				static_assert(std::same_as< decltype(kind), lsp::notifications::Kinds >);
				return NotificationSuccessResult{};
			}
		}

		static auto make_default_result(lsp::requests::Shutdown const&)
		{
			return RequestSuccessResult{ nullptr };
		}

		template < typename ImplementationInit >
		auto wrap_implementation(ImplementationInit&& implementation_init) -> ServerImplementation
		{
			auto typed_impl = std::forward< ImplementationInit >(implementation_init)(static_cast< ServerImplAPI& >(*this));
			auto impl_ptr = typed_impl.get();

//#if defined(_MSC_VER) && !defined(__clang__) && (_MSC_VER > 1943)
//#error "This compiler version is unsupported. Pending https://developercommunity.visualstudio.com/t/Trivial-template-argument-expressions-br/10852233"
//#endif
			
			auto make_message_handler = [&]< typename Result >(std::in_place_type_t< Result >) {
				return [impl_ptr]< typename GenericMsg >(GenericMsg&& msg) mutable {
					return std::visit([&]< typename Msg >(Msg&& msg) -> Result {
						if constexpr (requires { (*impl_ptr)(std::move(msg)); })
						{
							return (*impl_ptr)(std::move(msg));
						}
						else
						{
							/* @todo: log unsupported/maybe check against our published capabilities. */
							return make_default_result(msg);
						}
					}, std::move(msg));
					};
				};

			return ServerImplementation{
				std::move(typed_impl),
				make_message_handler(std::in_place_type< RequestResult >),
				make_message_handler(std::in_place_type< NotificationResult >),
				[impl_ptr] { impl_ptr->pump(); },
			};
		}

		struct InternalMessageResult
		{
			bool exit = false;
		};

		struct DispatchResult
		{
			std::string identifier;

			std::chrono::system_clock::time_point received;
			std::chrono::system_clock::time_point dispatch_start;
			std::chrono::system_clock::time_point dispatch_end;

			InternalMessageResult result;

			auto metrics() const -> MessageMetrics
			{
				return {
					.identifier = identifier,
					.response = std::chrono::duration_cast< std::chrono::microseconds >(dispatch_end - received),
					.dispatch = std::chrono::duration_cast< std::chrono::microseconds >(dispatch_end - dispatch_start),
				};
			}
		};

		auto dispatch_request(std::string_view method, lsp::RawMessage&& msg) -> std::expected< InternalMessageResult, ResponseError >;
		auto dispatch_notification(std::string_view method, lsp::RawMessage&& msg) -> InternalMessageResult;
		auto dispatch_message(ReceivedMessage&& msg) -> InternalMessageResult;

		using RequestId = std::uint64_t; // @todo: move to lsp interface partition

		auto make_request_response(RequestId id) const -> boost::json::object;
		auto send_request_success_response(RequestId id, RequestSuccessResult result) const -> void;
		auto send_request_error_response(RequestId id, ResponseError error) const -> void;

		struct StoredRequest
		{
			RequestId id;
			lsp::Request req;
		};

		using StoredNotification = lsp::Notification;
			
		struct Job
		{
			using Variant = std::variant<
				StoredRequest,
				StoredNotification,
				ServerInternalTask
			>;

			Job(StoredRequest request) : impl{ std::move(request) }
			{
			}
			Job(StoredNotification notification) : impl{ std::move(notification) }
			{
			}
			Job(ServerInternalTask task) : impl{ std::move(task) }
			{
			}

			Variant impl;

			auto process(auto&& handler) &&
			{
				return std::visit([&](auto&& j) {
					return std::forward< decltype(handler) >(handler)(std::move(j));
					}, std::move(impl));
			}
		};

		using PendingJobs = std::queue< Job >;

		auto push_job(Job job) -> void;
		
		/**
		 * Stores request for later processing.
		 */
		auto push_request(RequestId id, lsp::Request request) -> void;

		/**
		 * Stores notification for later processing.
		 */
		auto push_notification(lsp::Notification notification) -> void;

		auto handle_request(lsp::Request request) -> RequestResult
		{
			return impl.handle_request(std::move(request));
		}

		auto handle_notification(lsp::Notification notification) -> NotificationResult
		{
			return impl.handle_notification(std::move(notification));
		}

		auto process_incoming_queue_non_blocking() -> InternalMessageResult;
		auto postprocess_message(DispatchResult const&) const -> void;
		auto pump_external() const -> void;
		auto get_status() const -> boost::json::object;

	private:
		auto send_notification_impl(lsp::RawMessage&&) const -> void override;
		auto queue_internal_task_impl(ServerInternalTask&&) -> void override;
		auto get_status_impl() const -> boost::json::object override;
		auto log_impl(LogOutputCallbackView) const -> void override;

	private:
		PendingInputQueue& in_queue;
		OutputQueue& out_queue;
		ExternalPump ext_pump;
		PendingJobs pending_jobs;
		ServerImplementation impl;
		LoggingSink logger;
		MetricsSink metrics;
#if not defined(LSP_BOOT_DISABLE_THREADS)
		std::atomic< bool > shutdown;
#endif
	};
}
