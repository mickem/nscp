#include "log_controller.hpp"

#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

#include "helpers.hpp"

namespace json = boost::json;

std::string get_str_or(const json::object &o, const std::string &key, const std::string &def) {
  const auto cit = o.find(key);
  if (cit == o.end()) {
    return def;
  }
  return cit->value().as_string().c_str();
}

long long get_int_or(const json::object &o, const std::string &key, const int def) {
  const auto cit = o.find(key);
  if (cit == o.end()) {
    return def;
  }
  return cit->value().as_int64();
}

log_controller::log_controller(const int version, const boost::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                               unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/logs" : "/api/v2/logs"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &log_controller::get_log);
  addRoute("POST", "/?$", this, &log_controller::add_log);
  addRoute("GET", "/status?$", this, &log_controller::get_status);
  addRoute("DELETE", "/status?$", this, &log_controller::reset_status);
  addRoute("GET", "/since?$", this, &log_controller::get_log_since);
}

void log_controller::get_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("logs.list", request, response)) return;

  json::array root;

  std::list<std::string> levels;
  str::utils::split(levels, request.get("level", ""), ",");
  std::size_t count = 0;
  std::size_t page = str::stox<std::size_t>(request.get("page", "1"), 1);
  std::size_t ipp = str::stox<std::size_t>(request.get("per_page", "10"), 10);
  if (ipp < 2 || ipp > 500) {
    response.setCodeBadRequest("Invalid request");
    return;
  }
  std::size_t pos = (page - 1) * ipp;
  for (const error_handler_interface::log_entry &e : session->get_log_data()->get_messages(levels, pos, ipp, count)) {
    json::object node;
    node.insert(json::object::value_type("file", e.file));
    node.insert(json::object::value_type("line", e.line));
    node.insert(json::object::value_type("level", e.type));
    node.insert(json::object::value_type("date", e.date));
    node.insert(json::object::value_type("message", e.message));
    root.push_back(node);
  }
  std::string base = request.get_host() + get_prefix() + "?page=";
  std::string tail = "&per_page=" + str::xtos(ipp);
  if (!levels.empty()) {
    tail += "&level=" + str::utils::joinEx(levels, ",");
  }
  std::string next = "";
  if ((page * ipp) < count) {
    next = "<" + base + str::xtos(page + 1) + tail + ">; rel=\"next\", ";
  }
  response.setHeader("Link", next + "<" + base + str::xtos((count / ipp) + 1) + tail + ">; rel=\"last\"");
  response.setHeader("X-Pagination-Count", str::xtos(count));
  response.setHeader("X-Pagination-Page", str::xtos(page));
  response.setHeader("X-Pagination-Limit", str::xtos(ipp));
  response.append(json::serialize(root));
}

void log_controller::get_log_since(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("logs.list", request, response)) return;

  json::array root;

  auto since = str::stox<std::size_t>(request.get("since", "0"), 0);

  std::size_t count = 0;
  std::size_t page = str::stox<std::size_t>(request.get("page", "1"), 1);
  std::size_t ipp = str::stox<std::size_t>(request.get("per_page", "10"), 10);
  if (ipp < 2 || ipp > 500) {
    response.setCodeBadRequest("Invalid request");
    return;
  }
  std::size_t pos = (page - 1) * ipp;
  std::size_t last_index = since;
  for (const error_handler_interface::log_entry &e : session->get_log_data()->get_messages_since(since, pos, ipp, count)) {
    json::object node;
    node.insert(json::object::value_type("file", e.file));
    node.insert(json::object::value_type("line", e.line));
    node.insert(json::object::value_type("level", e.type));
    node.insert(json::object::value_type("date", e.date));
    node.insert(json::object::value_type("message", e.message));
    root.push_back(node);
    last_index = e.index;
  }
  std::string base = request.get_host() + get_prefix() + "?page=";
  std::string tail = "&per_page=" + str::xtos(ipp) + "&since=" + str::xtos(since);
  std::string next = "";
  if ((page * ipp) < count) {
    next = "<" + base + str::xtos(page + 1) + tail + ">; rel=\"next\", ";
  }
  response.setHeader("Link", next + "<" + base + str::xtos((count / ipp) + 1) + tail + ">; rel=\"last\"");
  response.setHeader("X-Pagination-Count", str::xtos(count));
  response.setHeader("X-Pagination-Page", str::xtos(page));
  response.setHeader("X-Pagination-Limit", str::xtos(ipp));
  response.setHeader("X-Log-Index", str::xtos(last_index));
  response.append(json::serialize(root));
}

void log_controller::add_log(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("logs.put", request, response)) return;

  try {
    auto root = json::parse(request.getData());
    std::string object_type;
    json::object o = root.as_object();
    std::string file = get_str_or(o, "file", "REST");
    int line = get_int_or(o, "line", 0);
    NSCAPI::log_level::level level = nscapi::logging::parse(get_str_or(o, "level", "error"));
    std::string message = get_str_or(o, "message", "no message");
    core->log(level, file, line, message);
  } catch (const std::exception &e) {
    response.setCodeBadRequest("Problems parsing JSON");
  }
  response.setCodeOk();
}

void log_controller::get_status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("logs.list", request, response)) return;
  error_handler_interface::status status = session->get_log_data()->get_status();
  json::object node;
  node.insert(json::object::value_type("errors", status.error_count));
  node.insert(json::object::value_type("last_error", status.last_error));
  response.append(json::serialize(node));
}

void log_controller::reset_status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("logs.list", request, response)) return;
  session->reset_log();
  json::object node;
  node.insert(json::object::value_type("errors", 0));
  node.insert(json::object::value_type("last_error", ""));
}
