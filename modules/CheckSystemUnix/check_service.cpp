#include "check_service.h"

#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cstdio>
#include <memory>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <sstream>

namespace po = boost::program_options;

using namespace parsers::where;

namespace checks {

namespace check_svc_filter {

// Case-insensitive string comparison
struct CaseBlindCompare {
  bool operator()(const std::string &a, const std::string &b) const { return boost::ilexicographical_compare(a, b); }
};

// Execute a command and return its output
std::string exec_command(const std::string &cmd) {
  std::array<char, 4096> buffer{};
  std::string result;
  const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) {
    return "";
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

// Parse state helper functions for the filter
node_type parse_state(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
}

node_type parse_start_type(boost::shared_ptr<filter_obj> object, evaluation_context context, node_type subject) {
  return factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
}

node_type state_is_ok(const value_type, evaluation_context context, const node_type subject) {
  auto *native = reinterpret_cast<native_context *>(context.get());
  if (!native->has_object()) {
    context->error("No object available");
    return factory::create_false();
  }
  return native->get_object()->state_is_ok() ? factory::create_true() : factory::create_false();
}

node_type state_is_perfect(const value_type, evaluation_context context, const node_type subject) {
  auto *native = reinterpret_cast<native_context *>(context.get());
  if (!native->has_object()) {
    context->error("No object available");
    return factory::create_false();
  }
  return native->get_object()->state_is_perfect() ? factory::create_true() : factory::create_false();
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_state = type_custom_int_1;
  static constexpr value_type type_custom_start_type = type_custom_int_2;

  registry_.add_string("name", &filter_obj::get_name, "Service name")
      .add_string("desc", &filter_obj::get_desc, "Service description")
      .add_string("sub_state", &filter_obj::get_sub_state, "Service sub-state (running, dead, exited, etc.)");

  registry_.add_int_x("pid", &filter_obj::get_pid, "Process id")
      .add_int_x("state", type_custom_state, &filter_obj::get_state_i, "The current state (active, inactive, failed)")
      .add_int_perf("", "")
      .add_int_x("start_type", type_custom_start_type, &filter_obj::get_start_type_i, "The configured start type (enabled, disabled, static, masked)")
      .add_int_x("started", type_bool, &filter_obj::get_started, "Service is started/active")
      .add_int_x("stopped", type_bool, &filter_obj::get_stopped, "Service is stopped/inactive");

  // clang-format off
  registry_.add_int_fun()
    ("state_is_perfect", type_bool, &state_is_perfect, "Check if the state is perfect (enabled services running, disabled services stopped)")
    ("state_is_ok", type_bool, &state_is_ok, "Check if the state is ok (enabled services running or starting, disabled services can be any state)")
    ;
  // clang-format on

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state")
      .add_human_string("start_type", &filter_obj::get_start_type_s, "The configured start type");

  registry_.add_converter()(type_custom_state, &parse_state)(type_custom_start_type, &parse_start_type);
}

// Get service info using systemctl show
filter_obj get_service_info(const std::string &service_name) {
  filter_obj info;
  info.name = service_name;

  // Get detailed service info using systemctl show
  std::string cmd = "systemctl show " + service_name + " --no-pager 2>/dev/null";
  std::string output = exec_command(cmd);

  std::istringstream iss(output);
  std::string line;
  while (std::getline(iss, line)) {
    std::size_t pos = line.find('=');
    if (pos == std::string::npos) continue;

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    boost::trim(value);

    if (key == "Description") {
      info.desc = value;
    } else if (key == "LoadState") {
      info.load_state = value;
    } else if (key == "ActiveState") {
      info.state = value;
    } else if (key == "SubState") {
      info.sub_state = value;
    } else if (key == "UnitFileState") {
      info.start_type = value;
    } else if (key == "MainPID") {
      try {
        info.pid = std::stoi(value);
      } catch (...) {
        info.pid = 0;
      }
    }
  }

  // If UnitFileState is empty, try to get it from is-enabled
  if (info.start_type.empty()) {
    cmd = "systemctl is-enabled " + service_name + " 2>/dev/null";
    std::string enabled_output = exec_command(cmd);
    boost::trim(enabled_output);
    if (!enabled_output.empty()) {
      info.start_type = enabled_output;
    }
  }

  return info;
}

// List all services using systemctl
std::vector<filter_obj> enumerate_services(const std::string &type, const std::string &state_filter) {
  std::vector<filter_obj> result;

  // Build the systemctl command
  std::string cmd = "systemctl list-units --type=service --all --no-pager --no-legend 2>/dev/null";
  std::string output = exec_command(cmd);

  std::istringstream iss(output);
  std::string line;
  while (std::getline(iss, line)) {
    boost::trim(line);
    if (line.empty()) continue;

    // Parse: UNIT LOAD ACTIVE SUB DESCRIPTION
    std::istringstream line_iss(line);
    std::string unit, load, active, sub;
    line_iss >> unit >> load >> active >> sub;

    // Get the rest as description
    std::string desc;
    std::getline(line_iss, desc);
    boost::trim(desc);

    // Remove .service suffix for cleaner names
    std::string name = unit;
    if (boost::ends_with(name, ".service")) {
      name = name.substr(0, name.length() - 8);
    }

    filter_obj info;
    info.name = name;
    info.desc = desc;
    info.load_state = load;
    info.state = active;
    info.sub_state = sub;

    // Apply state filter
    if (state_filter == "active" && active != "active") continue;
    if (state_filter == "inactive" && active != "inactive") continue;
    if (state_filter == "failed" && active != "failed") continue;

    // Get enabled status
    std::string enabled_cmd = "systemctl is-enabled " + unit + " 2>/dev/null";
    std::string enabled_output = exec_command(enabled_cmd);
    boost::trim(enabled_output);
    info.start_type = enabled_output;

    // Get PID for running services
    if (active == "active" && sub == "running") {
      std::string pid_cmd = "systemctl show " + unit + " --property=MainPID --value 2>/dev/null";
      std::string pid_output = exec_command(pid_cmd);
      boost::trim(pid_output);
      try {
        info.pid = std::stoi(pid_output);
      } catch (...) {
        info.pid = 0;
      }
    }

    result.push_back(info);
  }

  return result;
}

}  // namespace check_svc_filter

void check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_svc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> services, excludes;
  std::string state;

  filter_type filter;
  filter_helper.add_options("not state_is_perfect()", "not state_is_ok()", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${crit_list}, delayed (${warn_list})", "${name}=${state} (${start_type})", "${name}", "%(status): No services found",
                           "%(status): All %(count) service(s) are ok.");

  // clang-format off
  filter_helper.get_desc().add_options()
    ("service", po::value<std::vector<std::string>>(&services), "The service to check, set this to * to check all services")
    ("exclude", po::value<std::vector<std::string>>(&excludes), "A list of services to ignore (mainly useful in combination with service=*)")
    ("state", po::value<std::string>(&state)->default_value("all"), "The state of services to enumerate: active, inactive, failed, or all")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (services.empty()) {
    services.emplace_back("*");
  }

  if (!filter_helper.build_filter(filter)) return;

  for (const std::string &service : services) {
    if (service == "*") {
      // Enumerate all services
      std::vector<check_svc_filter::filter_obj> service_list = check_svc_filter::enumerate_services("service", state);
      for (const check_svc_filter::filter_obj &info : service_list) {
        // Check excludes
        if (std::find(excludes.begin(), excludes.end(), info.name) != excludes.end()) continue;

        boost::shared_ptr<check_svc_filter::filter_obj> record(new check_svc_filter::filter_obj(info));
        filter.match(record);
        if (filter.has_errors()) {
          return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
        }
      }
    } else {
      // Get specific service
      std::string service_name = service;
      // Add .service suffix if not present
      if (!boost::ends_with(service_name, ".service")) {
        service_name += ".service";
      }

      check_svc_filter::filter_obj info = check_svc_filter::get_service_info(service_name);

      // Remove .service suffix for display
      if (boost::ends_with(info.name, ".service")) {
        info.name = info.name.substr(0, info.name.length() - 8);
      }

      boost::shared_ptr<check_svc_filter::filter_obj> record(new check_svc_filter::filter_obj(info));
      filter.match(record);
      if (filter.has_errors()) {
        return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed (see log for details)");
      }
    }
  }

  filter_helper.post_process(filter);
}

}  // namespace checks
