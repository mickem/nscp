// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <memory>
#include <settings/client/settings_client_interface.hpp>
#include <settings/settings_core.hpp>
#include <utility>

#include "settings_handler_impl.hpp"

namespace settings_manager {
struct provider_interface {
  virtual std::string expand_path(std::string file) = 0;
  virtual nsclient::logging::logger_instance get_logger() const = 0;
  // Apply a set of path overrides (parsed from boot.ini's [paths] section)
  // to the underlying path resolver. Called once from NSCSettingsImpl::boot()
  // after boot.ini has been read but before the main settings store is
  // opened, so that overrides take effect for every subsequent path lookup
  // (including the main INI's own location).
  virtual void apply_path_overrides(std::map<std::string, std::string> overrides) = 0;
  // Select the on-disk layout named by boot.ini's [layout] section, before any
  // path is resolved and before the [paths] overrides are applied - an explicit
  // override is a statement about one folder, the layout is the default the
  // rest of them are built from, so the layout has to be in place first.
  // Default is a no-op for providers that have only one layout.
  virtual void apply_layout(const std::string &mode) { static_cast<void>(mode); }
  // Create the folder the layout points at, with whatever access control the
  // platform needs, before anything writes into it. Called after the [paths]
  // overrides so it acts on the operator's final answer, and before
  // prepare_trust_store(), which is the first thing to write a file there.
  virtual void prepare_shared_folder() {}
  // Materialise anything the settings transport needs to verify a peer, before
  // the main settings store is opened. A remote (https://) settings source is
  // fetched as part of opening that store, and on Windows the CA bundle it
  // verifies against (${ca-path}) is a file this service exports itself - so it
  // has to exist by now, not merely by the end of startup. Called from boot()
  // after boot.ini's [paths] overrides have been applied, so ${ca-path}
  // resolves against the operator's final paths. Implementations must be
  // idempotent; the default is a no-op for providers with nothing to prepare.
  virtual void prepare_trust_store() {}
};

class NSCSettingsImpl : public settings::settings_handler_impl {
 private:
  boost::filesystem::path boot_;
  provider_interface *provider_;
  std::string tls_version_;
  std::string tls_verify_mode_;
  std::string tls_ca_;
  std::string proxy_url_;
  std::string no_proxy_;

 public:
  // Defaults for [tls] in boot.ini. These govern the transport used to fetch a
  // remote (http[s]://) settings source, i.e. the channel that delivers this
  // agent's entire configuration - including [/settings/external scripts],
  // which is arbitrary command execution by design. Verifying the peer is
  // therefore not optional-by-default: `verify mode = none` used to mean any
  // attacker who could answer for the settings host owned every agent that
  // booted against it, with no certificate error and nothing in the log.
  //
  // `ca-path` is the same trust anchor CheckNet and CheckNSCP already default
  // to: the auto-exported Windows ROOT bundle on Windows, the distribution CA
  // bundle on Linux.
  static constexpr const char *kDefaultTlsVerifyMode = "peer";
  static constexpr const char *kDefaultTlsCa = "${ca-path}";

  explicit NSCSettingsImpl(provider_interface *provider)
      : settings::settings_handler_impl(provider->get_logger()),
        provider_(provider),
        tls_version_("1.3"),
        tls_verify_mode_(kDefaultTlsVerifyMode),
        tls_ca_(kDefaultTlsCa) {}
  NSCSettingsImpl(provider_interface *provider, std::string tls_version, std::string tls_verify_mode, std::string tls_ca)
      : settings::settings_handler_impl(provider->get_logger()),
        provider_(provider),
        tls_version_(std::move(tls_version)),
        tls_verify_mode_(std::move(tls_verify_mode)),
        tls_ca_(std::move(tls_ca))

  {}
  ~NSCSettingsImpl() override = default;

  std::string expand_simple_context(const std::string &key);
  void boot(std::string file);
  std::string find_file(std::string file, std::string fallback = "");
  std::string expand_path(std::string file);
  std::string expand_context(const std::string &key);
  // The protocol aliases only ("ini", "dummy", ...), without the host name
  // placeholders expand_context resolves. Use this wherever the result is
  // written back to a configuration file rather than opened.
  static std::string expand_context_alias(const std::string &key);

  settings::instance_raw_ptr create_instance(std::string alias, std::string key);
  void change_context(const std::string &file);
  bool context_exists(std::string key);
  bool create_context(const std::string &key);
  bool has_boot_conf();
  void write_boot_ini_key(std::string section, std::string key, std::string value);
  void set_primary(std::string key);
  bool supports_edit(const std::string key);

  std::string get_tls_version() const override { return tls_version_; }
  std::string get_tls_verify_mode() const override { return tls_verify_mode_; }
  std::string get_tls_ca() const override { return tls_ca_; }
  std::string get_proxy_url() const override { return proxy_url_; }
  std::string get_no_proxy() const override { return no_proxy_; }
};

// Alias to make handling "compatible" with old syntax
settings::instance_ptr get_settings();
settings::instance_ptr get_settings_no_wait();
settings::settings_core *get_core();
std::shared_ptr<nscapi::settings_helper::settings_impl_interface> get_proxy();
void destroy_settings();
bool init_settings(provider_interface *provider, const std::string &context = "");
bool init_installer_settings(provider_interface *provider, const std::string &context, std::string tls_version, std::string tls_verify_mode,
                             std::string tls_ca);
void change_context(const std::string &context);
void set_boot_ini_primary(const std::string &context);
bool has_boot_conf();
void write_boot_ini_key(std::string section, std::string key, std::string value);
bool context_exists(const std::string &key);
bool create_context(std::string key);
// True when this host carries configuration of its own, i.e. anything in the
// active settings store beyond the [/includes] entry that pulls in the
// fleet-managed file.
//
// It matters because a local value wins: a lookup reads this store first and
// only falls back to an included file when the key is absent, so anything set
// here silently shadows what the fleet server sends. Enrollment warns about it
// and the agent reports *that* it is the case (never what is configured).
bool has_local_configuration();
}  // namespace settings_manager
