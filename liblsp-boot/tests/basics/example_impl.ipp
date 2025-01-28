
module;

#include <concepts>

export module example_impl;

import lsp_boot;

export class ExampleImpl
{
public:
	template < std::regular_invocable< lsp_boot::lsp::RawMessage&& > SendNotify >
	ExampleImpl(SendNotify&& /*send_notify*/)
	{
	}

	auto operator() (lsp_boot::lsp::requests::Initialize&& msg) -> lsp_boot::Server::RequestResult
	{
		return {};
	}

	auto operator() (lsp_boot::lsp::requests::Shutdown&& msg) -> lsp_boot::Server::RequestResult
	{
		return {};
	}

	auto pump() -> void
	{
	}
};
