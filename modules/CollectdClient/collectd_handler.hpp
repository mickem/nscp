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
    // collectd's default multicast group, so a target with no address still
    // matches a stock collectd server. See `multicast interface` for which
    // interface those datagrams leave through.
    set_property_string("host", "239.192.74.66");
    set_property_string("multicast interface", "auto");
  }
  collectd_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_key()
        .add_int("interval", sh::int_fun_key([this](auto value) { this->set_property_int("interval", value); }), "METRICS INTERVAL",
                 "The interval (in seconds) reported to collectd for metrics sent to this target. Overrides the client-level interval; should match the core "
                 "'metrics interval'.")

        .add_string("multicast interface", sh::string_fun_key([this](auto value) { this->set_property_string("multicast interface", value); }, "auto"),
                    "MULTICAST INTERFACE",
                    "Which local interface a multicast target's metrics leave through. A target with no address is sent to collectd's default multicast group "
                    "(239.192.74.66), so this decides how far those metrics travel. 'auto' (the default) sends one copy through the interface the routing "
                    "table picks; 'all' sends a copy through every local interface of the target's address family, which puts the metrics on every attached "
                    "segment; a comma-separated list of local IP addresses sends one copy through each of them and nothing else. Ignored for unicast "
                    "targets.");

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

      ("multicast-interface", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("multicast interface", value); }),
      "Which local interface a multicast target's metrics leave through: auto (the routed interface), all (every local interface of the target's address "
      "family) or a comma-separated list of local IP addresses.")
    ;
    // clang-format on
  }
};
}  // namespace collectd_handler
