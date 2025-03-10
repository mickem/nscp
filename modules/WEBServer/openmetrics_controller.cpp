#include "openmetrics_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_helper.hpp>

#include <str/utils.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

openmetrics_controller::openmetrics_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper *core,
                                               unsigned int plugin_id)
    : session(session), core(core), plugin_id(plugin_id), RegexpController("/api/v2/openmetrics") {
  addRoute("GET", "/?$", this, &openmetrics_controller::get_openmetrics);
}

void openmetrics_controller::get_openmetrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("openmetrics.list", request, response)) return;

  response.append(session->get_openmetrics());
}
