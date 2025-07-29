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

#include <boost/optional.hpp>
#include <nscapi/nscapi_protobuf_settings.hpp>
#include <nscapi/nscapi_protobuf_settings_functions.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

namespace nscapi {
namespace protobuf {
namespace functions {

struct settings_query_data {
  PB::Settings::SettingsRequestMessage request_message;
  PB::Settings::SettingsResponseMessage response_message;
  std::string response_buffer;
  int plugin_id;

  settings_query_data(int plugin_id) : plugin_id(plugin_id) {}
};

struct settings_query_key_values_data {
  std::string path;
  boost::optional<std::string> key;
  boost::optional<std::string> str_value;
  boost::optional<long long> int_value;
  boost::optional<bool> bool_value;

  settings_query_key_values_data(std::string path) : path(path) {}
  settings_query_key_values_data(std::string path, std::string key, std::string str_value) : path(path), key(key), str_value(str_value) {}
  settings_query_key_values_data(std::string path, std::string key, long long int_value) : path(path), key(key), int_value(int_value) {}
  settings_query_key_values_data(std::string path, std::string key, bool bool_value) : path(path), key(key), bool_value(bool_value) {}
  settings_query_key_values_data(const settings_query_key_values_data &other)
      : path(other.path), key(other.key), str_value(other.str_value), int_value(other.int_value), bool_value(other.bool_value) {}

  settings_query_key_values_data &operator=(const settings_query_key_values_data &other) {
    path = other.path;
    key = other.key;
    str_value = other.str_value;
    int_value = other.int_value;
    bool_value = other.bool_value;
    return *this;
  }
};

settings_query::key_values::key_values(std::string path) : pimpl(new settings_query_key_values_data(path)) {}
settings_query::key_values::key_values(std::string path, std::string key, std::string str_value)
    : pimpl(new settings_query_key_values_data(path, key, str_value)) {}
settings_query::key_values::key_values(std::string path, std::string key, long long int_value)
    : pimpl(new settings_query_key_values_data(path, key, int_value)) {}
settings_query::key_values::key_values(std::string path, std::string key, bool bool_value) : pimpl(new settings_query_key_values_data(path, key, bool_value)) {}

settings_query::key_values::key_values(const key_values &other) : pimpl(new settings_query_key_values_data(*other.pimpl)) {}
settings_query::key_values &settings_query::key_values::operator=(const key_values &other) {
  pimpl->operator=(*other.pimpl);
  return *this;
}

settings_query::key_values::~key_values() { delete pimpl; }

bool settings_query::key_values::matches(const char *path, const char *key) const {
  if (!pimpl || !pimpl->key) return false;
  return pimpl->path == path && *pimpl->key == key;
}
bool settings_query::key_values::matches(const char *path) const {
  if (!pimpl->key) return false;
  return pimpl->path == path;
}
bool settings_query::key_values::matches(const std::string &path, const std::string &key) const {
  if (!pimpl || !pimpl->key) return false;
  return pimpl->path == path && *pimpl->key == key;
}
bool settings_query::key_values::matches(const std::string &path) const {
  if (!pimpl) return false;
  return pimpl->path == path;
}

std::string settings_query::key_values::path() const {
  if (!pimpl) return "";
  return pimpl->path;
}
std::string settings_query::key_values::key() const {
  if (!pimpl || !pimpl->key) return "";
  return *pimpl->key;
}

std::string settings_query::key_values::get_string() const {
  if (pimpl->str_value) return *pimpl->str_value;
  if (pimpl->int_value) return str::xtos(*pimpl->int_value);
  if (pimpl->bool_value) return *pimpl->bool_value ? "true" : "false";
  return "";
}

long long settings_query::key_values::get_int() const {
  if (pimpl->str_value) return str::stox<long long>(*pimpl->str_value);
  if (pimpl->int_value) return *pimpl->int_value;
  if (pimpl->bool_value) return *pimpl->bool_value ? 1 : 0;
  return 0;
}

bool settings_query::key_values::get_bool() const {
  if (pimpl->str_value) {
    std::string s = *pimpl->str_value;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s == "true" || s == "1";
  }
  if (pimpl->int_value) return *pimpl->int_value == 1;
  if (pimpl->bool_value) return *pimpl->bool_value;
  return "";
}

settings_query::settings_query(int plugin_id) : pimpl(new settings_query_data(plugin_id)) {}
settings_query::~settings_query() { delete pimpl; }

void settings_query::set(const std::string path, const std::string key, const std::string value) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_update()->mutable_node()->set_path(path);
  r->mutable_update()->mutable_node()->set_key(key);
  r->mutable_update()->mutable_node()->set_value(value);
}

void settings_query::erase(const std::string path, const std::string key) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_update()->mutable_node()->set_path(path);
  r->mutable_update()->mutable_node()->set_key(key);
}

void settings_query::get(const std::string path, const std::string key, const std::string def) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_query()->mutable_node()->set_path(path);
  r->mutable_query()->mutable_node()->set_key(key);
  r->mutable_query()->set_default_value(def);
  r->mutable_query()->set_recursive(false);
}
void settings_query::get(const std::string path, const std::string key, const char *def) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_query()->mutable_node()->set_path(path);
  r->mutable_query()->mutable_node()->set_key(key);
  r->mutable_query()->set_default_value(def);
  r->mutable_query()->set_recursive(false);
}
void settings_query::get(const std::string path, const std::string key, const long long def) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_query()->mutable_node()->set_path(path);
  r->mutable_query()->mutable_node()->set_key(key);
  r->mutable_query()->set_default_value(str::xtos(def));
  r->mutable_query()->set_recursive(false);
}
void settings_query::get(const std::string path, const std::string key, const bool def) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_query()->mutable_node()->set_path(path);
  r->mutable_query()->mutable_node()->set_key(key);
  r->mutable_query()->set_default_value(def ? "true" : "false");
  r->mutable_query()->set_recursive(false);
}

void settings_query::list(const std::string path, const bool recursive) {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_inventory()->mutable_node()->set_path(path);
  r->mutable_inventory()->set_fetch_keys(true);
  r->mutable_inventory()->set_recursive_fetch(recursive);
}

void settings_query::save() {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_control()->set_command(PB::Settings::Command::SAVE);
}
void settings_query::load() {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_control()->set_command(PB::Settings::Command::LOAD);
}
void settings_query::reload() {
  PB::Settings::SettingsRequestMessage::Request *r = pimpl->request_message.add_payload();
  r->set_plugin_id(pimpl->plugin_id);
  r->mutable_control()->set_command(PB::Settings::Command::RELOAD);
}
const std::string settings_query::request() const { return pimpl->request_message.SerializeAsString(); }

bool settings_query::validate_response() const {
  pimpl->response_message.ParsePartialFromString(pimpl->response_buffer);
  bool ret = true;
  for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
    if (pimpl->response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) ret = false;
  }
  return ret;
}
std::string settings_query::get_response_error() const {
  std::string ret;
  for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
    ret += pimpl->response_message.payload(i).result().message();
  }
  return ret;
}
std::list<settings_query::key_values> settings_query::get_query_key_response() const {
  std::list<key_values> ret;
  for (int i = 0; i < pimpl->response_message.payload_size(); ++i) {
    PB::Settings::SettingsResponseMessage::Response pl = pimpl->response_message.payload(i);
    if (pl.has_query()) {
      PB::Settings::SettingsResponseMessage::Response::Query q = pl.query();
      if (!q.node().key().empty()) {
        ret.push_back(key_values(q.node().path(), q.node().key(), q.node().value()));
      } else {
        ret.push_back(key_values(q.node().path()));
      }
    } else if (pl.inventory_size() > 0) {
      for (const PB::Settings::SettingsResponseMessage::Response::Inventory &q : pl.inventory()) {
        if (!q.node().key().empty()) {
          ret.push_back(key_values(q.node().path(), q.node().key(), q.node().value()));
        } else if (!q.node().key().empty() && q.has_info() && !q.info().default_value().empty()) {
          ret.push_back(key_values(q.node().path(), q.node().key(), q.info().default_value()));
        } else {
          ret.push_back(key_values(q.node().path()));
        }
      }
    }
  }
  return ret;
}

std::string &settings_query::response() const { return pimpl->response_buffer; }
}  // namespace functions
}  // namespace protobuf
}  // namespace nscapi