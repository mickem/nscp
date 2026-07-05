// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/settings/helper.hpp>

namespace nscp_handler {
namespace sh = nscapi::settings_helper;

struct nrpe_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  nrpe_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_string("certificate", "");
    set_property_string("certificate key", "");
    set_property_string("certificate format", "PEM");
    set_property_string("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    set_property_string("verify mode", "none");
    set_property_string("password", "");
  }

  nrpe_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path
        .add_key()

        .add_password("password", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }), "PASSWORD",
                      "The password to use to authenticate towards the server.");

    settings.register_all();
    settings.notify();
    settings.clear();

    add_ssl_keys(root_path);

    settings.register_all();
    settings.notify();
  }

  virtual void translate(const std::string &key, const std::string &translated_value) { parent::translate(key, translated_value); }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) { return std::make_shared<nrpe_target_object>(alias, path); }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return std::make_shared<nrpe_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &, client::destination_container &target) {
    add_ssl_options(desc, target);

    desc.add_options()("password,p", po::value<std::string>()->notifier([&target](auto value) { target.set_string_data("password", value); }), "Password");
  }
};
}  // namespace nscp_handler
