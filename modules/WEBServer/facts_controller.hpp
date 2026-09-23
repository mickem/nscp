// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

// Host facts: the opt-in inventory the core collects from its modules on a
// schedule (see service/fact_repository.hpp).
//
// Distinct from the tags controller next door, and deliberately so. A tag is a
// flat key=value a fleet selector matches whole; the facts document is a tree
// with lists in it, and it carries the bookkeeping a consumer needs to render
// and cache it - the revision, when the last round ran, which sets are enabled
// and which of them failed.
//
// `GET /api/v2/facts` serves the whole document, `?path=os.family` a subtree.
// A path nothing produced answers 200 with `found: false`: "no such subtree"
// and "the call failed" are different answers, and a UI that asks for a set an
// operator has not enabled should render "not collected", not an error.
class facts_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  facts_controller(int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void refresh_facts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
