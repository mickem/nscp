// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

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
    explicit key_values(std::string path);
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

  void get(const std::string &path, const std::string &key, const std::string &def) const;
  void get(const std::string &path, const std::string &key, const char *def) const;
  void get(const std::string &path, const std::string &key, const long long def) const;
  void get(const std::string &path, const std::string &key, const bool def) const;
  void list(const std::string &path, const bool recursive = false) const;

  void set(const std::string &path, const std::string &key, const std::string &value) const;
  void erase(const std::string &path, const std::string &key) const;
  std::string request() const;
  std::string &response() const;
  bool validate_response() const;
  std::list<key_values> get_query_key_response() const;
  std::string get_response_error() const;
  void save() const;
  void load() const;
  void reload() const;
};
}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi
