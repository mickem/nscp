// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <nscapi/settings/filter.hpp>
#include <nscapi/settings/object.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utils.hpp>
#include <string>

#include "filter.hpp"

namespace eventlog_filter {
struct filter_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  filter_config_object(std::string alias, std::string path)
      : parent(alias, path), filter("${file}: ${count} (${list})", "${level}: ${message}", "NSCA"), dwLang(0), truncate_(0) {}

  nscapi::settings_filters::filter_object filter;
  DWORD dwLang;
  int truncate_;
  std::list<std::string> files;

  std::string to_string() const;
  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  static unsigned short get_language(std::string lang);

  void set_files(std::string file_string) {
    if (file_string.empty()) return;
    files.clear();
    for (const std::string &s : str::utils::split_lst(file_string, std::string(","))) {
      files.push_back(s);
    }
  }
  void set_file(std::string file_string) {
    if (file_string.empty()) return;
    files.clear();
    files.push_back(file_string);
  }

  void set_truncate(int truncate) { truncate_ = truncate; }
  int get_truncate() { return truncate_; }
  void set_language(std::string lang) {
    WORD wLang = get_language(lang);
    if (wLang == LANG_NEUTRAL)
      dwLang = MAKELANGID(wLang, SUBLANG_DEFAULT);
    else
      dwLang = MAKELANGID(wLang, SUBLANG_NEUTRAL);
  }
};
typedef boost::optional<filter_config_object> optional_filter_config_object;

typedef nscapi::settings_objects::object_handler<filter_config_object> filter_config_handler;
}  // namespace eventlog_filter
