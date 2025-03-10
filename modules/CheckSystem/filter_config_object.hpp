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

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${type} > ${used}", "NSCA") {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_datas(std::string file_string);
  void set_data(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace legacy

namespace mem {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${type} > ${used}", "NSCA") {}

  filter_config_object(filters::legacy::filter_config_object &other) : parent(other.get_alias(), other.get_path()), filter(other.filter), data(other.data) {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace mem

namespace cpu {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${core}>${load}%", "NSCA") {}

  filter_config_object(filters::legacy::filter_config_object &other) : parent(other.get_alias(), other.get_path()), filter(other.filter), data(other.data) {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace cpu

namespace proc {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${exe}", "NSCA") {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace proc

}  // namespace filters