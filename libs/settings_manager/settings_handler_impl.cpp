// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "settings_handler_impl.hpp"

#include "settings_manager_impl.h"

settings::instance_ptr settings::settings_handler_impl::get() {
  boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");
  if (!instance_) throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
  return instance_ptr(instance_);
}

settings::instance_ptr settings::settings_handler_impl::get_no_wait() {
  boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::try_to_lock);
  if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");
  if (!instance_) throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
  return instance_;
}

void settings::settings_handler_impl::update_defaults(bool include_samples) {
  for (const std::string &path : get_reg_sections("", include_samples)) {
    get()->add_path(path);
    for (const std::string &key : get_reg_keys(path, include_samples)) {
      auto desc = get_registered_key(path, key);
      auto advanced = desc.has_value() && desc.value().advanced;
      auto default_value = desc.has_value() ? desc.value().default_value : "";
      if (!advanced) {
        if (!get()->has_key(path, key)) {
          get_logger()->debug("settings", __FILE__, __LINE__, "Adding: " + key_to_string(path, key));
          get()->set_string(path, key, default_value);
        } else {
          settings_interface::op_string val = get()->get_string(path, key);
          if (val) {
            get_logger()->debug("settings", __FILE__, __LINE__, "Setting old (already exists): " + key_to_string(path, key));
            get()->set_string(path, key, *val);
          }
        }
      } else {
        get_logger()->debug("settings", __FILE__, __LINE__, "Skipping (advanced): " + key_to_string(path, key));
      }
    }
  }
}

void settings::settings_handler_impl::remove_defaults() {
  // Samples are included so this is the inverse of update_defaults(true): a
  // sample section written by --use-samples holds only default-valued keys and
  // is removed again here; one the operator has edited differs from the
  // defaults and is kept, like any other key.
  for (std::string path : get_reg_sections("", true)) {
    for (std::string key : get_reg_keys(path, true)) {
      auto desc = get_registered_key(path, key);
      auto default_value = desc.has_value() ? desc.value().default_value : "";
      if (get()->has_key(path, key)) {
        try {
          if (get()->get_string(path, key) == default_value) {
            get()->remove_key(path, key);
          }
        } catch (const std::exception &) {
          get_logger()->error("settings", __FILE__, __LINE__, "invalid default value for: " + key_to_string(path, key));
        }
      }
    }
    if (get()->get_keys(path).size() == 0 && get()->get_sections(path).size() == 0) {
      get()->remove_path(path);
    }
  }
}

void settings::settings_handler_impl::destroy_all_instances() {
  boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "destroy_all_instances Failed to get mutex, cant get access settings");
  instance_.reset();
}

void settings::settings_handler_impl::house_keeping() {
  boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "house_keeping Failed to get mutex, cant get access settings");
  // The scheduler calls this on a timer (scheduler::handle_settings), so it
  // can fire before boot() installs an instance or after shutdown destroyed
  // it. Dereferencing then is a null crash; the scheduler thread catches
  // exceptions, so report it the way every sibling here does.
  if (!instance_) throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
  instance_->house_keeping();
}

settings::error_list settings::settings_handler_impl::validate() { return get()->validate(); }

bool settings::settings_handler_impl::supports_updates() {
  const boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "Failed to get mutex, cant get settings instance");
  if (!instance_) throw settings_exception(__FILE__, __LINE__, "Failed initialize settings instance");
  return instance_->supports_updates();
}
bool settings::settings_handler_impl::use_sensitive_keys() {
  return nscapi::settings::settings_value::to_bool(get()->get_string("/settings", "use credential manager", "false"));
}
