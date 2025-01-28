
module;

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <string_view>
#include <functional>
#include <variant>
#include <memory>
#endif

export module lsp_boot.server;

import lsp_boot.lsp;
import lsp_boot.work_queue;

import ext_mod_wrap.boost.json;

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

	export class Server
	{
	public:
		using RequestResult = boost::json::value; // @todo: simplifying for now, unclear if reason to maintain static typing.
		using NotificationResult = boost::json::value; // @todo: simplifying for now, unclear if reason to maintain static typing.
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
			ImplementationInit&& implementation_init)
			: in_queue{ pending_input_queue }, out_queue{ output_queue }
		{
			impl = wrap_implementation(std::forward< ImplementationInit >(implementation_init));
		}

		auto run() -> void;

	private:
		template < typename ImplementationInit >
		auto wrap_implementation(ImplementationInit&& implementation_init) -> ServerImplementation
		{
			// @todo: probably won't be sufficient, suspect we may need more complex interactions with the server implementation
			// needing to send requests to the client too.
			auto send_notify = [this](lsp::RawMessage&& msg) {
				out_queue.push(std::move(msg));
				};
			auto typed_impl = std::forward< ImplementationInit >(implementation_init)(send_notify);
			auto impl_ptr = typed_impl.get();
			
			auto make_message_handler = [&]< typename Result >(std::in_place_type_t< Result >) {
				return [impl_ptr](auto&& msg) mutable {
					return std::visit([&]< typename M >(M&& msg) -> Result {
						if constexpr (requires { (*impl_ptr)(std::move(msg)); })
						{
							return (*impl_ptr)(std::move(msg));
						}
						else
						{
							/* @todo: log unsupported/maybe check against our published capabilities. */
							return Result{};
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

		auto dispatch_request(std::string_view method, lsp::RawMessage&& msg) -> InternalMessageResult;
		auto dispatch_notification(std::string_view method, lsp::RawMessage&& msg) -> InternalMessageResult;
		auto dispatch_message(lsp::RawMessage&& msg) -> InternalMessageResult;

		auto handle_request(lsp::Request request) -> RequestResult
		{
			return impl.handle_request(std::move(request));
		}

		auto handle_notification(lsp::Notification notification) -> NotificationResult
		{
			return impl.handle_notification(std::move(notification));
		}

	private:
		PendingInputQueue& in_queue;
		OutputQueue& out_queue;
		ServerImplementation impl;
	};
}
