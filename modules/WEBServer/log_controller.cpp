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

	json_spirit::Array root;

	std::list<std::string> levels;
	str::utils::split(levels, request.get("level", ""), ",");
	std::size_t count = 0;
	std::size_t page = str::stox<std::size_t>(request.get("page", "1"), 1);
	std::size_t ipp = str::stox<std::size_t>(request.get("per_page", "10"), 10);
	if (ipp < 2 || ipp > 100 || page < 0) {
		response.setCode(HTTP_BAD_REQUEST);
		response.append("Invalid request");
		return;
	}
	std::size_t pos = (page-1)*ipp;
	BOOST_FOREACH(const error_handler_interface::log_entry &e, session->get_log_data()->get_messages(levels, pos, ipp, count)) {
		json_spirit::Object node;
		node.insert(json_spirit::Object::value_type("file", e.file));
		node.insert(json_spirit::Object::value_type("line", e.line));
		node.insert(json_spirit::Object::value_type("level", e.type));
		node.insert(json_spirit::Object::value_type("date", e.date));
		node.insert(json_spirit::Object::value_type("message", e.message));
		root.push_back(node);
	}
	std::string base = request.get_host() + "/api/v1/logs?page=";
	std::string tail = "&per_page=" + str::xtos(ipp);
	if (!levels.empty()) {
		tail += "&level=" + str::utils::joinEx(levels, ",");
	}
	std::string next = "";
	if ((page*ipp) < count) {
		next = "<" + base + str::xtos(page + 1) + tail + ">; rel=\"next\", ";
	}
	response.setHeader("Link", next + "<" + base + str::xtos((count / ipp) + 1) + tail + ">; rel=\"last\"");
	response.append(json_spirit::write(root));
}
