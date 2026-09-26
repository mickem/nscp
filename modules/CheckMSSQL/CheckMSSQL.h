// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/snapshot.hpp>

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
  // The module's connection defaults, swapped whole on every load and read
  // whole by every check and facts round (see nscapi/settings/snapshot.hpp).
  nscapi::settings_helper::snapshot<mssql_odbc::connection_info> defaults_;
  // Which parts of the `mssql` fact set this module is configured to produce
  // ([/settings/mssql/facts]). Read by fetchFacts on the core's scheduler
  // thread, written by loadModuleEx on every load, hence atomic.
  std::atomic<bool> facts_server_{false};
  std::atomic<bool> facts_databases_{false};
};
