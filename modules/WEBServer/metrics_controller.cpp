#include "metrics_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "helpers.hpp"

metrics_controller::metrics_controller(const int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                       unsigned int plugin_id)
    : RegexpController("/api/v2/metrics"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &metrics_controller::get_metrics);
}

void metrics_controller::get_metrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metrics.list", request, response)) return;

  response.append(session->get_metrics_v2());
}
