/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <map>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>

#include "CheckSystem.h"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

namespace check_pdh {
void counter_config_object::read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
  parent::read(proxy, oneliner, is_sample);
  if (!get_value().empty()) counter = get_value();

  if (oneliner) return;

  nscapi::settings_helper::settings_registry settings(proxy);
  nscapi::settings_helper::path_extension root_path = settings.path(get_path());
  if (is_sample) root_path.set_sample();

  root_path.add_path()("COUNTER", "Definition for counter: " + get_alias());

  root_path.add_key()
      .add_string("collection strategy", sh::string_key(&collection_strategy), "COLLECTION STRATEGY",
                  "The way to handled values when collecting them: static means we keep the last known value, rrd means we store values in a buffer from which "
                  "you can retrieve the average")
      .add_string("counter", sh::string_key(&counter), "COUNTER", "The counter to check")
      .add_string("instances", sh::string_key(&instances), "Interpret instances", "IF we shoul interpret instance (default auto). Values: auto, true, false")
      .add_string("buffer size", sh::string_key(&buffer_size), "BUFFER SIZE", "Size of buffer (in seconds) larger buffer use more memory")
      .add_string("type", sh::string_key(&type), "COUNTER TYPE", "The type of counter to use long, large and double")
      .add_string("flags", sh::string_key(&flags), "FLAGS", "Extra flags to configure the counter (nocap100, 1000, noscale)");

  settings.register_all();
  settings.notify();
}

namespace {
// Free-standing function callbacks invoked from where-expressions and from
// `%(...)` placeholders in detail-syntax. Resolves issue #281 (PDH counter
// scaling) by exposing format / scale helpers in the expression grammar.
//
// Usage examples (all valid in both filter expressions and detail-syntax):
//   detail-syntax = "%(alias) = %(format_bytes(value))"          -> "Bytes/sec = 4.2MB"
//   detail-syntax = "%(alias) = %(format_bytes(value, 'MB'))"    -> "Bytes/sec = 4.2M"
//   warning       = "convert_bytes(value, 'MB') > 100"           -> compare in MB
//   detail-syntax = "%(scale(value, 1000000)) Mbps"              -> decimal scaling

using parsers::where::evaluation_context;
using parsers::where::factory;
using parsers::where::node_type;
using parsers::where::value_type;

node_type fn_format_bytes(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.empty() || args.size() > 2) {
      context->error("format_bytes expects 1 or 2 arguments: format_bytes(value) or format_bytes(value, unit)");
      return factory::create_false();
    }
    auto it = args.begin();
    const long long v = (*it)->get_int_value(context);
    if (args.size() == 1) {
      return factory::create_string(str::format::format_byte_units(v));
    }
    ++it;
    const std::string unit = (*it)->get_string_value(context);
    return factory::create_string(str::format::format_byte_units(v, unit));
  } catch (const std::exception &e) {
    context->error(std::string("format_bytes failed: ") + e.what());
    return factory::create_false();
  }
}

node_type fn_convert_bytes(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.size() != 2) {
      context->error("convert_bytes expects 2 arguments: convert_bytes(value, unit)");
      return factory::create_false();
    }
    auto it = args.begin();
    const long long v = (*it)->get_int_value(context);
    ++it;
    const std::string unit = (*it)->get_string_value(context);
    return factory::create_float(str::format::convert_to_byte_units(v, unit));
  } catch (const std::exception &e) {
    context->error(std::string("convert_bytes failed: ") + e.what());
    return factory::create_false();
  }
}

node_type fn_scale(value_type, evaluation_context context, const node_type subject) {
  try {
    const std::list<node_type> args = subject->get_list_value(context);
    if (args.size() != 2) {
      context->error("scale expects 2 arguments: scale(value, divisor)");
      return factory::create_false();
    }
    auto it = args.begin();
    const double v = (*it)->get_float_value(context);
    ++it;
    const double divisor = (*it)->get_float_value(context);
    if (divisor == 0.0) {
      context->error("scale: divisor must be non-zero");
      return factory::create_false();
    }
    return factory::create_float(v / divisor);
  } catch (const std::exception &e) {
    context->error(std::string("scale failed: ") + e.what());
    return factory::create_false();
  }
}
}  // namespace

filter_obj_handler::filter_obj_handler() {
  registry_.add_string_var("counter", &filter_obj::get_counter, "The counter name")
      .add_string_var("alias", &filter_obj::get_alias, "The counter alias")
      .add_string_var("time", &filter_obj::get_time, "The time for rrd checks");

  registry_.add_numbers("value", parsers::where::type_float, &filter_obj::get_value_i, &filter_obj::get_value_f, "The counter value (either float or int)");
  registry_.add_int_var("value_i", &filter_obj::get_value_i, "The counter value (force int value)");
  registry_.add_float("value_f", &filter_obj::get_value_f, "The counter value (force float value)");

  // Pre-scaled / pre-formatted views of the same value. Convenient shortcuts
  // for the common units; for arbitrary scaling use the scale() / format_bytes()
  // functions registered below.
  registry_
      .add_string_var(
          "value_human", [](auto obj) { return str::format::format_byte_units(obj->get_value_i()); },
          "Counter value formatted as a human-readable byte string, auto-scaled to B/KB/MB/GB/...")
      .add_float(
          "value_kb", [](auto obj) { return obj->get_value_f() / 1024.0; }, "Counter value in KB (1024-based).")
      .add_float(
          "value_mb", [](auto obj) { return obj->get_value_f() / (1024.0 * 1024.0); }, "Counter value in MB (1024-based).")
      .add_float("value_gb", [](auto obj) { return obj->get_value_f() / (1024.0 * 1024.0 * 1024.0); }, "Counter value in GB (1024-based).");

  // General-purpose format and scaling functions.
  registry_
      .add_string_fun("format_bytes", &fn_format_bytes,
                      "Format a number as a human-readable byte string.\n"
                      "Syntax: format_bytes(value) auto-scales to B/KB/MB/GB/... (1024-based).\n"
                      "        format_bytes(value, unit) formats to a fixed unit (\"B\", \"K\"/\"KB\", \"M\"/\"MB\", \"G\"/\"GB\", \"T\"/\"TB\").")
      .add_custom_fun("convert_bytes", parsers::where::type_float, &fn_convert_bytes,
                      "Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.\n"
                      "Syntax: convert_bytes(value, unit) where unit is \"B\", \"K\"/\"KB\", \"M\"/\"MB\", \"G\"/\"GB\", \"T\"/\"TB\".")
      .add_custom_fun("scale", parsers::where::type_float, &fn_scale,
                      "Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)).\n"
                      "Syntax: scale(value, divisor).");
}

void check::clear() { counters_.clear(); }
void check::add_counter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    counters_.add(proxy, key, query);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add counter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add counter: " + key);
  }
}

void check::add_rrd_counter(std::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
  try {
    auto instance = counters_.add(proxy, key, query);
    instance->collection_strategy = "rrd";
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add counter: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add counter: " + key);
  }
}

void check::check_pdh(std::shared_ptr<pdh_thread> &collector, const PB::Commands::QueryRequestMessage::Request &request,
                      PB::Commands::QueryResponseMessage::Response *response) {
  typedef filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> counters;
  std::vector<std::string> times;
  bool expand_index = false;
  bool reload = false;
  bool check_average = false;
  bool expand_instance = false;
  bool ignore_errors = false;
  std::string flags;
  std::string type;

  // clang-format off
  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${alias} = ${value}", "${alias}", "", "");
  filter_helper.get_desc().add_options()
    ("counter", po::value<std::vector<std::string>>(&counters), "Performance counter to check")
    ("expand-index", po::value<bool>(&expand_index)->implicit_value(true)->default_value(false), "Expand indexes in counter strings")
    ("instances", po::value<bool>(&expand_instance)->implicit_value(true)->default_value(false), "Expand wildcards and fetch all instances")
    ("reload", po::value<bool>(&reload)->implicit_value(true)->default_value(false), "Reload counters on errors (useful to check counters which are not added at boot)")
    ("averages", po::value<bool>(&check_average)->implicit_value(true)->default_value(false), "Check average values (ie. wait for 1 second to collecting two samples)")
    ("time", po::value<std::vector<std::string>>(&times), "Timeframe to use for named rrd counters")
    ("flags", po::value<std::string>(&flags), "Extra flags to configure the counter (nocap100, 1000, noscale)")
    ("type", po::value<std::string>(&type)->default_value("large"), "Format of value (double, long, large)")
    ("ignore-errors", po::value<bool>(&ignore_errors)->implicit_value(true)->default_value(false), "If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors")
  ;
  // clang-format on

  std::vector<std::string> extra;
  if (!filter_helper.parse_options(extra)) return;

  if (filter_helper.empty() && data.syntax_top == "${problem_list}") data.syntax_top = "${list}";

  if (counters.empty() && extra.empty())
    return nscapi::protobuf::functions::set_response_bad(*response, "No counters specified: add counter=<name of counter>");

  if (times.empty())
    times.push_back("");
  else if (times.size() > 1) {
    if (filter_helper.data.syntax_perf == "${alias}") filter_helper.data.syntax_perf = "%(alias)_%(time)";
    if (filter_helper.data.syntax_detail == "${alias} = ${value}") filter_helper.data.syntax_detail = "%(alias) %(time) = %(value)";
  }

  if (!filter_helper.build_filter(filter)) return;
  if (filter_helper.empty()) {
    filter.add_manual_perf("value");
  }

  PDH::PDHQuery pdh;
  std::list<PDH::pdh_instance> free_counters;
  typedef std::map<std::string, std::string> counter_list;
  counter_list named_counters;

  bool has_counter = false;
  std::list<std::wstring> to_check;
  for (std::string &counter : counters) {
    try {
      if (counter.find('\\') == std::string::npos) {
        named_counters[counter] = counter;
      } else {
        if (expand_index) {
          PDH::PDHResolver::expand_index(counter);
        }
        PDH::pdh_object obj;
        if (expand_instance) obj.set_instances("true");
        obj.add_flags(flags);
        obj.set_counter(counter);
        obj.set_alias(counter);
        obj.set_strategy_static();
        obj.set_type(type);
        PDH::pdh_instance instance = PDH::factory::create(obj);
        pdh.addCounter(instance);
        free_counters.push_back(instance);
        has_counter = true;
      }
    } catch (const std::exception &e) {
      NSC_LOG_ERROR_EXR("Failed to poll counter", e);
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to add counter: " + utf8::utf8_from_native(e.what()));
    }
  }
  for (const std::string &s : extra) {
    try {
      std::string counter, alias;
      if ((s.size() > 8) && (s.substr(0, 8) == "counter:")) {
        std::string::size_type pos = s.find('=');
        if (pos != std::string::npos) {
          alias = s.substr(8, pos - 8);
          counter = s.substr(pos + 1);
        } else
          return nscapi::protobuf::functions::set_response_bad(*response, "Invalid option: " + s);
      } else
        return nscapi::protobuf::functions::set_response_bad(*response, "Invalid option: " + s);
      if (counter.find('\\') == std::string::npos) {
        named_counters[counter] = counter;
      } else {
        if (expand_index) {
          PDH::PDHResolver::expand_index(counter);
        }
        PDH::pdh_object obj;
        obj.add_flags(flags);
        obj.set_counter(counter);
        obj.set_strategy_static();
        obj.set_type(type);
        obj.set_alias(alias);
        PDH::pdh_instance instance = PDH::factory::create(obj);
        pdh.addCounter(instance);
        free_counters.push_back(instance);
        has_counter = true;
      }
    } catch (const std::exception &e) {
      if (!ignore_errors) {
        NSC_LOG_ERROR_EXR("Failed to poll counter", e);
        return nscapi::protobuf::functions::set_response_bad(*response, "Failed to add counter: " + utf8::utf8_from_native(e.what()));
      } else {
        NSC_DEBUG_MSG_STD("Ignoring counter failure: " + utf8::utf8_from_native(e.what()));
      }
    }
  }
  if (!free_counters.empty()) {
    try {
      pdh.open();
      if (check_average) {
        pdh.collect();
        Sleep(1000);
      }
      pdh.gatherData(expand_instance);
      pdh.close();
    } catch (const PDH::pdh_exception &e) {
      if (!ignore_errors) {
        NSC_LOG_ERROR_EXR("Failed to poll counter", e);
        return nscapi::protobuf::functions::set_response_bad(*response, "Failed to add counter: " + utf8::utf8_from_native(e.what()));
      } else {
        NSC_DEBUG_MSG_STD("Ignoring counter failure: " + utf8::utf8_from_native(e.what()));
      }
    }
  }
  for (const counter_list::value_type &vc : named_counters) {
    try {
      typedef std::map<std::string, double> value_list_type;

      for (const std::string &time : times) {
        value_list_type values;
        if (time.empty()) {
          values = collector->get_value(vc.second);
        } else {
          values = collector->get_average(vc.second, str::format::stox_as_time_sec<long>(time, "s"));
        }
        if (values.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value");
        for (const value_list_type::value_type &v : values) {
          std::shared_ptr<filter_obj> record(new filter_obj(vc.first, v.first, time, static_cast<long long>(v.second), v.second));
          modern_filter::match_result ret = filter.match(record);
        }
      }
    } catch (const PDH::pdh_exception &e) {
      NSC_LOG_ERROR_EXR("ERROR", e);
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      return nscapi::protobuf::functions::set_response_bad(*response, std::string("Invalid argument: ") + e.what());
    }
  }
  for (PDH::pdh_instance &instance : free_counters) {
    try {
      if (expand_instance) {
        for (const PDH::pdh_instance &child : instance->get_instances()) {
          std::shared_ptr<filter_obj> record(new filter_obj(child->get_name(), child->get_counter(), "", child->get_int_value(), child->get_float_value()));
          modern_filter::match_result ret = filter.match(record);
        }
      } else {
        std::shared_ptr<filter_obj> record(
            new filter_obj(instance->get_name(), instance->get_counter(), "", instance->get_int_value(), instance->get_float_value()));
        modern_filter::match_result ret = filter.match(record);
      }
    } catch (const PDH::pdh_exception &e) {
      NSC_LOG_ERROR_EXR("ERROR", e);
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
    }
  }
  filter_helper.post_process(filter);
}
}  // namespace check_pdh
