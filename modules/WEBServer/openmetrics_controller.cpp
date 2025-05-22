#include "openmetrics_controller.hpp"
#include "helpers.hpp"

#include <boost/json.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

openmetrics_controller::openmetrics_controller(const int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                               unsigned int plugin_id)
    : RegexpController("/api/v2/openmetrics"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &openmetrics_controller::get_openmetrics);
}

void openmetrics_controller::get_openmetrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("openmetrics.list", request, response)) return;

  response.append(session->get_openmetrics());
}
