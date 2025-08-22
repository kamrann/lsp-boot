
#if defined(LSP_BOOT_DISABLE_THREADS)

#include <liblsp-boot/version.hpp>

#include <optional>
#include <string_view>
#include <sstream>
#include <format>
#include <iostream>
#include <memory>

#undef NDEBUG
#include <cassert>

import lsp_boot;
import lsp_boot.ext_mod_wrap.boost.json;
import lsp_boot.utility;
import example_impl;

using namespace std::string_view_literals;

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
#if defined(LSP_BOOT_DISABLE_THREADS)
	std::stringstream in, out;

	in << format_request("initialize", boost::json::object{
		{ "processId", nullptr },
		{ "rootUri", nullptr },
		{ "capabilities", boost::json::object{} },
		});

	// @todo: some basic response checking

	in << format_request("shutdown");

	in << format_notification("exit");

	auto input_queue = lsp_boot::PendingInputQueue{};
	auto output_queue = lsp_boot::OutputQueue{};

	auto server_impl_init = [](auto&& send_notify) {
		return std::make_unique< ExampleImpl >(send_notify);
		};

	auto server = lsp_boot::Server(input_queue, output_queue, [] {}, server_impl_init);
	auto connection = lsp_boot::StreamConnection(input_queue, output_queue, in, out, std::cerr);

	bool connection_done = false;
	bool server_done = false;
	while (!connection_done || !server_done)
	{
		if (!connection_done && !connection.update())
		{
			std::cerr << "StreamConnection completed." << std::endl;
			connection_done = true;
		}

		if (!server_done && server.update() == lsp_boot::Server::UpdateResult::shutdown)
		{
			std::cerr << "Server execution completed." << std::endl;
			server_done = true;
		}
	}
#endif

	return 0;
}
