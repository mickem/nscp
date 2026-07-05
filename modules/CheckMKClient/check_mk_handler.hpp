// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <net/check_mk/client/client_protocol.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/settings/helper.hpp>

namespace check_mk_handler {
namespace sh = nscapi::settings_helper;

struct check_mk_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  check_mk_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_int("retries", 3);
    set_property_string("port", "5667");
  }
  check_mk_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    add_ssl_keys(root_path);
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return std::make_shared<check_mk_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return std::make_shared<check_mk_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &_source, client::destination_container &data) {
    add_ssl_options(desc, data);
  }
};
}  // namespace check_mk_handler
