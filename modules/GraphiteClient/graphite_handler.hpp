// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/settings/helper.hpp>

namespace graphite_handler {
namespace sh = nscapi::settings_helper;

struct graphite_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  graphite_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_bool("send perfdata", true);
    set_property_bool("send status", true);
    set_property_int("timeout", 30);
    set_property_string("perf path", "nsclient.${hostname}.${check_alias}.${perf_alias}");
    set_property_string("status path", "nsclient.${hostname}.${check_alias}.status");
    set_property_string("metric path", "nsclient.${hostname}.${metric}");
  }
  graphite_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (is_default()) {
      root_path
          .add_key()

          .add_string(
              "path",
              sh::string_fun_key([this](auto value) { this->set_property_string("perf path", value); }, "nsclient.${hostname}.${check_alias}.${perf_alias}"),
              "PATH FOR METRICS", "Path mapping for metrics")

          .add_string("status path",
                      sh::string_fun_key([this](auto value) { this->set_property_string("status path", value); }, "nsclient.${hostname}.${check_alias}.status"),
                      "PATH FOR STATUS", "Path mapping for status")

          .add_bool("send perfdata", sh::bool_fun_key([this](auto value) { this->set_property_bool("send perfdata", value); }, true), "SEND PERF DATA",
                    "Send performance data to this server")

          .add_bool("send status", sh::bool_fun_key([this](auto value) { this->set_property_bool("send status", value); }, true), "SEND STATUS",
                    "Send status data to this server")

          .add_string("metric path",
                      sh::string_fun_key([this](auto value) { this->set_property_string("metric path", value); }, "nsclient.${hostname}.${metric}"),
                      "PATH FOR METRICS", "Path mapping for metrics");
    } else {
      root_path
          .add_key()

          .add_string("path", sh::string_fun_key([this](auto value) { this->set_property_string("perf path", value); }), "PATH FOR METRICS",
                      "Path mapping for metrics")

          .add_string("status path", sh::string_fun_key([this](auto value) { this->set_property_string("status path", value); }), "PATH FOR STATUS",
                      "Path mapping for status")

          .add_bool("send perfdata", sh::bool_fun_key([this](auto value) { this->set_property_bool("send perfdata", value); }), "SEND PERF DATA",
                    "Send performance data to this server")

          .add_bool("send status", sh::bool_fun_key([this](auto value) { this->set_property_bool("send status", value); }), "SEND STATUS",
                    "Send status data to this server");
    }

    // Optional TLS. Carbon's line receiver is plaintext, so this is for talking
    // to a TLS-terminating proxy (stunnel / nginx / carbon-relay-ng) in front of
    // carbon. SSL is off by default to preserve the historical plaintext
    // behaviour; when enabled the server certificate is verified by default
    // (verify mode = peer). Registered for every target with defaults.
    root_path.add_key()
        .add_bool("ssl", sh::bool_fun_key([this](auto value) { this->set_property_bool("ssl", value); }, false), "ENABLE TLS",
                  "Encrypt the connection with TLS. Carbon does not speak TLS itself - point this at a TLS-terminating proxy in front of carbon.")
        .add_string("ca", sh::path_fun_key([this](auto value) { this->set_property_string("ca", value); }, "${ca-path}"), "CA",
                    "Certificate authority bundle used to verify the server certificate.")
        .add_string("verify mode", sh::string_fun_key([this](auto value) { this->set_property_string("verify mode", value); }, "peer"), "VERIFY MODE",
                    "How to verify the server certificate: 'peer' (default) / 'peer-cert' validate the chain and hostname; 'none' disables verification (insecure).")
        .add_string("tls version", sh::string_fun_key([this](auto value) { this->set_property_string("tls version", value); }, "1.2+"), "TLS VERSION",
                    "The TLS version to use (1.0, 1.1, 1.2, 1.2+ or 1.3).")
        .add_string("allowed ciphers",
                    sh::string_fun_key([this](auto value) { this->set_property_string("allowed ciphers", value); }, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"),
                    "ALLOWED CIPHERS", "OpenSSL cipher list.")
        .add_string("certificate", sh::path_fun_key([this](auto value) { this->set_property_string("certificate", value); }), "CLIENT CERTIFICATE",
                    "Optional client certificate for mutual TLS.")
        .add_string("certificate key", sh::path_fun_key([this](auto value) { this->set_property_string("certificate key", value); }), "CLIENT CERTIFICATE KEY",
                    "Private key for the client certificate (if not bundled in the certificate file).")
        .add_string("certificate format", sh::string_fun_key([this](auto value) { this->set_property_string("certificate format", value); }, "PEM"),
                    "CERTIFICATE FORMAT", "Format of the client certificate/key (PEM or DER).");

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return std::make_shared<graphite_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return std::make_shared<graphite_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    desc.add_options()("path", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("path", value); }), "");
  }
};
}  // namespace graphite_handler
