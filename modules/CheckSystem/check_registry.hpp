// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/prefix_access_policy.hpp>

#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <win/registry.hpp>

// ── check_registry_key ───────────────────────────────────────────────────────

// Normalise the long hive spelling to the abbreviation, so an allow list
// written one way still matches a caller who used the other. Both are accepted
// by win_registry::parse_hive.
std::string normalize_registry_hive(const std::string &key);

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

// `access` decides which keys the caller may name; see
// docs/docs/concepts/check-access.md. It is passed in rather than looked up so
// the gate can be unit tested without a live module.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::prefix_policy &access);

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

// `access` decides which keys the caller may name; see
// docs/docs/concepts/check-access.md.
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::prefix_policy &access);

}  // namespace registry_value_checks
