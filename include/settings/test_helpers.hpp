/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

// Shared mocks and helpers for include/settings/* unit tests.
//
// settings_core has ~30 pure-virtual methods; the storage backends only
// touch a handful of them at runtime (logger, expand_path, register_path,
// set_reload, create_instance).  The mock here implements those with
// sensible defaults and stubs the rest with no-op or empty returns so
// tests can construct any backend without dragging in libs/settings_manager.

#pragma once

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <fstream>
#include <nsclient/logger/logger.hpp>
#include <settings/settings_core.hpp>
#include <sstream>
#include <string>

namespace settings_test {

// A logger that swallows everything.  We don't assert on log output here.
class null_logger : public nsclient::logging::logger {
 public:
  void trace(const std::string &, const char *, const int, const std::string &) override {}
  void debug(const std::string &, const char *, const int, const std::string &) override {}
  void info(const std::string &, const char *, const int, const std::string &) override {}
  void warning(const std::string &, const char *, const int, const std::string &) override {}
  void error(const std::string &, const char *, const int, const std::string &) override {}
  void critical(const std::string &, const char *, const int, const std::string &) override {}

  bool should_trace() const override { return false; }
  bool should_debug() const override { return false; }
  bool should_info() const override { return false; }
  bool should_warning() const override { return false; }
  bool should_error() const override { return false; }
  bool should_critical() const override { return false; }

  void raw(const std::string &) override {}
  void add_subscriber(nsclient::logging::logging_subscriber_instance) override {}
  void clear_subscribers() override {}
  bool startup() override { return true; }
  bool shutdown() override { return true; }
  void destroy() override {}
  void configure() override {}
  void set_log_level(std::string) override {}
  std::string get_log_level() const override { return "off"; }
  void set_backend(std::string) override {}
};

inline nsclient::logging::logger_instance make_null_logger() { return std::make_shared<null_logger>(); }

// Minimal settings_core stub.  Only the methods actually invoked by the
// portable storage backends do anything useful; everything else is a no-op
// or returns a default-constructed value.  Tests that need richer behaviour
// can subclass and override.
class mock_settings_core : public settings::settings_core {
 public:
  mock_settings_core()
      : logger_(make_null_logger()),
        proxy_url_(),
        no_proxy_(),
        tls_version_("1.3"),
        tls_verify_("none"),
        tls_ca_(),
        reload_flag_(false),
        dirty_flag_(false),
        ready_flag_(true) {}

  // --- methods backends actually call ----------------------------------------
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  std::string expand_path(std::string key) override { return key; }
  std::string expand_context(const std::string &key) override { return key; }
  void register_path(unsigned int, std::string, std::string, std::string, bool, bool, bool = true) override {}
  void register_subkey(unsigned int, std::string, std::string, std::string, bool, bool, bool = true) override {}
  void register_key(unsigned int, std::string, std::string, std::string, std::string, std::string, std::string, bool, bool, bool = true) override {}
  void add_sensitive_key(unsigned int, std::string, std::string) override {}
  void register_tpl(unsigned int, std::string, std::string, std::string) override {}
  void set_reload(bool flag = true) override { reload_flag_ = flag; }
  bool needs_reload() override { return reload_flag_; }

  // create_instance is invoked by add_child().  Tests that exercise child
  // chaining can override; the default here returns null and add_child logs.
  settings::instance_raw_ptr create_instance(std::string, std::string) override { return settings::instance_raw_ptr(); }

  // --- proxy / TLS plumbing exercised by settings_http -----------------------
  std::string get_proxy_url() const override { return proxy_url_; }
  std::string get_no_proxy() const override { return no_proxy_; }
  std::string get_tls_version() const override { return tls_version_; }
  std::string get_tls_verify_mode() const override { return tls_verify_; }
  std::string get_tls_ca() const override { return tls_ca_; }

  // --- the rest: no-ops / empty defaults -------------------------------------
  boost::optional<key_description> get_registered_key(std::string, std::string) override { return boost::none; }
  bool is_sensitive_key(std::string, std::string) override { return false; }
  settings::settings_core::path_description get_registered_path(const std::string &) override { return {}; }
  std::list<settings::settings_core::tpl_description> get_registered_templates() override { return {}; }
  string_list get_reg_sections(std::string, bool) override { return {}; }
  string_list get_reg_keys(std::string, bool) override { return {}; }
  settings::instance_ptr get() override { return settings::instance_ptr(); }
  settings::instance_ptr get_no_wait() override { return settings::instance_ptr(); }
  void migrate_to(std::string, std::string) override {}
  void migrate_from(std::string, std::string) override {}
  void set_primary(std::string) override {}
  settings::error_list validate() override { return {}; }
  void update_defaults() override {}
  void remove_defaults() override {}
  void boot(std::string = "boot.ini") override {}
  void set_ready(bool flag = true) override { ready_flag_ = flag; }
  bool is_ready() override { return ready_flag_; }
  void house_keeping() override {}
  std::string find_file(std::string file, std::string) override { return file; }
  bool supports_edit(const std::string) override { return true; }
  void set_base(boost::filesystem::path) override {}
  std::string to_string() override { return "mock"; }
  void set_dirty(bool flag = true) override { dirty_flag_ = flag; }
  bool is_dirty() override { return dirty_flag_; }
  bool supports_updates() override { return true; }
  bool use_sensitive_keys() override { return false; }

  // Test-side knobs:
  void set_proxy_url(std::string url) { proxy_url_ = std::move(url); }
  void set_no_proxy(std::string list) { no_proxy_ = std::move(list); }

 private:
  nsclient::logging::logger_instance logger_;
  std::string proxy_url_;
  std::string no_proxy_;
  std::string tls_version_;
  std::string tls_verify_;
  std::string tls_ca_;
  bool reload_flag_;
  bool dirty_flag_;
  bool ready_flag_;
};

// RAII helper: creates a unique temp directory under the system temp area
// and removes it on destruction.  Used by INI/HTTP-cache tests.
class temp_dir {
 public:
  temp_dir() {
    path_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-settings-test-%%%%%%%%");
    boost::filesystem::create_directories(path_);
  }
  ~temp_dir() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(path_, ec);
  }
  temp_dir(const temp_dir &) = delete;
  temp_dir &operator=(const temp_dir &) = delete;

  const boost::filesystem::path &path() const { return path_; }
  std::string string() const { return path_.string(); }
  boost::filesystem::path file(const std::string &name) const { return path_ / name; }

 private:
  boost::filesystem::path path_;
};

inline void write_file(const boost::filesystem::path &p, const std::string &content) {
  std::ofstream os(p.string().c_str(), std::ofstream::binary);
  os << content;
}

inline std::string read_file(const boost::filesystem::path &p) {
  std::ifstream is(p.string().c_str(), std::ifstream::binary);
  std::ostringstream os;
  os << is.rdbuf();
  return os.str();
}

}  // namespace settings_test
