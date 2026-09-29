// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <string>

namespace ad_replication_filter {

// One inbound replication link (neighbor) of a domain controller: a
// (naming context, source DC) pair. Timestamps are unix epoch seconds
// (0 = never) so the where-engine date keywords work unmodified; the
// win32 acquisition (ad_replication_source_win.cpp) converts FILETIMEs.
struct filter_obj {
  filter_obj() : last_attempt(0), last_success(0), last_error(0), consecutive_failures(0) {}

  std::string get_naming_context() const { return naming_context; }
  std::string get_source() const { return source_server; }
  std::string get_source_dsa() const { return source_dsa_dn; }
  std::string get_source_address() const { return source_address; }
  std::string get_last_error_message() const { return last_error_message; }

  long long get_last_attempt() const { return last_attempt; }
  long long get_last_success() const { return last_success; }
  long long get_last_error() const { return last_error; }
  long long get_consecutive_failures() const { return consecutive_failures; }
  long long get_failed() const { return last_error != 0 ? 1 : 0; }

  std::string get_last_attempt_su() const { return last_attempt == 0 ? "never" : str::format::format_date(static_cast<std::time_t>(last_attempt)); }
  std::string get_last_success_su() const { return last_success == 0 ? "never" : str::format::format_date(static_cast<std::time_t>(last_success)); }

  std::string show() const { return source_server + " " + naming_context; }

  std::string naming_context;      // DN of the replicated partition
  std::string source_server;       // friendly source DC name (from the DSA DN)
  std::string source_dsa_dn;       // full source DSA object DN
  std::string source_address;      // transport address (GUID-based DNS name)
  std::string last_error_message;  // formatted win32 message for last_error ("" when ok)
  long long last_attempt;          // unix epoch seconds, 0 = never
  long long last_success;          // unix epoch seconds, 0 = never
  long long last_error;            // win32 result of the last sync (0 = ok)
  long long consecutive_failures;  // consecutive failed sync attempts
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

// "CN=NTDS Settings,CN=DC02,CN=Servers,CN=Site,CN=Sites,..." -> "DC02".
// Falls back to the full DN when the shape is not the expected NTDS DSA DN.
std::string extract_server_from_ntds_dn(const std::string &dn);

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace ad_replication_filter
