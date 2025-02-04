// Copyright 2024 Cameron Angus.

module;

#include <boost/json.hpp>

export module lsp_boot.ext_mod_wrap.boost.json;

namespace bj = boost::json;

export namespace boost::json
{
    using bj::value;
    using bj::string;
    using bj::array;
    using bj::object;

    using bj::string_view;
    using bj::system_error;

    using bj::kind;
    using bj::visit;

    using bj::value_to;
    using bj::try_value_to;
    using bj::value_from;

    using bj::parse;
    using bj::serialize;
    using bj::serializer;
    using bj::to_string;
}
