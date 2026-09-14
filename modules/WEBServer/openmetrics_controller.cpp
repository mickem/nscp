// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "openmetrics_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>

#include "helpers.hpp"
#include "openmetrics_renderer.hpp"

openmetrics_controller::openmetrics_controller(const int version, const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                               unsigned int plugin_id)
    : RegexpController("/api/v2/openmetrics"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &openmetrics_controller::get_openmetrics);
}

void openmetrics_controller::get_openmetrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("openmetrics.list", request, response)) return;

  // Negotiate rather than falling back to the server default of `text/plain`
  // with no version: a scraper that asks for OpenMetrics 1.0 and is answered
  // with a bare `text/plain` parses the body with the older Prometheus text
  // parser instead. The body is identical either way.
  response.setHeader("Content-Type", openmetrics::content_type_for(request.readHeader("Accept")));
  response.append(session->get_open_metrics());
}
