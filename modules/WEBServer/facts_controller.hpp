// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

// Host facts: the structured inventory modules publish through fetchFacts -
// which volumes exist, what the hardware is, which software is installed - as
// opposed to tags (tags_controller), which are the flat strings a fleet
// selector matches on.
//
// Three routes:
//   GET  /api/v2/facts          the whole document, with its revision, hash,
//                               the enabled fact set ids and any collection
//                               errors                          (facts.get)
//   GET  /api/v2/facts/<path>   the subtree at a dotted path, 404 when the
//                               path is not there                (facts.get)
//   POST /api/v2/facts/refresh  run a collection round now       (facts.refresh)
//
// The refresh is a separate grant because it is the one that costs something:
// it asks every producer to collect, which for the expensive sets means
// re-reading the installed-software hives or querying Windows Update.
class facts_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  facts_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_fact_path(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void refresh_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);

  // Whether `path` is a dotted fact path the core will accept: snake_case
  // components separated by single dots. Static so the test can drive it
  // without a session, and applied before the path reaches the core so a URL
  // cannot smuggle anything into the query.
  static bool is_safe_fact_path(const std::string &path);
};
