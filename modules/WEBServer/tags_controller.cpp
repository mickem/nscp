// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "tags_controller.hpp"

#include <utility>

tags_controller::tags_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                 unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/tags" : "/api/v2/tags"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &tags_controller::get_tags);
}

void tags_controller::get_tags(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("tags.get", request, response)) return;

  // get_tags_json() is already a serialized JSON object - pass it through.
  response.append(core->get_tags_json());
}
