// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <settings/client/settings_proxy.hpp>

void settings_client::settings_proxy::register_path(std::string path, std::string title, std::string description, bool advanced) {
  get_core()->register_path(0xffff, path, title, description, advanced);
}

virtual void settings_client::settings_proxy::register_key(std::string path, std::string key, std::string title, std::string description, std::string defValue,
                                                           bool advanced) {
  get_core()->register_key(0xffff, path, key, title, description, defValue, advanced);
}

virtual std::string settings_client::settings_proxy::get_string(std::string path, std::string key, std::string def) {
  return get_impl()->get_string(path, key, def);
}
virtual void settings_client::settings_proxy::set_string(std::string path, std::string key, std::string value) { get_impl()->set_string(path, key, value); }

virtual string_list settings_client::settings_proxy::get_sections(std::string path) { return get_impl()->get_sections(path); }
virtual string_list settings_client::settings_proxy::get_keys(std::string path) { return get_impl()->get_keys(path); }
virtual std::string settings_client::settings_proxy::expand_path(std::string key) { return get_handler()->expand_path(key); }

virtual void settings_client::settings_proxy::err(const char* file, int line, std::string message) {
  nsclient::logging::logger::get_logger()->error("settings", file, line, message);
}
virtual void settings_client::settings_proxy::warn(const char* file, int line, std::string message) {
  nsclient::logging::logger::get_logger()->warning("settings", file, line, message);
}
virtual void settings_client::settings_proxy::info(const char* file, int line, std::string message) {
  nsclient::logging::logger::get_logger()->info("settings", file, line, message);
}
virtual void settings_client::settings_proxy::debug(const char* file, int line, std::string message) {
  nsclient::logging::logger::get_logger()->debug("settings", file, line, message);
}
