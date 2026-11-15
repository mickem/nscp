// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

#include "session_manager_interface.hpp"

// REST view over the core's latest diagnostics snapshot
// (service/diagnostics_snapshot.hpp, reached through NSAPIDiagnosticsQuery):
//
//   GET /api/v2/diagnostics             the sections, without their rows   diagnostics.list
//   GET /api/v2/diagnostics/<section>   one section with its rows          diagnostics.list
//
// Both serve what the last diagnostics round left, the way /api/v2/metrics
// serves the last metrics round: a request never triggers a collection. With
// [/settings/core/failed checks] capture diagnostics off the list answers
// `enabled:false` and no sections, and a section answers 404 saying so.
//
// `diagnostics.list` is held by no default role (only by `full`, through
// `*`): the tables name the processes running on the host, which is more
// than a monitoring or metrics role should read by default.
class diagnostics_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  diagnostics_controller(int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void list_sections(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_section(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
