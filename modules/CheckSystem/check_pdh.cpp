/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should  have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "stdafx.h"
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
#include <parsers/filter/cli_helper.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

namespace check_pdh {

	void command_reader::init_default(object_type& object) {
		object.collection_strategy = "static";
		object.instances = "none";
		object.type = "large";
	}
	void command_reader::read_object(boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool oneliner, bool is_sample) {
		if (!object.tpl.value.empty())
			object.counter = object.tpl.value;
		std::string alias;

		nscapi::settings_helper::settings_registry settings(proxy);
		nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
		if (is_sample)
			root_path.set_sample();

		if (oneliner) {
			std::string::size_type pos = object.tpl.path.find_last_of("/");
			if (pos != std::string::npos) {
				std::string path = object.tpl.path.substr(0, pos);
				std::string key = object.tpl.path.substr(pos+1);
				proxy->register_key(path, key, NSCAPI::key_string, object.tpl.alias, "Counter " + object.tpl.alias + ". To configure this item add a section called: " + object.tpl.path, "", false, is_sample);
				proxy->set_string(path, key, object.tpl.value);
				return;
			}
		}

		root_path.add_path()
			("COUNTER", "Definition for counter: " + object.tpl.alias)
			;

		root_path.add_key()
			("collection strategy", sh::string_key(&object.collection_strategy),
			"COLLECTION STRATEGY", "The way to handled values when collecting them: static means we keep the last known value, rrd means we store values in a buffer from which you can retrieve the average")
			("counter", sh::string_key(&object.counter),
			"COUNTER", "The counter to check")
			("instances", sh::string_key(&object.instances),
			"TODO", "TODO")
			("buffer size", sh::string_key(&object.buffer_size),
			"BUFFER SIZE", "Size of buffer (in seconds) larger buffer use more memory")
			("type", sh::string_key(&object.type),
			"COUNTER TYPE", "The type of counter to use long, large and double")
			;

		object.tpl.read_object(root_path);

		settings.register_all();
		settings.notify();
		if (!alias.empty())
			object.tpl.alias = alias;
	}

	void command_reader::apply_parent(object_type &object, object_type &parent) {
		import_string(object.collection_strategy, parent.collection_strategy);
		import_string(object.buffer_size, parent.buffer_size);
		import_string(object.type, parent.type);
	}


	filter_obj_handler::filter_obj_handler() {
		registry_.add_string()
			("counter", boost::bind(&filter_obj::get_counter, _1), "The counter")
			;
		registry_.add_int()
			("value", boost::bind(&filter_obj::get_value, _1), "The value")
			;
	}

	void check::add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string path, std::string key, std::string query) {
		try {
			counters_.add(proxy, path, key, query, key == "default");
		} catch (const std::exception &e) {
			NSC_LOG_ERROR_EXR("Failed to add counter: " + key, e);
		} catch (...) {
			NSC_LOG_ERROR_EX("Failed to add counter: " + key);
		}
	}

	void check::check_pdh(pdh_thread &collector, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
		typedef filter filter_type;
		modern_filter::data_container data;
		modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
		std::vector<std::string> counters;
		bool expand_index = false;
		bool reload = false;
		bool check_average = false;
		std::string flags;
		std::string type;
		std::string time;

		filter_type filter;
		filter_helper.add_options(filter.get_filter_syntax(), "Everything looks good");
		filter_helper.add_syntax("${problem_list}", filter.get_format_syntax(), "${counter} = ${value}", "${counter}");
		filter_helper.get_desc().add_options()
			("counter", po::value<std::vector<std::string>>(&counters), "Performance counter to check")
			("expand-index", po::bool_switch(&expand_index), "Expand indexes in counter strings")
			("reload", po::bool_switch(&reload), "Reload counters on errors (useful to check counters which are not added at boot)")
			("averages", po::bool_switch(&check_average), "Check average values (ie. wait for 1 second to collecting two samples)")
			("time", po::value<std::string>(&time), "Timeframe to use for named rrd counters")
			("flags", po::value<std::string>(&flags), "Extra flags to configure the counter (nocap100, 1000, noscale)")
			("type", po::value<std::string>(&type)->default_value("long"), "Format of value (double, long, large)")
			;

		if (!filter_helper.parse_options())
			return;

		if (filter_helper.empty())
			return nscapi::protobuf::functions::set_response_bad(*response, "No checks specified add warn/crit boundries");

		if (counters.empty())
			return nscapi::protobuf::functions::set_response_bad(*response, "No counters specified: add counter=<name of counter>");

		if (!filter_helper.build_filter(filter))
			return;

		PDH::PDHQuery pdh;
		std::list<PDH::pdh_instance> free_counters;
		std::list<std::string> named_counters;

		bool has_counter = false;
		std::list<std::wstring> to_check;
		BOOST_FOREACH(std::string &counter, counters) {
			try {
				if (counter.find('\\') == std::string::npos) {
					named_counters.push_back(counter);
				} else {
					if (expand_index) {
						PDH::PDHResolver::expand_index(counter);
					}
					// 				std::wstring tstr;
					// 				if (!PDH::PDHResolver::validate(utf8::cvt<std::wstring>(counter), tstr, reload)) {
					// 					return nscapi::protobuf::functions::set_response_bad(*response, "Counter not found: " + counter);
					// 				}

					PDH::pdh_object obj;
					obj.set_flags(flags);
					obj.set_counter(counter);
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
		if (!free_counters.empty()) {
			try {
				pdh.open();
				if (check_average) {
					pdh.collect();
					Sleep(1000);
				}
				pdh.gatherData();
				pdh.close();
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("Failed to poll counter", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to poll counter: " + utf8::utf8_from_native(e.what()));
			}
		}
		BOOST_FOREACH(std::string &counter, named_counters) {
			try {
				typedef std::map<std::string,double> value_list_type;

				value_list_type values;
				if (time.empty()) {
					values = collector.get_value(counter);
				} else {
					values = collector.get_average(counter, strEx::stoui_as_time(time));
				}
				if (values.empty())
					return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value");
				BOOST_FOREACH(const value_list_type::value_type &v, values) {
					boost::shared_ptr<filter_obj> record(new filter_obj(v.first, v.second));
					boost::tuple<bool,bool> ret = filter.match(record);
					if (ret.get<1>()) {
						break;
					}
				}
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("ERROR", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
			}
		}
		BOOST_FOREACH(PDH::pdh_instance &instance, free_counters) {
			try {
				boost::shared_ptr<filter_obj> record(new filter_obj(instance->get_counter(), instance->get_int_value()));
				boost::tuple<bool,bool> ret = filter.match(record);
				if (ret.get<1>()) {
					break;
				}
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("ERROR", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value: " + utf8::utf8_from_native(e.what()));
			}
		}
		modern_filter::perf_writer scaler(response);
		filter_helper.post_process(filter, &scaler);
	}

}



