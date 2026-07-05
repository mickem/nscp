// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/settings/object.hpp>
#include <nscapi/settings/proxy.hpp>
#include <string>

namespace nscapi {
namespace targets {
struct target_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  NSCAPI_EXPORT target_object(std::string alias, std::string path) : parent(alias, path) {}
  NSCAPI_EXPORT target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  NSCAPI_EXPORT std::string to_string() const;
  NSCAPI_EXPORT void set_address(std::string new_value) { set_property_string("address", new_value); }

  NSCAPI_EXPORT virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample);

  NSCAPI_EXPORT void add_ssl_keys(nscapi::settings_helper::path_extension root_path);

  NSCAPI_EXPORT virtual void translate(const std::string &key, const std::string &new_value);
};
}  // namespace targets
}  // namespace nscapi
