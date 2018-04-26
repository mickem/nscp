#include "metrics_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <str/utils.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

metrics_controller::metrics_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController("/api/v2/metrics")
{
	addRoute("GET", "/?$", this, &metrics_controller::get_metrics);
}

void metrics_controller::get_metrics(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("metrics.list", request, response))
		return;

	response.append(session->get_metrics_v2());

}
