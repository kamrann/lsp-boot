
module;

// workaround: MSVC modules template specialization and boost::system::error_code
#if defined(_MSC_VER) && !defined(__clang__)
#include <boost/json/value_to.hpp>
#endif

#if not defined(LSP_BOOT_DISABLE_THREADS)
#include <asio.hpp>
#endif

#if defined(LSP_BOOT_ENABLE_IMPORT_STD)
import std;
#else
#include <utility>
#include <optional>
#include <expected>
#include <variant>
#include <string_view>
#include <string>
#include <ranges>
#include <algorithm>
#include <functional>
#include <chrono>
#include <format>
#endif
#include <cassert>

module lsp_boot.server;

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace lsp_boot
{
	using namespace lsp;

	constexpr auto message_logging_string_element_char_limit = 1024uz;
	constexpr auto message_logging_hard_char_limit = 512;// 1 << 24;

	//auto format_message_for_logging(auto msg)
	//{
	//	struct Visitor
	//	{
	//		auto operator() (boost::json::object&& obj) const
	//		{

	//		}
	//	};

	//	boost::json::visit()
	//	if (msg.contains(keys::params) and msg.at(keys::params).contains())
	//	return boost::json::serialize(msg) | std::views::take(message_logging_hard_char_limit)
	//}

	template < typename... Args >
	auto log_task_scope(ServerImplAPI const& server, std::basic_format_string< char, std::type_identity_t< Args >... > fmt_str, Args&&... args)
	{
		server.log(fmt_str, std::forward< Args >(args)...);

		struct ScopedTimer
		{
			ServerImplAPI const& server;
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

			~ScopedTimer()
			{
				auto const duration = std::chrono::steady_clock::now() - start;
				server.log("Done in {}", std::chrono::duration_cast< std::chrono::milliseconds >(duration));
			}
		};

		return ScopedTimer{ server };
	}

	struct Server::TimedJobEntry
	{
#if not defined(LSP_BOOT_DISABLE_THREADS)
		TimedJobEntry(Job&& j, asio::io_context& io) : job{ std::move(j) }, timer(io)
		{
		}

		Job job;
		asio::steady_timer timer;
		std::uint64_t generation = 0;
#endif
	};

	using JobResult = void;

	struct Server::HandleJob
	{
		Server& self;

		// @todo: think we need to take result from handle_request/handle_notification, and potentially return
		// not implemented result if the implementation had no corresponding handler. see commented out make_not_implemented_result() call above.

		auto operator() (StoredRequest&& request) const -> JobResult
		{
			auto scope = log_task_scope(self, "Processing request: id={}, method={}", request.id, lsp::message_name(request.req));

			bool const is_initialize_request = std::holds_alternative< lsp::requests::Initialize >(request.req);
			auto result = self.handle_request(std::move(request.req));

			if (result.has_value())
			{
				self.send_request_success_response(request.id, std::move(*result));

				if (is_initialize_request)
				{
					self.has_initialized = true;
				}
			}
			else
			{
				self.send_request_error_response(request.id, std::move(result.error()));
			}
		}

		auto operator() (StoredNotification&& notification) const -> JobResult
		{
			auto scope = log_task_scope(self, "Processing notification: method={}", lsp::message_name(notification));

			/*return*/ self.handle_notification(std::move(notification));
		}

		auto operator() (ServerInternalTask&& task) const -> JobResult
		{
			auto scope = log_task_scope(self, "Processing internal task");

			/*return*/ std::move(task)();
		}
	};

	Server::Server(
		PendingInputQueue& pending_input_queue,
		OutputQueue& output_queue,
		ExternalPump external_pump,
		ServerImplementation&& impl,
		LoggingSink logging_sink,
		MetricsSink metrics_sink)
		: in_queue{ pending_input_queue }
		, out_queue{ output_queue }
		, ext_pump{ std::move(external_pump) }
		, impl{ std::move(impl) }
		, logger{ logging_sink }
		, metrics{ std::move(metrics_sink) }
#if not defined(LSP_BOOT_DISABLE_THREADS)
		, work_guard{ asio::make_work_guard(io_ctx) }
#endif
	{
	}

	Server::~Server() = default;

	auto Server::dispatch_request(std::string_view const method, lsp::RawMessage&& msg) -> std::expected< InternalMessageResult, ResponseError >
	{
		if (not msg.contains(keys::id))
		{
			return InternalMessageResult{};
		}

		// @todo: try_value_to and error handling, below also.
		// @todo: not sure why we're assuming integer request ids?
		auto const request_id = value_to< RequestId >(msg.find(keys::id)->value());

		log("Dispatching request: id={}, method={}", request_id, method);
		// @todo: logging verbosity control. for now just truncating.
		log([&](auto out) {
			std::ranges::copy(boost::json::serialize(msg) | std::views::take(message_logging_hard_char_limit), out.iter());
			return out;
			});

		// @todo: consider dispatching via type list
		if (method == requests::Initialize::name)
		{
			push_request(request_id, requests::Initialize(std::move(msg)));
		}
		else if (method == requests::Shutdown::name)
		{
			push_request(request_id, requests::Shutdown(std::move(msg)));
		}
		else if (method == requests::Completion::name)
		{
			push_request(request_id, requests::Completion(std::move(msg)));
		}
		//else if (method == requests::DocumentHighlight::name)
		//{
		//	push_request(request_id, requests::DocumentHighlight(std::move(msg)));
		//}
		else if (method == requests::InlayHint::name)
		{
			push_request(request_id, requests::InlayHint(std::move(msg)));
		}
		else if (method == requests::Hover::name)
		{
			push_request(request_id, requests::Hover(std::move(msg)));
		}
		else if (method == requests::SemanticTokensFull::name)
		{
			push_request(request_id, requests::SemanticTokensFull(std::move(msg)));
		}
		else if (method == requests::SemanticTokensRange::name)
		{
			push_request(request_id, requests::SemanticTokensRange(std::move(msg)));
		}
		else if (method == requests::DocumentSymbols::name)
		{
			push_request(request_id, requests::DocumentSymbols(std::move(msg)));
		}
		else
		{
			push_request(request_id, requests::Custom(method, std::move(msg)));
		}
		//else
		//{
		//	return make_not_implemented_result();
		//}

		return InternalMessageResult{};
	}

	auto Server::dispatch_notification(std::string_view const method, lsp::RawMessage&& msg) -> InternalMessageResult
	{
		log("Dispatching notification: method={}", method);
		// @todo: logging verbosity control. for now just truncating.
		log([&](auto out) {
			std::ranges::copy(boost::json::serialize(msg) | std::views::take(message_logging_hard_char_limit), out.iter());
			return out;
			});

		if (method == notifications::Initialized::name)
		{
			// client initialization completed
			push_notification(notifications::Initialized(std::move(msg)));
		}
		else if (method == notifications::Exit::name)
		{
			return {
				.exit = true,
			};
		}
		else if (method == notifications::DidChangeConfiguration::name)
		{
			push_notification(notifications::DidChangeConfiguration(std::move(msg)));
		}
		else if (method == notifications::DidOpenTextDocument::name)
		{
			push_notification(notifications::DidOpenTextDocument(std::move(msg)));
		}
		else if (method == notifications::DidChangeTextDocument::name)
		{
			push_notification(notifications::DidChangeTextDocument(std::move(msg)));
		}
		else if (method == notifications::DidCloseTextDocument::name)
		{
			push_notification(notifications::DidCloseTextDocument(std::move(msg)));
		}
		else if (method == notifications::DidChangeWorkspaceFolders::name)
		{
			push_notification(notifications::DidChangeWorkspaceFolders(std::move(msg)));
		}
		else if (method == notifications::DidChangeWatchedFiles::name)
		{
			push_notification(notifications::DidChangeWatchedFiles(std::move(msg)));
		}
		else
		{
			push_notification(notifications::Custom(method, std::move(msg)));
		}

		return {};
	}

	auto Server::dispatch_message(ReceivedMessage&& msg) -> InternalMessageResult
	{
		auto const dispatch_start_timestamp = std::chrono::system_clock::now();

		auto&& json_msg = msg.msg;
		auto const id_it = json_msg.find(keys::id);
		auto const method_it = json_msg.find(keys::method);

		auto const extract_document_id = [&]() -> std::string_view {
			// @todo: intention is to include the readable name of the document. to do so will need to move top level control of the active documents
			// from the implementation into the base server. which probably does make sense to do.
			if (auto const params = message_params(json_msg); params != nullptr)
			{
				if (auto const params_obj = params->if_object(); params_obj != nullptr)
				{
					if (auto const doc = params_obj->if_contains(keys::text_document); doc != nullptr)
					{
						if (auto const doc_obj = doc->if_object(); doc_obj != nullptr)
						{
							if (auto const uri = doc_obj->if_contains(keys::uri); uri != nullptr)
							{
								if (auto const uri_str = uri->if_string(); uri_str != nullptr)
								{
									return *uri_str;
								}
							}
						}
					}
				}
			}

			return "?"sv;
			};

		//auto const id_str = id_it != json_msg.end() ? std::invoke([&]() -> std::optional< std::string > { auto res = try_value_to< RequestId >(id_it->value()); return res.has_value() ? std::optional(std::to_string(*res)) : std::nullopt; }) : "?"s;
		auto const method_str = method_it != json_msg.end() ? std::invoke([&]() -> std::optional< std::string_view > { return method_it->value().is_string() ? std::optional(std::string_view(method_it->value().get_string())) : std::nullopt; }) : "?"sv;
		if (/*!id_str || */ !method_str)
		{
			return {};
		}

		//auto const identifier = std::format("{}:{} [{}]", *id_str, *method_str, extract_document_id());
		auto result = std::invoke([&]() -> InternalMessageResult {
			if (id_it != json_msg.end())
			{
				if (method_it != json_msg.end())
				{
					auto dispatch_result = dispatch_request(*method_str, std::move(json_msg));
					if (!dispatch_result)
					{
						send_request_error_response(value_to< RequestId >(id_it->value()), std::move(dispatch_result.error()));
						return {};
					}
					else
					{
						return *dispatch_result;
					}
				}
				else
				{
					// Assume is a response
					if (id_it->value().is_string())
					{
						auto const request_id = std::string{ id_it->value().as_string() };
						if (auto const handler_iter = pending_response_handlers_.find(request_id); handler_iter != pending_response_handlers_.end())
						{
							if (handler_iter->second.handler_fn)
							{
								// @todo: should probably invoke on an internal task.

								// @todo: should modify handler sig to specify either result or error.
								if (auto const result_iter = json_msg.find(keys::result); result_iter != json_msg.end())
								{
									handler_iter->second.handler_fn(result_iter->value().as_object());
								}
								else if (auto const error_iter = json_msg.find(keys::error); error_iter != json_msg.end())
								{
									handler_iter->second.handler_fn(error_iter->value().as_object());
								}
								else
								{
									// @todo: log protocol error - should have either result or error.
								}
							}
							pending_response_handlers_.erase(handler_iter);
						}
						else
						{
							// @todo: no associated response handler, shouldn't happen, log warning at least.
						}
					}
				}
			}
			else if (method_it != json_msg.end())
			{
				return dispatch_notification(*method_str, std::move(json_msg));
			}
			return InternalMessageResult{};
			});

		//auto const dispatch_completion_timestamp = std::chrono::system_clock::now();

		//return std::move(result).transform([&](auto&& result) -> DispatchResult {
		//	return {
		//		.identifier = identifier,
		//		.received = msg.received_time,
		//		.dispatch_start = dispatch_start_timestamp,
		//		.dispatch_end = dispatch_completion_timestamp,
		//		.result = std::move(result)
		//	};
		//	});

		return result;
	}

	auto Server::make_request_response(RequestId id) const -> boost::json::object
	{
		return boost::json::object{
			{ "jsonrpc", "2.0" },
			{ keys::id, id },
		};
	}

	auto Server::send_request_success_response(RequestId id, RequestSuccessResult result) const -> void
	{
		auto response = make_request_response(id);

		static constexpr auto max_log_chars = 128;
		log([&](auto out) {
			std::format_to(out.iter(), "Queueing response [success]: result={:s}...", boost::json::serialize(result.json) | std::views::take(max_log_chars));
			return out;
			});

		response["result"] = std::move(result.json);
		out_queue.push(std::move(response));
	}

	auto Server::send_request_error_response(RequestId id, ResponseError error) const -> void
	{
		auto response = make_request_response(id);

		log([&](auto out) {
			std::format_to(out.iter(), "Queueing response [failure]: error={}", boost::json::serialize(error));
			return out;
			});

		response["error"] = std::move(error);
		out_queue.push(std::move(response));
	}

	auto Server::push_job(Job job) -> void
	{
		// @todo: need to reconcile threaded and non-threaded in a cleaner way.
		// currently pending_jobs and process_job_queue are both specific to non-threaded mode.
#if not defined(LSP_BOOT_DISABLE_THREADS)
		asio::post(io_ctx, [this, job = std::move(job)]() mutable {
			if (has_initialized)
			{
				auto scope = log_task_scope(*this, "Pumping implementation");
				impl.pump();
			}

			std::move(job).process(HandleJob{ *this });
			});
#else
		pending_jobs.push(std::move(job));
#endif
	}

	auto Server::push_request(RequestId id, lsp::Request request) -> void
	{
		push_job(StoredRequest{ id, std::move(request) });
	}

	auto Server::push_notification(lsp::Notification notification) -> void
	{
		push_job(std::move(notification));
	}

#if defined(LSP_BOOT_DISABLE_THREADS)
	auto Server::process_job_queue() -> bool
	{
		while (!pending_jobs.empty())
		{
			pump_external();
			auto queue_result = process_incoming_queue_non_blocking();
			if (queue_result.exit)
			{
				log("Server exiting as requested.");
				return false;
			}

			// Nothing more in incoming queue, so run next potentially long-running job.
			// (Long-running jobs are expected to periodically call the server pump to allow us to recheck queues, and also,
			// in the single-threaded context, call our external pump to update the low level connection).

			// @todo: not sure how best to appoach this, but as we currently return the request result synchronously we need to ensure that
			// any pending notifications or prior requests that could potentially affect our result have already been processed.
			// this would be fairly complex to try to handle based on what notifications there were and in what order they came, so for now
			// we simply enforce synchronization before each request.
			// may want to consider at least making the response async so we can move the pump invocation off the server dispatching thread.
			if (has_initialized)
			{
				auto scope = log_task_scope(*this, "Pumping implementation");
				impl.pump();
			}

			auto& job = pending_jobs.front();
			std::move(job).process(HandleJob{ *this });
			pending_jobs.pop();
		}

		return true;
	}
#endif

#if not defined(LSP_BOOT_DISABLE_THREADS)
	auto Server::run() -> void
	{
		// @note: this is a workaround to connect asio io context (added for timed job purposes) with the existing blocking queue.
		// if stick with asio, would make sense to just post directly to the io context instead of the queue.
		auto queue_bridge_thread = std::jthread{ [this] {
			while (!shutdown)
			{
				if (auto msg = in_queue.pop_with_abort([this] { return shutdown.load(); }); msg.has_value())
				{
					asio::post(io_ctx, [this, msg = std::move(*msg)]() mutable {
						auto dispatch_result = dispatch_message(std::move(msg));
						if (dispatch_result.exit)
						{
							log("Server exiting as requested.");
							request_shutdown();
						}
						});
				}
			}
		} };

		io_ctx.run();

		//while (!shutdown)
		//{
		//	// Run any outstanding jobs to completion.
		//	if (not process_job_queue())
		//	{
		//		return;
		//	}

		//	// Now perform blocking wait on incoming queue.
		//	if (auto msg = in_queue.pop_with_abort([this] { return shutdown.load(); }); msg.has_value())
		//	{
		//		auto dispatch_result = dispatch_message(std::move(*msg));
		//		if (dispatch_result.exit)
		//		{
		//			log("Server exiting as requested.");
		//			return;
		//		}
		//	}
		//}
	}

	auto Server::request_shutdown() -> void
	{
		shutdown = true;
		in_queue.notify();
		work_guard.reset();
	}
#else
	auto Server::update() -> UpdateResult
	{
		while (true)
		{
			// Run any outstanding jobs to completion.
			if (not process_job_queue())
			{
				return UpdateResult::shutdown;
			}

			if (auto msg = in_queue.try_pop(); msg.has_value())
			{
				auto dispatch_result = dispatch_message(std::move(*msg));
				if (dispatch_result.exit)
				{
					log("Server exiting as requested.");
					return UpdateResult::shutdown;
				}

				//return UpdateResult::processed;
			}
			else
			{
				break;
			}
		}

		return UpdateResult::idle;
	}
#endif

	auto Server::process_incoming_queue_non_blocking() -> InternalMessageResult
	{
		// Non-blocking retrieval of any incoming messages.
		// Dispatching these is trivial (they're just stored), so idea is to do this as a priority so we have most up-to-date
		// information on our current pending tasks, for purposes of client side status updates.
		while (true)
		{
			auto msg = in_queue.try_pop();
			if (not msg.has_value())
			{
				break;
			}

			auto result = dispatch_message(std::move(*msg));

			// @NOTE: For now we just bail immediately on exit request, ignoring everything else in queues and in-flight.
			if (result.exit)
			{
				return result;
			}
		}

		return {};
	}

	auto Server::postprocess_message(DispatchResult const& result) const -> void
	{
		if (metrics)
		{
			metrics(result.metrics());
		}
	}

	auto Server::pump_external() const -> void
	{
		if (ext_pump)
		{
			ext_pump();
		}
	}

	auto Server::get_status() const -> boost::json::object
	{
		return {
			{ "num_jobs",
#if not defined(LSP_BOOT_DISABLE_THREADS)
				0 // todo
#else
				pending_jobs.size()
#endif
			},
		};
	}

	auto Server::send_request_impl(lsp::RawMessage&& msg, ResponseHandlerFn&& handler) -> void
	{
		assert(has_initialized);
		auto id = std::string{ msg["id"].as_string() };
		auto const [iter, added] = pending_response_handlers_.try_emplace(id, std::move(handler));
		//assert(added);
		out_queue.push(std::move(msg));
	}

	auto Server::send_notification_impl(lsp::RawMessage&& msg) const -> void
	{
		assert(has_initialized);
		out_queue.push(std::move(msg));
	}

	auto Server::queue_internal_task_impl(ServerInternalTask&& task) -> void
	{
		//internal_task_queue.push(std::move(task));
		push_job(std::move(task));
	}

	auto Server::set_delayed_internal_task_impl(std::uint64_t id, std::chrono::milliseconds delay, ServerInternalTask&& task) -> void
	{
#if defined(LSP_BOOT_DISABLE_THREADS)
		assert(false); // not currently supported in single-threaded mode
		return;
#else
		//auto timepoint = std::chrono::steady_clock::now() + delay;
		//if (auto existing_iter = std::ranges::lower_bound(timed_jobs, id, std::ranges::less{}, &TimedJobEntry::id); existing_iter != timed_jobs.end())
		//{
		//	if (existing_iter->id == id)
		//	{
		//		// @note: currently, if resetting an existing job, we simply destroy the task that's there and replace it with the new one.
		//		timed_jobs.erase(existing_iter);
		//	}
		//}
		//auto iter = std::ranges::lower_bound(timed_jobs, timepoint, std::ranges::less{}, &TimedJobEntry::time);
		//timed_jobs.emplace(iter, id, timepoint, std::move(task));

		// @note: assumption is that this function is only invoked from the thread running the io_context.
		// timed_jobs is not synchronized.

		auto iter = timed_jobs.find(id);
		if (iter == timed_jobs.end())
		{
			log("Establishing new timed job for id {}", id);
			iter = timed_jobs.emplace(id, std::make_unique< TimedJobEntry >(std::move(task), io_ctx)).first;
		}
		else
		{
			// replace existing
			log("Updating existing timed job for id {}", id);
			iter->second->job = std::move(task);
			++iter->second->generation;
		}

		auto cancelled = iter->second->timer.expires_after(delay);
		log("{} waits cancelled", cancelled);
		auto job = [id, gen = iter->second->generation, this](std::error_code const& ec) {
			log("Timed job handler [{}/{}]: {}", id, gen, !ec ? "<completion>"s : ec.message());

			if (!ec)
			{
				auto const iter = timed_jobs.find(id);
				assert(iter != timed_jobs.end());
				if (iter->second->generation == gen)
				{
					std::move(iter->second->job).process(HandleJob{ *this });
					timed_jobs.erase(iter);
				}
				else
				{
					log("Generation not latest ({}): dropping", iter->second->generation);
				}
			}
			};
		iter->second->timer.async_wait(std::move(job));
#endif
	}

	auto Server::get_status_impl() const -> boost::json::object
	{
		return get_status();
	}

	auto Server::log_impl(LogOutputCallbackView callback) const -> void
	{
		if (logger)
		{
			logger(std::move(callback));
		}
	}
}
