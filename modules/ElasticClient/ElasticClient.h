// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include <string>
#include <vector>

class ElasticClient : public nscapi::impl::simple_plugin {
 private:
  bool started;

  std::string hostname_;

  std::string address;
  std::string user;
  std::string password;
  std::string api_key;
  std::string tls_version;
  std::string verify_mode;
  std::string ca;
  unsigned int timeout;

  std::string event_index;
  std::string event_type;

  std::string metrics_index;
  std::string metrics_type;

  std::string nsclient_index;
  std::string nsclient_type;

 public:
  ElasticClient();
  virtual ~ElasticClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void submitMetrics(const PB::Metrics::MetricsMessage &response);
  void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);

  void handleLogMessage(const PB::Log::LogEntry::Entry &message);

 private:
  void send_to_elastic(const std::string &index, const std::string &type, const std::vector<std::string> &payloads, bool log_errors) const;
};
