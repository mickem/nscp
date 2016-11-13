/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <map>
#include <string>

#include <boost/cstdint.hpp>
#include <boost/optional.hpp>
#include <boost/date_time.hpp>

#include <NSCAPI.h>

#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_object.hpp>
#include <nscapi/nscapi_settings_filter.hpp>

#include "filter.hpp"

namespace filters {
	namespace legacy {
		struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			nscapi::settings_filters::filter_object filter;
			std::string check;
			std::list<std::string> data;

			filter_config_object(std::string alias, std::string path)
				: parent(alias, path)
				, filter("${list}", "${type} > ${used}", "NSCA") {}

			void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

			std::string to_string() const;
			void set_datas(std::string file_string);
			void set_data(std::string file_string);
		};
		typedef boost::optional<filter_config_object> optional_filter_config_object;

		typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
	}

	namespace mem {
		struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			nscapi::settings_filters::filter_object filter;
			std::list<std::string> data;

			filter_config_object(std::string alias, std::string path)
				: parent(alias, path)
				, filter("${list}", "${type} > ${used}", "NSCA") {}

			filter_config_object(filters::legacy::filter_config_object &other)
				: parent(other.get_alias(), other.get_path())
				, filter(other.filter) 
				, data(other.data)
			{}

			void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

			std::string to_string() const;
			void set_data(std::string file_string);
		};
		typedef boost::optional<filter_config_object> optional_filter_config_object;

		typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
	}

	namespace cpu {
		struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			nscapi::settings_filters::filter_object filter;
			std::list<std::string> data;

			filter_config_object(std::string alias, std::string path)
				: parent(alias, path)
				, filter("${list}", "${core}>${load}%", "NSCA") {}

			filter_config_object(filters::legacy::filter_config_object &other)
				: parent(other.get_alias(), other.get_path())
				, filter(other.filter)
				, data(other.data) {}

			void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

			std::string to_string() const;
			void set_data(std::string file_string);
		};
		typedef boost::optional<filter_config_object> optional_filter_config_object;

		typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
	}

	namespace proc {
		struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
			typedef nscapi::settings_objects::object_instance_interface parent;

			nscapi::settings_filters::filter_object filter;
			std::list<std::string> data;

			filter_config_object(std::string alias, std::string path)
				: parent(alias, path)
				, filter("${list}", "${exe}", "NSCA") {}

			void read(boost::shared_ptr<nscapi::settings_proxy> proxy, bool oneliner, bool is_sample);

			std::string to_string() const;
			void set_data(std::string file_string);
		};
		typedef boost::optional<filter_config_object> optional_filter_config_object;

		typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
	}


}