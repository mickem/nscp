// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <memory>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

#include "mysql_client.hpp"

class CheckMySQL : public nscapi::impl::simple_plugin {
  // The module's connection defaults ([/settings/mysql]), as one immutable
  // snapshot. A settings reload runs loadModuleEx on the loading thread while
  // checks and facts rounds read the host, user and password on theirs, and
  // a std::string being rewritten under a reader is a data race; so a load
  // builds a fresh struct and swaps the pointer, and every reader takes the
  // pointer once and reads that. Never null: a module that has not loaded
  // yet reads the defaults.
  std::shared_ptr<const mysql_client::connection_info> defaults_ = std::make_shared<const mysql_client::connection_info>();
  std::shared_ptr<const mysql_client::connection_info> settings_snapshot() const { return std::atomic_load(&defaults_); }
  // Which parts of the `mysql` fact set this module is configured to produce
  // ([/settings/mysql/facts]). Read by fetchFacts on the core's scheduler
  // thread, written by loadModuleEx on every load, hence atomic.
  std::atomic<bool> facts_server_{false};
  std::atomic<bool> facts_databases_{false};

 public:
  CheckMySQL();
  virtual ~CheckMySQL() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);

  // Check commands
  void check_mysql(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mysql_query(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
