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

#pragma once

#include <nscapi/nscapi_protobuf.hpp>

#include <nscapi/nscapi_settings_object.hpp>

#include <parsers/where.hpp>
#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace check_pdh {
	struct counter_config_object : public nscapi::settings_objects::object_instance_interface {
		typedef nscapi::settings_objects::object_instance_interface parent;

		bool debug;
		std::string collection_strategy;
		std::string counter;
		std::string instances;
		std::string buffer_size;
		std::string type;
		std::string flags;

		counter_config_object(std::string alias, std::string path)
			: parent(alias, path)
			, collection_strategy("static")
			, instances("auto")
			, type("double") {}

		// Runtime items

		void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

		std::string to_string() const {
			std::stringstream ss;
			ss << parent::to_string() << "{counter: " << counter << ", " << collection_strategy << ", " << type << "}";
			return ss.str();
		}
	};

	typedef nscapi::settings_objects::object_handler<counter_config_object> counter_config_handler;

	struct filter_obj {
		std::string alias;
		std::string counter;
		std::string time;
		long long value_i;
		double value_f;

		filter_obj(std::string alias, std::string counter, std::string time, long long value_i, double value_f) : alias(alias), counter(counter), time(time), value_i(value_i), value_f(value_f) {}

		long long get_value_i() const {
			return value_i;
		}
		double get_value_f() const {
			return value_f;
		}
		std::string get_counter() const {
			return counter;
		}
		std::string get_alias() const {
			return alias;
		}
		std::string get_time() const {
			return time;
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		filter_obj_handler();
	};

	typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

	struct check {
		counter_config_handler counters_;
		void check_pdh(boost::shared_ptr<pdh_thread> &collector, const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
		void add_counter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query);
		void clear();
	};
}