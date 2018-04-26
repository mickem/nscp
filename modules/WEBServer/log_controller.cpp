#include "log_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_helper.hpp>

#include <str/utils.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>


std::string get_str_or(const json_spirit::Object &o, const std::string key, const std::string def) {
	json_spirit::Object::const_iterator cit = o.find(key);
	if (cit == o.end()) {
		return def;
	}
	return cit->second.getString();
}

int get_int_or(const json_spirit::Object &o, const std::string key, const int def) {
	json_spirit::Object::const_iterator cit = o.find(key);
	if (cit == o.end()) {
		return def;
	}
	return cit->second.getInt();
}


log_controller::log_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/logs":"/api/v2/logs")
{
	addRoute("GET", "/?$", this, &log_controller::get_log);
	addRoute("POST", "/?$", this, &log_controller::add_log);
}

bool is_str_empty(const std::string& m) {
	return m.empty();
}
void log_controller::get_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("logs.list", request, response))
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
	std::string base = request.get_host() + get_prefix() + "?page=";
	std::string tail = "&per_page=" + str::xtos(ipp);
	if (!levels.empty()) {
		tail += "&level=" + str::utils::joinEx(levels, ",");
	}
	std::string next = "";
	if ((page*ipp) < count) {
		next = "<" + base + str::xtos(page + 1) + tail + ">; rel=\"next\", ";
	}
	response.setHeader("Link", next + "<" + base + str::xtos((count / ipp) + 1) + tail + ">; rel=\"last\"");
	response.setHeader("X-Pagination-Count", str::xtos(count));
	response.setHeader("X-Pagination-Page", str::xtos(page));
	response.setHeader("X-Pagination-Limit", str::xtos(ipp));
	response.append(json_spirit::write(root));
}

void log_controller::add_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("logs.put", request, response))
		return;

	try {
		json_spirit::Value root;
		std::string data = request.getData();
		json_spirit::read_or_throw(data, root);
		std::string object_type;
		json_spirit::Object o = root.getObject();
		std::string file = get_str_or(o, "file", "REST");
		int line = get_int_or(o, "line", 0);
		NSCAPI::log_level::level level = nscapi::logging::parse(get_str_or(o, "level", "error"));
		std::string message = get_str_or(o, "message", "no message");
		core->log(level, file, line, message);
	} catch (const json_spirit::ParseError &e) {
		response.setCode(HTTP_BAD_REQUEST);
		response.append("Problems parsing JSON");
	}
	response.setCode(HTTP_OK);
}
