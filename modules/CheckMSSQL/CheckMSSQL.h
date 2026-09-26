// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <memory>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

#include "odbc_query.hpp"

class CheckMSSQL : public nscapi::impl::simple_plugin {
 public:
  CheckMSSQL();
  virtual ~CheckMSSQL() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Host facts
  void fetchFacts(const nscapi::facts::request &request, nscapi::facts::response &response);

  // Check commands
  void check_mssql(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mssql_query(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mssql_databases(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mssql_backup(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mssql_jobs(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

 private:
  // Connection defaults from /settings/mssql, as one immutable snapshot;
  // each check copies these and applies its command-line overrides. A
  // settings reload runs loadModuleEx on the loading thread while checks and
  // facts rounds read the server, login and driver on theirs, and a
  // std::string being rewritten under a reader is a data race; so a load
  // builds a fresh struct and swaps the pointer, and every reader takes the
  // pointer once and reads that. Never null: a module that has not loaded
  // yet reads the defaults.
  std::shared_ptr<const mssql_odbc::connection_info> defaults_ = std::make_shared<const mssql_odbc::connection_info>();
  std::shared_ptr<const mssql_odbc::connection_info> settings_snapshot() const { return std::atomic_load(&defaults_); }
  // Which parts of the `mssql` fact set this module is configured to produce
  // ([/settings/mssql/facts]). Read by fetchFacts on the core's scheduler
  // thread, written by loadModuleEx on every load, hence atomic.
  std::atomic<bool> facts_server_{false};
  std::atomic<bool> facts_databases_{false};
};
