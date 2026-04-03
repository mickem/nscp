/*
* Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "check_service.h"

#include "nsclient/nsclient_exception.hpp"
#include "parsers/filter/cli_helper.hpp"

namespace po = boost::program_options;

namespace service_checks {
namespace check_svc_filter {
using namespace parsers::where;

bool check_state_is_perfect(const DWORD state, const DWORD start_type, const bool trigger) {
  if (start_type == SERVICE_BOOT_START) return state == SERVICE_RUNNING;
  if (start_type == SERVICE_SYSTEM_START) return state == SERVICE_RUNNING;
  if (start_type == SERVICE_AUTO_START) {
    if (trigger) return true;
    return state == SERVICE_RUNNING;
  }
  if (start_type == SERVICE_DEMAND_START) return true;
  if (start_type == SERVICE_DISABLED) return state == SERVICE_STOPPED;
  return false;
}

bool check_state_is_ok(const DWORD state, const DWORD start_type, const bool delayed, const bool trigger, const DWORD exit_code) {
  if ((state == SERVICE_START_PENDING) && (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START))
    return true;
  if (delayed || trigger) {
    if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START) return true;
  }
  if (state == SERVICE_STOPPED && exit_code == 0) {
    if (start_type == SERVICE_BOOT_START || start_type == SERVICE_SYSTEM_START || start_type == SERVICE_AUTO_START) return true;
  }
  return check_state_is_perfect(state, start_type, trigger);
}

node_type state_is_ok(const value_type, const evaluation_context &raw_context, const node_type &subject) {
  const auto context = reinterpret_cast<native_context*>(raw_context.get());
  const DWORD state = context->get_object()->state;
  const DWORD start_type = context->get_object()->start_type;
  const bool delayed = context->get_object()->get_delayed() == 1;
  const bool trigger = context->get_object()->get_is_trigger() == 1;
  const DWORD exit_code = context->get_object()->exit_code;
  if (check_state_is_ok(state, start_type, delayed, trigger, exit_code))
    return factory::create_true();
  else
    return factory::create_false();
}

node_type state_is_perfect(const value_type, const evaluation_context &raw_context, const node_type &subject) {
  auto context = reinterpret_cast<native_context*>(raw_context.get());
  const DWORD state = context->get_object()->state;
  const DWORD start_type = context->get_object()->start_type;
  const bool trigger = context->get_object()->get_is_trigger() == 1;
  if (check_state_is_perfect(state, start_type, trigger))
    return factory::create_true();
  return factory::create_false();
}

node_type parse_state(boost::shared_ptr<filter_obj> /*object*/, const evaluation_context &context, const node_type &subject) {
  try {
    return factory::create_int(filter_obj::parse_state(subject->get_string_value(context)));
  } catch (const std::string& e) {
    context->error(e);
    return factory::create_false();
  }
}
node_type parse_start_type(boost::shared_ptr<filter_obj> /*object*/, const evaluation_context &context, const node_type &subject) {
  try {
    return factory::create_int(filter_obj::parse_start_type(subject->get_string_value(context)));
  } catch (const std::string& e) {
    context->error(e);
    return factory::create_false();
  }
}

filter_obj_handler::filter_obj_handler() {
  static constexpr value_type type_custom_state = type_custom_int_1;
  static constexpr value_type type_custom_start_type = type_custom_int_2;

  registry_.add_string("name", &filter_obj::get_name, "Service name")
      .add_string("desc", &filter_obj::get_desc, "Service description")
      .add_string("legacy_state", &filter_obj::get_legacy_state_s, "Get legacy state (deprecated and only used by check_nt)")
      .add_string("classification", &filter_obj::get_classification, "Get classification");
  registry_.add_int_x("pid", &filter_obj::get_pid, "Process id")
      .add_int_x("state", type_custom_state, &filter_obj::get_state_i, "The current state ()")
      .add_int_perf("", "")
      .add_int_x("start_type", type_custom_start_type, &filter_obj::get_start_type_i, "The configured start type ()")
      .add_int_x("delayed", type_bool, &filter_obj::get_delayed, "If the service is delayed")
      .add_int_x("is_trigger", type_bool, &filter_obj::get_is_trigger, "If the service is has associated triggers")
      .add_int_x("triggers", type_int, &filter_obj::get_triggers, "The number of associated triggers for this service")
      .add_int_x("exit_code", type_int, &filter_obj::get_exit_code, "The Win32 exit code of the service");

  // clang-format off
  registry_.add_int_fun()
    ("state_is_perfect", type_bool, &state_is_perfect, "Check if the state is ok, i.e. all running services are running")
    ("state_is_ok", type_bool, &state_is_ok, "Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)")
    ;
  // clang-format on

  registry_.add_human_string("state", &filter_obj::get_state_s, "The current state ()")
      .add_human_string("start_type", &filter_obj::get_start_type_s, "The configured start type ()");

  registry_.add_converter()(type_custom_state, &parse_state)(type_custom_start_type, &parse_start_type);
}
}  // namespace check_svc_filter
}


void service_checks::check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_svc_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> services, excludes;
  std::string type;
  std::string state;
  std::string computer;
  bool class_e = false, class_i = false, class_r = false, class_s = false, class_y = false, class_u = false;

  filter_type filter;
  filter_helper.add_options("not state_is_perfect()", "not state_is_ok()", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${crit_list}, delayed (${warn_list})", "${name}=${state}, exit=%(exit_code), type=%(start_type)", "${name}",
                           "%(status): No services found", "%(status): All %(count) service(s) are ok.");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("computer", po::value<std::string>(&computer), "The name of the remote computer to check")
    ("service", po::value<std::vector<std::string>>(&services), "The service to check, set this to * to check all services")
    ("exclude", po::value<std::vector<std::string>>(&excludes), "A list of services to ignore (mainly useful in combination with service=*)")
    ("type", po::value<std::string>(&type)->default_value("service"), "The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process")
    ("state", po::value<std::string>(&state)->default_value("all"), "The types of services to enumerate available states are active, inactive or all")
    ("only-essential", po::bool_switch(&class_e), "Set filter to classification = 'essential'")
    ("only-ignored", po::bool_switch(&class_i), "Set filter to classification = 'ignored'")
    ("only-role", po::bool_switch(&class_r), "Set filter to classification = 'role'")
    ("only-supporting", po::bool_switch(&class_s), "Set filter to classification = 'supporting'")
    ("only-system", po::bool_switch(&class_y), "Set filter to classification = 'system'")
    ("only-user", po::bool_switch(&class_u), "Set filter to classification = 'user'")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (class_e) filter_helper.append_all_filters("and", "classification = 'essential'");
  if (class_i) filter_helper.append_all_filters("and", "classification = 'ignored'");
  if (class_r) filter_helper.append_all_filters("and", "classification = 'role'");
  if (class_s) filter_helper.append_all_filters("and", "classification = 'supporting'");
  if (class_y) filter_helper.append_all_filters("and", "classification = 'system'");
  if (class_u) filter_helper.append_all_filters("and", "classification = 'user'");

  if (services.empty()) {
    services.emplace_back("*");
  } else {
    if (filter_helper.data.perf_config.empty()) filter_helper.data.perf_config = "extra(state)";
  }
  if (!filter_helper.build_filter(filter)) return;

  for (const std::string &service : services) {
    if (service == "*") {
      for (const win_list_services::service_info &info :
           win_list_services::enum_services(computer, win_list_services::parse_service_type(type), win_list_services::parse_service_state(state), excludes)) {
        if (std::find(excludes.begin(), excludes.end(), info.get_name()) != excludes.end() ||
            std::find(excludes.begin(), excludes.end(), info.get_desc()) != excludes.end())
          continue;
        boost::shared_ptr<win_list_services::service_info> record(new win_list_services::service_info(info));
        filter.match(record);
        if (filter.has_errors()) return nscapi::protobuf::functions::set_response_bad(*response, "Filter processing failed: " + filter.get_errors());
           }
    } else {
      try {
        win_list_services::service_info info = win_list_services::get_service_info(computer, service);
        boost::shared_ptr<win_list_services::service_info> record(new win_list_services::service_info(info));
        filter.match(record);
      } catch (const nsclient::nsclient_exception &e) {
        return nscapi::protobuf::functions::set_response_bad(*response, e.reason());
      }
    }
  }
  filter_helper.post_process(filter);
}
