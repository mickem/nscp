// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace users_filter {

// One interactive logon session (a Windows session or a Unix utmp entry).
struct filter_obj {
  std::string get_user() const { return user; }
  std::string get_session_state() const { return session_state; }
  std::string get_session_type() const { return session_type; }
  std::string get_client() const { return client; }
  std::string show() const { return user; }

  std::string user;           // account name
  std::string session_state;  // "active" / "disconnected" / ...
  std::string session_type;   // "console" / "rdp" / "remote" / ...
  std::string client;         // client name or remote host (may be empty)
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace users_filter

namespace users_source {
// Populate the currently logged-on sessions for this platform (WTS on Windows,
// utmp on Unix). Sets `error` on failure.
void gather(std::vector<users_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace users_source

namespace check_users_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
