// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time.hpp>
#include <boost/optional.hpp>
#include <list>
#include <nscapi/settings/filter.hpp>
#include <nscapi/settings/object.hpp>
#include <string>

namespace filters {

struct file_container {
  std::string file;
  boost::uintmax_t size;
};

namespace cpu {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${core}>${load}%", "NSCA") {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
  void set_datas(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;
typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace cpu

namespace mem {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${type} > ${used}", "NSCA") {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
  void set_datas(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;
typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace mem

namespace proc {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  // Optional list of executable names to watch; empty means all processes.
  std::list<std::string> data;

  filter_config_object(std::string alias, std::string path) : parent(alias, path), filter("${list}", "${exe}=${state}", "NSCA") {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  std::string to_string() const;
  void set_data(std::string file_string);
  void set_datas(std::string file_string);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;
typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace proc

}  // namespace filters
