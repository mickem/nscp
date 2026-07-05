// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

class CheckDocker : public nscapi::impl::simple_plugin {
 public:
  CheckDocker() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void check_docker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  std::size_t get_errors(std::string &last_error);
};