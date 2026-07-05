// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <nscapi/settings/filter.hpp>
#include <nscapi/settings/object.hpp>
#include <string>

#include "filter.hpp"

namespace filters {
struct file_container {
  std::string file;
  boost::uintmax_t size;
};

struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  nscapi::settings_filters::filter_object filter;
  std::string column_split;
  std::string line_split;
  std::list<std::string> files;
  bool read_from_start;

  filter_config_object(std::string alias, std::string path)
      : parent(alias, path), filter("${file}: ${count} (${list})", "${column1}, ${column2}, ${column3}", "NSCA"), column_split("\\t"), read_from_start(false) {}

  std::string to_string() const;
  void set_files(std::string file_string);
  void set_file(std::string file_string);

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace filters