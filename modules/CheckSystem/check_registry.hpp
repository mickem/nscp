// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <win/registry.hpp>

// ── check_registry_key ───────────────────────────────────────────────────────

namespace registry_key_checks {

namespace check_rk_filter {

typedef win_registry::key_info filter_obj;
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

// Exposed for unit testing
long long parse_type_name(const std::string &s);

}  // namespace check_rk_filter

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace registry_key_checks

// ── check_registry_value ─────────────────────────────────────────────────────

namespace registry_value_checks {

namespace check_rv_filter {

typedef win_registry::value_info filter_obj;
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_rv_filter

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

}  // namespace registry_value_checks
