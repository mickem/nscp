// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "boost_python_wrapper.hpp"
#include "script_interface.hpp"

class PythonScript : public nscapi::impl::simple_plugin {
 private:
  boost::filesystem::path root_;
  std::string alias_;

  std::shared_ptr<script_provider_interface> provider_;

 public:
  PythonScript() {}
  virtual ~PythonScript() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void submitMetrics(const PB::Metrics::MetricsMessage &response);
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);
  void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);

  void execute_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

 private:
  void loadScript(std::string alias, std::string script);
};
