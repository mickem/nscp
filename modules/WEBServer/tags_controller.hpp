// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "session_manager_interface.hpp"

// Host tags (key=value facts contributed by modules via the tag API, e.g.
// `drives=c:,d:` from CheckDisk). Serves the full map as a JSON object.
class tags_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  const nscapi::core_wrapper *core;
  const unsigned int plugin_id;

 public:
  tags_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core, unsigned int plugin_id);

  void get_tags(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
