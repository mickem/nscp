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

#include <list>
#include <nscapi/dll_defines.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <settings/settings_interface.hpp>
#include <string>

namespace nscapi {
class NSCAPI_EXPORT settings_proxy : public settings_helper::settings_impl_interface {
  unsigned int plugin_id_;
  core_wrapper* core_;

 public:
  typedef boost::shared_ptr<settings_proxy> ptr;
  settings_proxy(unsigned int plugin_id, core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}

  static ptr create(unsigned int plugin_id, core_wrapper* core);

  typedef std::list<std::string> string_list;

  void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) override;
  void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue, bool advanced,
                    bool sample, bool sensitive) override;
  void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) override;
  void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) override;

  std::string get_string(std::string path, std::string key, std::string def) override;
  void set_string(std::string path, std::string key, std::string value) override;
  virtual int get_int(std::string path, std::string key, int def);
  virtual void set_int(std::string path, std::string key, int value);
  virtual bool get_bool(std::string path, std::string key, bool def);
  virtual void set_bool(std::string path, std::string key, bool value);
  string_list get_sections(std::string path) override;
  string_list get_keys(std::string path) override;
  std::string expand_path(std::string key) override;

  void remove_key(std::string path, std::string key) override;
  void remove_path(std::string path) override;

  void err(const char* file, int line, std::string message) override;
  void warn(const char* file, int line, std::string message) override;
  void info(const char* file, int line, std::string message) override;
  void debug(const char* file, int line, std::string message) override;
  void save(std::string context = "");
};
}  // namespace nscapi