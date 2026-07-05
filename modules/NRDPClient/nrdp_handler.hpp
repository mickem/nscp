// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/settings/helper.hpp>

namespace nrdp_handler {
namespace sh = nscapi::settings_helper;

struct nrdp_target_object : nscapi::targets::target_object {
  typedef target_object parent;

  nrdp_target_object(const std::string& alias, const std::string& path) : parent(alias, path) { set_property_int("timeout", 30); }

  nrdp_target_object(const nscapi::settings_objects::object_instance& other, const std::string& alias, const std::string& path) : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (oneliner) return;

    root_path.add_key()
        .add_string("key", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN", "The security token")
        .add_password("password", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN",
                      "The security token")
        .add_password("token", sh::string_fun_key([this](const auto& value) { this->set_property_string("token", value); }), "SECURITY TOKEN",
                      "The security token")
        .add_string("tls version", sh::string_fun_key([this](const auto& value) { this->set_property_string("tls version", value); }, "1.3"), "Tls version",
                    "The tls version to use 1.0, 1.1, 1.2, 1.3 or any")
        .add_string("verify mode", sh::string_fun_key([this](const auto& value) { this->set_property_string("verify mode", value); }, "peer"),
                    "TLS peer verify mode",
                    "Comma separated list of options: none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. "
                    "In general use peer-cert or none for self signed certificates.")
        .add_string("ca", sh::path_fun_key([this](const auto& value) { this->set_property_string("ca", value); }, "${ca-path}"), "Certificate Authority",
                    "Certificate authority to use when verifying certificates. Defaults to ${ca-path} (the auto-generated system ROOT bundle on Windows, "
                    "the distribution CA store on Linux).")
        .add_string("proxy", sh::string_fun_key([this](const auto& value) { this->set_property_string("proxy", value); }), "HTTP proxy URL",
                    "HTTP proxy to use when submitting checks (e.g. http://user:pass@proxy:3128/).")
        .add_string("no proxy", sh::string_fun_key([this](const auto& value) { this->set_property_string("no proxy", value); }), "No-proxy list",
                    "Comma-separated list of hostnames that bypass the proxy.");

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override { return std::make_shared<nrdp_target_object>(alias, path); }
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) override {
    return std::make_shared<nrdp_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description& desc, client::destination_container& source, client::destination_container& data) override {
    // clang-format off
    desc.add_options()
    ("key", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("password", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("source-host", po::value<std::string>()->notifier([&source] (const auto& value) { source.set_string_data("host", value); }),
    "Source/sender host name (default is auto which means use the name of the actual host)")
    ("sender-host", po::value<std::string>()->notifier([&source] (const auto& value) { source.set_string_data("host", value); }),
    "Source/sender host name (default is auto which means use the name of the actual host)")
    ("token", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("token", value); }),
    "The security token")
    ("tls-version", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("tls version", value); }),
      "The tls version to use 1.0, 1.1, 1.2, 1.3")
    ("tls version", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("tls version", value); }),
      "Legacy alias for --tls-version (kept for backwards compatibility).")
    ("verify", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Coma separated list of option none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. In general use peer-cert or none for self signed certificates.")
    ("verify-mode", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Alias for --verify.")
    ("verify mode", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("verify mode", value); }),
      "Legacy alias for --verify (kept for backwards compatibility).")
    ("ca", po::value<std::string>()->notifier([&data] (const auto& value) { data.set_string_data("ca", value); }),
      "Certificate authority to use when verifying certificates.")
    ("proxy", po::value<std::string>()->notifier([&data] (const auto &value) { data.set_string_data("proxy", value); }),
      "HTTP proxy URL to route requests through (e.g. http://user:pass@proxy:3128/).")
    ("no-proxy", po::value<std::string>()->notifier([&data] (const auto &value) { data.set_string_data("no proxy", value); }),
      "Comma-separated list of hostnames that bypass the proxy.")
    ;
    // clang-format on
  }
};
}  // namespace nrdp_handler
