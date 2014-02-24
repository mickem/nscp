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
			("counter", boost::bind(&filter_obj::get_counter, _1), "The counter name")
			("alias", boost::bind(&filter_obj::get_alias, _1), "The counter alias")
			;
		registry_.add_int()
			("value", boost::bind(&filter_obj::get_value, _1), "The counter value")
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
		bool expand_instance = false;
		std::string flags;
		std::string type;
		std::string time;

		filter_type filter;
		filter_helper.add_options(filter.get_filter_syntax(), "Everything looks good");
		filter_helper.add_syntax("${status}: ${problem_list}", filter.get_format_syntax(), "${counter} = ${value}", "${alias}");
		filter_helper.get_desc().add_options()
			("counter", po::value<std::vector<std::string>>(&counters), "Performance counter to check")
			("expand-index", po::bool_switch(&expand_index), "Expand indexes in counter strings")
			("instances", po::bool_switch(&expand_instance), "Expand wildcards and fetch all instances")
			("reload", po::bool_switch(&reload), "Reload counters on errors (useful to check counters which are not added at boot)")
			("averages", po::bool_switch(&check_average), "Check average values (ie. wait for 1 second to collecting two samples)")
			("time", po::value<std::string>(&time), "Timeframe to use for named rrd counters")
			("flags", po::value<std::string>(&flags), "Extra flags to configure the counter (nocap100, 1000, noscale)")
			("type", po::value<std::string>(&type)->default_value("large"), "Format of value (double, long, large)")
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

		PDH::PDHQuery pdh;
		std::list<PDH::pdh_instance> free_counters;
		typedef std::map<std::string,std::string> counter_list;
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
						obj.set_instances("*");
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
				if ((s.size() > 8) && (s.substr(0,8) == "counter:")) {
					std::string::size_type pos = s.find('=');
					if (pos != std::string::npos) {
						alias = s.substr(8,pos-8);
						counter = s.substr(pos+1);
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
				pdh.collect();
				pdh.gatherData(expand_instance);
				pdh.close();
			} catch (const PDH::pdh_exception &e) {
				NSC_LOG_ERROR_EXR("Failed to poll counter", e);
				return nscapi::protobuf::functions::set_response_bad(*response, "Failed to poll counter: " + utf8::utf8_from_native(e.what()));
			}
		}
		BOOST_FOREACH(const counter_list::value_type &vc, named_counters) {
			try {
				typedef std::map<std::string,double> value_list_type;

				value_list_type values;
				if (time.empty()) {
					values = collector.get_value(vc.second);
				} else {
					values = collector.get_average(vc.second, strEx::stoui_as_time(time)/1000);
				}
				if (values.empty())
					return nscapi::protobuf::functions::set_response_bad(*response, "Failed to get value");
				BOOST_FOREACH(const value_list_type::value_type &v, values) {
					boost::shared_ptr<filter_obj> record(new filter_obj(vc.first, v.first, v.second));
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
				if (expand_instance) {
					BOOST_FOREACH(const PDH::pdh_instance &child, instance->get_instances()) {
						boost::shared_ptr<filter_obj> record(new filter_obj(child->get_name(), child->get_counter(), child->get_int_value()));
						boost::tuple<bool,bool> ret = filter.match(record);
						if (ret.get<1>())
							break;
					}
				} else {
					boost::shared_ptr<filter_obj> record(new filter_obj(instance->get_name(), instance->get_counter(), instance->get_int_value()));
					boost::tuple<bool,bool> ret = filter.match(record);
					if (ret.get<1>())
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



