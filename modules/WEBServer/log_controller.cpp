#include "log_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <str/utils.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>


log_controller::log_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController("/api/v1/logs")
{
	addRoute("GET", "/?$", this, &log_controller::get_log);
}

bool is_str_empty(const std::string& m) {
	return m.empty();
}
void log_controller::get_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin(request, response))
		return;

	if (!session->can("logs.list", request, response))
		return;

	json_spirit::Object root, log;
	json_spirit::Array data;

	std::list<std::string> levels;
	str::utils::split(levels, request.get("level", ""), ",");
	std::string str_position = request.get("pos", "0");
	std::size_t pos = str::stox<std::size_t>(str_position, 0);
	BOOST_FOREACH(const error_handler_interface::log_entry &e, session->get_log_data()->get_errors(pos)) {
		if (!levels.empty() && std::find(levels.begin(), levels.end(), e.type) == levels.end()) {
			continue;
		}
		json_spirit::Object node;
		node.insert(json_spirit::Object::value_type("file", e.file));
		node.insert(json_spirit::Object::value_type("line", e.line));
		node.insert(json_spirit::Object::value_type("level", e.type));
		node.insert(json_spirit::Object::value_type("date", e.date));
		node.insert(json_spirit::Object::value_type("message", e.message));
		data.push_back(node);
	}
	log.insert(json_spirit::Object::value_type("data", data));
	log.insert(json_spirit::Object::value_type("pos", pos));
	root.insert(json_spirit::Object::value_type("log", log));
	response.append(json_spirit::write(root));
}
