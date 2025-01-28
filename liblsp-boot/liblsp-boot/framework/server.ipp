
module;

#if defined(lsp_boot_enable_import_std)
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
		using GenericResult = boost::json::value; // @todo: simplifying for now, unclear if reason to maintain static typing.
		using ServerImplementation = std::function< GenericResult(lsp::Message&&) >;
		using ServerPump = std::function< void() >;

		// @todo: concept for the type returned by the Init.
		// currently assumed to have op() overloads for message handlers, and pump().
		template < typename ImplementationInit >
		Server(
			PendingInputQueue& pending_input_queue,
			OutputQueue& output_queue,
			ImplementationInit&& implementation_init)
			: in_queue{ pending_input_queue }, out_queue{ output_queue }
		{
			std::tie(impl_handler, impl_pump) = wrap_implementation(std::forward< ImplementationInit >(implementation_init));
		}

		auto run() -> void;

	private:
		template < typename ImplementationInit >
		auto wrap_implementation(ImplementationInit&& implementation_init) -> std::pair< ServerImplementation, ServerPump >
		{
			// @todo: probably won't be sufficient, suspect we may need more complex interactions with the server implementation
			// needing to send requests to the client too.
			auto send_notify = [this](lsp::RawMessage&& msg) {
				out_queue.push(std::move(msg));
				};
			//using ImplType = std::remove_reference_t< std::invoke_result_t< ImplementationInit, decltype(send_notify) > >;
			//auto impl = std::forward< ImplementationInit >(implementation_init)(send_notify);
			auto impl = std::forward< ImplementationInit >(implementation_init)(send_notify);
			// @todo: yuck. ideally want to use a function supporting non-copyable.
			auto impl_ptr = std::shared_ptr{ std::move(impl) };
			auto handler = [impl_ptr](lsp::Message&& msg) mutable {
				return std::visit([&]< typename M >(M && msg) -> GenericResult {
					if constexpr (requires { (*impl_ptr)(std::move(msg)); })
					{
						return (*impl_ptr)(std::move(msg));
					}
					else
					{
						/* @todo: log unsupported/maybe check against our published capabilities. */
						return boost::json::value{};
					}
				}, std::move(msg));
				};
			auto pump = [impl_ptr] {
				impl_ptr->pump();
				};
			return std::pair{ handler, pump };
		}

		auto dispatch_request(std::string_view method, lsp::RawMessage&& msg) -> void;
		auto dispatch_notification(std::string_view method, lsp::RawMessage&& msg) -> void;
		auto dispatch_message(lsp::RawMessage&& msg) -> void;

		auto handle(lsp::Message msg)
		{
			return impl_handler(std::move(msg));
		}

	private:
		PendingInputQueue& in_queue;
		OutputQueue& out_queue;
		ServerImplementation impl_handler;
		std::function< void() > impl_pump;
	};
}
