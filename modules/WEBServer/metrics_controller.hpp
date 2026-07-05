// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <client/simple_client.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

class metrics_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper* core;
  const unsigned int plugin_id;

  typedef std::vector<std::pair<std::string, std::string> > arg_vector;

 public:
  metrics_controller(const int version, const std::shared_ptr<session_manager_interface>& session, const nscapi::core_wrapper* core, unsigned int plugin_id);

  void get_metrics(Mongoose::Request& request, boost::smatch& what, Mongoose::StreamResponse& response);
};
