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
#include <string>

namespace nscapi {
namespace protobuf {
namespace functions {

typedef int nagiosReturn;
struct settings_query_data;
struct settings_query_key_values_data;
class NSCAPI_EXPORT settings_query {
  settings_query_data *pimpl;

 public:
  struct NSCAPI_EXPORT key_values {
    settings_query_key_values_data *pimpl;
    key_values(std::string path);
    key_values(std::string path, std::string key, std::string str_value);
    key_values(std::string path, std::string key, long long int_value);
    key_values(std::string path, std::string key, bool bool_value);
    key_values(const key_values &other);
    key_values &operator=(const key_values &other);
    ~key_values();
    bool matches(const char *path) const;
    bool matches(const char *path, const char *key) const;
    bool matches(const std::string &path) const;
    bool matches(const std::string &path, const std::string &key) const;
    std::string get_string() const;
    std::string path() const;
    std::string key() const;
    bool get_bool() const;
    long long get_int() const;
  };
  settings_query(int plugin_id);
  ~settings_query();

  void get(const std::string path, const std::string key, const std::string def);
  void get(const std::string path, const std::string key, const char *def);
  void get(const std::string path, const std::string key, const long long def);
  void get(const std::string path, const std::string key, const bool def);
  void list(const std::string path, const bool recursive = false);

  void set(const std::string path, const std::string key, std::string value);
  void erase(const std::string path, const std::string key);
  const std::string request() const;
  std::string &response() const;
  bool validate_response() const;
  std::list<key_values> get_query_key_response() const;
  std::string get_response_error() const;
  void save();
  void load();
  void reload();
};
}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
