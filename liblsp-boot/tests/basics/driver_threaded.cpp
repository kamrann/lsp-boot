
// @todo: should probably enable/disable in the buildfile using metadata
#if not defined(LSP_BOOT_DISABLE_THREADS)

#include <liblsp-boot/version.hpp>

#include <string_view>
#include <sstream>
#include <format>
#include <iostream>
#include <memory>
#include <future>
#include <thread>
#include <chrono>

#undef NDEBUG
#include <cassert>

import lsp_boot;
import lsp_boot.ext_mod_wrap.boost.json;
import lsp_boot.utility;
import example_impl;

using namespace std::string_view_literals;
using namespace std::chrono_literals;

unsigned long message_counter = 0;

auto format_message(boost::json::value const& js)
{
	auto const content = boost::json::serialize(js);
	return std::format("Content-Length: {}\r\n\r\n{}", content.length(), content);
}

auto format_request(std::string_view const method, std::optional< boost::json::value > const& params = std::nullopt)
{
	return format_message(boost::json::object{
		{ "jsonrpc", "2.0" },
		{ "id", message_counter++ },
		{ "method", method },
		{ "params", params.value_or(boost::json::object{}) },
		});
}

auto format_notification(std::string_view const method, std::optional< boost::json::value > const& params = std::nullopt)
{
	return format_message(boost::json::object{
		{ "jsonrpc", "2.0" },
		{ "method", method },
		{ "params", params.value_or(boost::json::object{}) },
		});
}

#endif

int main ()
{
#if not defined(LSP_BOOT_DISABLE_THREADS)
	std::stringstream in, out;

	in << format_request("initialize", boost::json::object{
		{ "processId", nullptr },
		{ "rootUri", nullptr },
		{ "capabilities", boost::json::object{} },
		});

	// @todo: some basic response checking

	in << format_request("shutdown");

	in << format_notification("exit");

	auto const run_server = [&] {
		auto input_queue = lsp_boot::PendingInputQueue{};
		auto output_queue = lsp_boot::OutputQueue{};

		auto server_impl_init = [](auto&& send_notify) {
			return std::make_unique< ExampleImpl >(send_notify);
			};

		auto server = lsp_boot::Server(input_queue, output_queue, server_impl_init);
		auto server_thread = lsp_boot::Thread([&] {
			server.run();
			std::cerr << "Server execution completed." << std::endl;
			});

		auto connection = lsp_boot::StreamConnection(input_queue, output_queue, in, out, std::cerr);
		auto const result = connection.listen();
		std::cerr << "StreamConnection completed." << std::endl;
		assert(result == 0);
		};

	auto server_fut = std::async(std::launch::async, run_server);

	auto timeout = 5000ms;
	while (timeout.count() > 0)
	{
		if (server_fut.wait_for(0ms) == std::future_status::ready)
		{
			break;
		}

		auto const dur = 200ms;
		std::this_thread::sleep_for(dur);
		timeout = timeout > dur ? (timeout - dur) : 0ms;
	}

	// Server should have exited
	assert(server_fut.wait_for(0ms) == std::future_status::ready);
#endif

	return 0;
}
