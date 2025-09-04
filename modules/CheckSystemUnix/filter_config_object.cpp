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

#include "filter_config_object.hpp"

#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <map>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <str/utils.hpp>
#include <string>

#include "filter.hpp"

namespace sh = nscapi::settings_helper;

namespace filters {
std::string filter_config_object::to_string() const {
  std::stringstream ss;
  ss << get_alias() << "[" << get_alias() << "] = "
     << "{tpl: " << parent::to_string() << ", filter: " << filter.to_string() << "}";
  return ss.str();
}

void filter_config_object::set_datas(std::string file_string) {
  if (file_string.empty()) return;
  data.clear();
  for (const std::string &s : str::utils::split_lst(file_string, std::string(","))) {
    data.push_back(s);
  }
}
void filter_config_object::set_data(std::string file_string) {
  if (file_string.empty()) return;
  data.clear();
  data.push_back(file_string);
}

void filter_config_object::read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
  if (!get_value().empty()) filter.set_filter_string(get_value().c_str());
  bool is_default = parent::is_default();

  nscapi::settings_helper::settings_registry settings(proxy);
  nscapi::settings_helper::path_extension root_path = settings.path(get_path());
  if (is_sample) root_path.set_sample();

  // add_oneliner_hint(proxy, oneliner, is_sample);

  root_path.add_path()("REAL TIME FILTER DEFENITION", "Definition for real time filter: " + get_alias());
  root_path.add_key().add_string("check", sh::string_key(&check, "cpu"), "TYPE OF CHECK", "The type of check cpu or memory", false);

  settings.register_all();
  settings.notify();

  if (check == "memory") {
    if (is_default) {
      // Populate default values!
      filter.syntax_top = "${list}";
      filter.syntax_detail = "${type} > ${used}";
    }

    root_path.add_key()
        .add_string("type", sh::string_fun_key([this](auto value) { this->set_data(value); }), "TIME", "The time to check", false)
        .add_string("types", sh::string_fun_key([this](auto value) { this->set_datas(value); }), "FILES", "A list of times to check (soma separated)", true);

  } else {
    if (is_default) {
      // Populate default values!
      filter.syntax_top = "${list}";
      filter.syntax_detail = "${core}>${load}%";
      filter.set_filter_string("core = 'total'");
    }

    root_path.add_key()
        .add_string("time", sh::string_fun_key([this](auto value) { this->set_data(value); }), "TIME", "The time to check", false)
        .add_string("times", sh::string_fun_key([this](auto value) { this->set_datas(value); }), "FILES", "A list of times to check (soma separated)", true);
  }

  filter.read_object(root_path, is_default);

  settings.register_all();
  settings.notify();
}

}  // namespace filters
