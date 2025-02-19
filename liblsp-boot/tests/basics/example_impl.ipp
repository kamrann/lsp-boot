
module;

#include <concepts>

export module example_impl;

import lsp_boot;

export class ExampleImpl
{
public:
	ExampleImpl(lsp_boot::ServerImplAPI& /* api */)
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
