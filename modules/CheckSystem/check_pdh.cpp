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

#include "module.hpp"
#include "CheckSystem.h"

#include <map>
#include <set>

#include <boost/regex.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>

#include <parsers/filter/cli_helper.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

void foo() {
	check_pdh::counter_config_object *a = new check_pdh::counter_config_object("", "");
}
namespace check_pdh {
	void counter_config_object::read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample) {
		parent::read(proxy, oneliner, is_sample);
		if (!get_value().empty())
			counter = get_value();

		if (oneliner)
			return;

 		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(get_path());
		if (is_sample)
			root_path.set_sample();

		root_path.add_path()
			("COUNTER", "Definition for counter: " + get_alias())
			;

		root_path.add_key()
			("collection strategy", sh::string_key(&collection_strategy),
				"COLLECTION STRATEGY", "The way to handled values when collecting them: static means we keep the last known value, rrd means we store values in a buffer from which you can retrieve the average")
			("counter", sh::string_key(&counter),
				"COUNTER", "The counter to check")
			("instances", sh::string_key(&instances),
				"Interpret instances", "IF we shoul interpret instance (default auto). Values: auto, true, false")
			("buffer size", sh::string_key(&buffer_size),
				"BUFFER SIZE", "Size of buffer (in seconds) larger buffer use more memory")
			("type", sh::string_key(&type),
				"COUNTER TYPE", "The type of counter to use long, large and double")
			("flags", sh::string_key(&flags),
				"FLAGS", "Extra flags to configure the counter (nocap100, 1000, noscale)")
			;

		settings.register_all();
		settings.notify();
	}

	filter_obj_handler::filter_obj_handler() {
		registry_.add_string()
			("counter", boost::bind(&filter_obj::get_counter, _1), "The counter name")
			("alias", boost::bind(&filter_obj::get_alias, _1), "The counter alias")
			("time", boost::bind(&filter_obj::get_time, _1), "The time for rrd checks")
			;

		registry_.add_number()
			("value", parsers::where::type_float, boost::bind(&filter_obj::get_value_i, _1), boost::bind(&filter_obj::get_value_f, _1), "The counter value (either float or int)")
			;
		registry_.add_int()
			("value_i", boost::bind(&filter_obj::get_value_i, _1), "The counter value (force int value)")
			;
		registry_.add_float()
			("value_f", boost::bind(&filter_obj::get_value_f, _1), "The counter value (force float value)")
			;

	}

	void check::clear() {
		counters_.clear();
	}
	void check::add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
		try {
			counters_.add(proxy, key, query);
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to add counter: " + key, e);
		} catch (...) {
			NSC_LOG_ERROR_EX("Failed to add counter: " + key);
		}
	}

	void check::check_pdh(boost::shared_ptr<pdh_thread> &collector, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
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

		filter_type filter;
		filter_helper.add_options("", "", "", filter.get_filter_syntax(), "unknown");
		filter_helper.add_syntax("${status}: ${list}", "${alias} = ${value}", "${alias}", "", "");
		filter_helper.get_desc().add_options()
			("counter", po::value<std::vector<std::string>>(&counters), "Performance counter to check")
			("expand-index", po::bool_switch(&expand_index), "Expand indexes in counter strings")
			("instances", po::bool_switch(&expand_instance), "Expand wildcards and fetch all instances")
			("reload", po::bool_switch(&reload), "Reload counters on errors (useful to check counters which are not added at boot)")
			("averages", po::bool_switch(&check_average), "Check average values (ie. wait for 1 second to collecting two samples)")
			("time", po::value<std::vector<std::string>>(&times), "Timeframe to use for named rrd counters")
			("flags", po::value<std::string>(&flags), "Extra flags to configure the counter (nocap100, 1000, noscale)")
			("type", po::value<std::string>(&type)->default_value("large"), "Format of value (double, long, large)")
			("ignore-errors", po::bool_switch(&ignore_errors), "If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors")
			;

		std::vector<std::string> extra;
		if (!filter_helper.parse_options(extra))
			return;

		if (filter_helper.empty() && data.syntax_top == "${problem_list}")
			data.syntax_top = "${list}";

		if (counters.empty() && extra.empty())
			return nscapi::protobuf::functions::set_response_bad(*response, "No counters specified: add counter=<name of counter>");

		if (!filter_helper.build_filter(filter))
			return;
		if (filter_helper.empty()) {
			filter.add_manual_perf("value");
		}

		if (times.empty())
			times.push_back("");
		else if (filter_helper.data.syntax_perf == "${alias}")
			filter_helper.data.syntax_perf = "%(alias)_%(time)";

		PDH::PDHQuery pdh;
		std::list<PDH::pdh_instance> free_counters;
		typedef std::map<std::string, std::string> counter_list;
		counter_list named_counters;

		bool has_counter = false;
		std::list<std::wstring> to_check;
		BOOST_FOREACH(std::string &counter, counters) {
			try {
				if (counter.find('\\') == std::string::npos) {
					named_counters[counter] = counter;
				} else {
					if (expand_index) {
						PDH::PDHResolver::expand_index(counter);
					}
					PDH::pdh_object obj;
					if (expand_instance)
						obj.set_instances("true");
					obj.set_flags(flags);
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
		BOOST_FOREACH(const std::string &s, extra) {
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
					obj.set_flags(flags);
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
		BOOST_FOREACH(const counter_list::value_type &vc, named_counters) {
			try {
				typedef std::map<std::string, double> value_list_type;

				BOOST_FOREACH(const std::string &time, times) {
					value_list_type values;
					if (time.empty()) {
						values = collector->get_value(vc.second);
					} else {
						values = collector->get_average(vc.second, str::format::stox_as_time_sec<long>(time, "s"));
					}
					if (values.empty())
						return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value");
					BOOST_FOREACH(const value_list_type::value_type &v, values) {
						boost::shared_ptr<filter_obj> record(new filter_obj(vc.first, v.first, time, v.second, v.second));
						modern_filter::match_result ret = filter.match(record);
					}
				}
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("ERROR", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
			}
		}
		BOOST_FOREACH(PDH::pdh_instance &instance, free_counters) {
			try {
				if (expand_instance) {
					BOOST_FOREACH(const PDH::pdh_instance &child, instance->get_instances()) {
						boost::shared_ptr<filter_obj> record(new filter_obj(child->get_name(), child->get_counter(), "", child->get_int_value(), child->get_float_value()));
						modern_filter::match_result ret = filter.match(record);
					}
				} else {
					boost::shared_ptr<filter_obj> record(new filter_obj(instance->get_name(), instance->get_counter(), "", instance->get_int_value(), instance->get_float_value()));
					modern_filter::match_result ret = filter.match(record);
				}
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("ERROR", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
			}
		}
		filter_helper.post_process(filter);
	}
}