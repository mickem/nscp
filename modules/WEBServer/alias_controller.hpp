// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

// Lists registered query aliases (QUERY_ALIAS items in the registry).
// Distinct from query_controller which lists QUERY items: aliases are
// admin-defined wrappers around real queries (see CheckHelpers'
// [/settings/check helpers/alias] section and CheckExternalScripts'
// [/settings/external scripts/alias] section).
//
// Only the list endpoint is exposed - execution of an alias goes through
// /api/vX/queries/<alias>/commands/execute exactly like any other query
// (the core dispatches the alias to its target command), so duplicating
// the execute machinery here would be pure noise.
class alias_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  alias_controller(int version, const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_aliases(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
