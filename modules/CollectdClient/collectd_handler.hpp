// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/settings/helper.hpp>

namespace collectd_handler {
namespace sh = nscapi::settings_helper;

struct collectd_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  collectd_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_string("port", "25826");
    set_property_string("host", "239.192.74.66");
  }
  collectd_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_key().add_int(
        "interval", sh::int_fun_key([this](auto value) { this->set_property_int("interval", value); }), "METRICS INTERVAL",
        "The interval (in seconds) reported to collectd for metrics sent to this target. Overrides the client-level interval; should match the core 'metrics "
        "interval'.");

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return std::make_shared<collectd_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return std::make_shared<collectd_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    // clang-format off
    desc.add_options()
      ("interval", po::value<unsigned int>()->notifier([&data] (auto value) { data.set_int_data("interval", value); }),
      "The interval (in seconds) reported to collectd for these metrics.")
    ;
    // clang-format on
  }
};
}  // namespace collectd_handler
