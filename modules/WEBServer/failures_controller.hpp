// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

#include "session_manager_interface.hpp"

// REST view over the core's failed-check log (service/failed_check_store.hpp,
// reached through NSAPIFailuresQuery):
//
//   GET    /api/v2/failures   the last failed checks, newest first   failures.list
//   DELETE /api/v2/failures   empty the log                          failures.delete
//
// The log is the core's, not this module's: every transport's queries land in
// it, so the page shows NRPE and scheduler failures alongside REST ones. The
// list carries the store's configuration (`enabled`, `arguments_recorded`,
// ...) so a UI can say "recording is off" rather than render an empty table;
// a disabled log answers 200 with `enabled:false` and no entries, since the
// envelope already tells the two apart.
class failures_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  failures_controller(int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void list_failures(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void clear_failures(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
