// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <map>
#include <memory>
#include <nscapi/settings_proxy.hpp>
#include <settings/client/settings_client.hpp>
#include <string>

namespace settings_client {
struct target_handler {
  struct target {
    std::wstring host;
    std::wstring alias;
    std::wstring protocol;
    std::wstring parent;
    typedef std::map<std::wstring, std::wstring> options_type;
    options_type options;

    std::wstring to_wstring() {
      std::wstringstream ss;
      ss << _T("Target: ") << alias;
      ss << _T(", host: ") << host;
      ss << _T(", protocol: ") << protocol;
      ss << _T(", parent: ") << parent;
      for (options_type::value_type o : options) {
        ss << _T(", option[") << o.first << _T("]: ") << o.second;
      }
      return ss.str();
    }
  };
  typedef boost::optional<target> optarget;
  typedef std::map<std::wstring, target> target_list_type;

  target_list_type target_list;
  target_list_type template_list;
  void add(std::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring key, std::wstring value) {
    add(read_target(proxy, path, key, value));
  }
  void add(target t) { target_list[t.alias] = t; }
  void add_template(target t) { template_list[t.alias] = t; }
  target read_target(std::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring host);

  optarget find_target(std::wstring alias);
  static void apply_parent(target &t, target &p);
  std::wstring to_wstring();
};
}  // namespace settings_client
