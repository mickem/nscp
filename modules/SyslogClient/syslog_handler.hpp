// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/settings/helper.hpp>

namespace syslog_handler {
namespace sh = nscapi::settings_helper;

struct syslog_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  syslog_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_int("retries", 3);
    set_property_string("path", "/nsclient++");
    set_property_string("severity", "error");
    set_property_string("facility", "kernel");
    // The property keys here (and in read() below) must be the data keys
    // connection_data reads - "tag template", "message template" and the
    // "<state> severity" family - or the configured value silently never
    // reaches the wire. The settings keys (tag_syntax/message_syntax) are
    // unchanged; only the internal property name is aligned.
    set_property_string("tag template", "NSCA");
    set_property_string("message template", "%message%");
    set_property_string("ok severity", "informational");
    set_property_string("warning severity", "warning");
    set_property_string("critical severity", "critical");
    set_property_string("unknown severity", "emergency");
  }
  syslog_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_key()
        .add_string("severity", sh::string_fun_key([this](auto value) { this->set_property_string("severity", value); }, "error"), "SEVERITY",
                    "Severity of the syslog message when no per-state severity matches.")
        .add_string("facility", sh::string_fun_key([this](auto value) { this->set_property_string("facility", value); }, "kernel"), "FACILITY",
                    "Facility of the syslog message.")
        .add_string("tag_syntax", sh::string_fun_key([this](auto value) { this->set_property_string("tag template", value); }, "NSCA"), "TAG TEMPLATE",
                    "The template used for the syslog TAG field (%message% expands to the check output).")
        .add_string("message_syntax", sh::string_fun_key([this](auto value) { this->set_property_string("message template", value); }, "%message%"),
                    "MESSAGE TEMPLATE", "The template used for the syslog MESSAGE field (%message% expands to the check output).")
        .add_string("ok severity", sh::string_fun_key([this](auto value) { this->set_property_string("ok severity", value); }, "informational"), "OK SEVERITY",
                    "Severity to use when the check result is OK.")
        .add_string("warning severity", sh::string_fun_key([this](auto value) { this->set_property_string("warning severity", value); }, "warning"),
                    "WARNING SEVERITY", "Severity to use when the check result is WARNING.")
        .add_string("critical severity", sh::string_fun_key([this](auto value) { this->set_property_string("critical severity", value); }, "critical"),
                    "CRITICAL SEVERITY", "Severity to use when the check result is CRITICAL.")
        .add_string("unknown severity", sh::string_fun_key([this](auto value) { this->set_property_string("unknown severity", value); }, "emergency"),
                    "UNKNOWN SEVERITY", "Severity to use when the check result is UNKNOWN.");

    // Transport selection plus the tree-wide TLS target keys. UDP is the
    // historical default; TLS verifies the server certificate by default
    // (verify mode = peer against `ca`, plus hostname pinning) - the only
    // opt-outs are the explicit `verify mode = none` / `insecure = true`.
    root_path.add_key()
        .add_string("transport", sh::string_fun_key([this](auto value) { this->set_property_string("transport", value); }, ""), "TRANSPORT",
                    "How to reach the syslog server: udp (default, RFC 3164), tcp (RFC 6587) or tls (RFC 5425, encrypted and authenticated).")
        .add_string("framing", sh::string_fun_key([this](auto value) { this->set_property_string("framing", value); }, ""), "FRAMING",
                    "Message framing for stream transports: octet-counted (default, RFC 6587/RFC 5425 \"LEN SP MSG\") or non-transparent (LF-terminated, "
                    "legacy TCP receivers only - not allowed with TLS).",
                    true)
        .add_bool("use ssl", sh::bool_fun_key([this](auto value) { this->set_property_bool("ssl", value); }, false), "ENABLE TLS",
                  "Use TLS for this target (same as transport = tls).")
        .add_string("ca", sh::path_fun_key([this](auto value) { this->set_property_string("ca", value); }, "${ca-path}"), "CA",
                    "Certificate authority bundle used to verify the server certificate.")
        .add_string("verify mode", sh::string_fun_key([this](auto value) { this->set_property_string("verify mode", value); }, "peer"), "VERIFY MODE",
                    "How to verify the server certificate: 'peer' (default) / 'peer-cert' validate the chain and hostname; 'none' disables verification "
                    "(insecure).")
        .add_string("tls version", sh::string_fun_key([this](auto value) { this->set_property_string("tls version", value); }, "1.2+"), "TLS VERSION",
                    "The TLS version to use (1.0, 1.1, 1.2, 1.2+ or 1.3).")
        .add_string("allowed ciphers",
                    sh::string_fun_key([this](auto value) { this->set_property_string("allowed ciphers", value); }, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"),
                    "ALLOWED CIPHERS", "OpenSSL cipher list.", true)
        .add_string("certificate", sh::path_fun_key([this](auto value) { this->set_property_string("certificate", value); }), "CLIENT CERTIFICATE",
                    "Optional client certificate for mutual TLS.", true)
        .add_string("certificate key", sh::path_fun_key([this](auto value) { this->set_property_string("certificate key", value); }), "CLIENT CERTIFICATE KEY",
                    "Private key for the client certificate (if not bundled in the certificate file).", true)
        .add_string("certificate format", sh::string_fun_key([this](auto value) { this->set_property_string("certificate format", value); }, "PEM"),
                    "CERTIFICATE FORMAT", "Format of the client certificate/key (PEM or DER).", true)
        .add_bool("insecure", sh::bool_fun_key([this](auto value) { this->set_property_bool("insecure", value); }, false), "INSECURE",
                  "When true, allow TLS connections that do not verify the server certificate. Off by default; enabling this disables protection against "
                  "man-in-the-middle attacks.",
                  true);

    // The keys registered above only take effect when they are registered
    // and notified; without this a settings-defined target silently kept
    // every constructor default.
    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) { return std::make_shared<syslog_target_object>(alias, path); }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return std::make_shared<syslog_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    // The tree-wide TLS client options (--ssl, --ca, --verify, --certificate,
    // ...); `--ssl` is a valued boolean, so REST-style `ssl=true` tokens
    // parse as well.
    add_ssl_options(desc, data);

    // clang-format off
  desc.add_options()
    ("path", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("path", value); }),
    "")
    ("transport", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("transport", value); }),
    "Transport to use: udp (default, RFC 3164), tcp (RFC 6587) or tls (RFC 5425)")
    ("framing", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("framing", value); }),
    "Stream framing: octet-counted (default) or non-transparent (legacy TCP receivers only)")
    ("tls-version", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("tls version", value); }),
    "The TLS version to use (1.0, 1.1, 1.2, 1.2+ or 1.3)")
    ("insecure", po::value<bool>()->implicit_value(true)->notifier([&data] (const bool &value) { data.set_bool_data("insecure", value); }),
    "Allow TLS connections without verifying the server certificate (disables MITM protection)")
    ("severity,s", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("severity", value); }),
    "Severity of error message")
    // The data keys must match what connection_data reads ("<state> severity",
    // with a space): the underscore variants used to be stored here and were
    // never read, so these options silently did nothing.
    ("unknown-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("unknown severity", value); }),
    "Severity to use when the check result is UNKNOWN")
    ("ok-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("ok severity", value); }),
    "Severity to use when the check result is OK")
    ("warning-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("warning severity", value); }),
    "Severity to use when the check result is WARNING")
    ("critical-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("critical severity", value); }),
    "Severity to use when the check result is CRITICAL")
    ("facility,f", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("facility", value); }),
    "Facility of error message")
    ("tag template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("tag template", value); }),
    "Tag template (TODO)")
    ("message template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("message template", value); }),
    "Message template (TODO)")
    ;
    // clang-format on
  }
};
}  // namespace syslog_handler
