// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "metrics_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "helpers.hpp"

metrics_controller::metrics_controller(const int version, const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                       unsigned int plugin_id)
    : RegexpController("/api/v2/metrics"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &metrics_controller::get_metrics);
}

namespace {
// `?meta=1`, `?meta=true` and `?meta=yes` ask for the described document;
// anything else - `?meta=0`, a bare `?meta` with no value, no `meta` at all -
// is the plain flat list this endpoint has always returned.
bool wants_metadata(const Mongoose::Request &request) {
  const std::string value = request.get("meta", "");
  return value == "1" || boost::iequals(value, "true") || boost::iequals(value, "yes");
}
}  // namespace

void metrics_controller::get_metrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metrics.list", request, response)) return;

  // The described document is a different shape (values under `metrics`, what
  // they mean under `metadata`), so it is opt-in: a scraper or dashboard that
  // does not ask for it keeps reading the flat key/value map it always did.
  if (wants_metadata(request)) {
    response.append(session->get_metrics_v2_described());
    return;
  }
  response.append(session->get_metrics_v2());
}
