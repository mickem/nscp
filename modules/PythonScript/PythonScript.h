// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>
#include <utility>
#include <vector>

#include "boost_python_wrapper.hpp"
#include <nscp/script_roots.hpp>

#include "script_interface.hpp"

class PythonScript : public nscapi::impl::simple_plugin {
 private:
  boost::filesystem::path root_;
  // Folders a configured script may be loaded from: ${scripts} plus whatever
  // the operator adds for scripts the agent does not own.
  nscp::scripts::allowed_roots allowed_roots_;
  std::string alias_;

  std::shared_ptr<script_provider_interface> provider_;
  // The scripts the settings walk found, in the order it found them. They
  // cannot be loaded as they are discovered: loading one constructs a
  // `python_script`, which takes the GIL, and the interpreter is only
  // initialised once the walk has produced the settings `init()` needs.
  std::vector<std::pair<std::string, std::string> > pending_scripts_;

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
  // Settings callback: records the script for `load_pending_scripts()`.
  void loadScript(std::string alias, std::string script);
  // Split a comma separated setting and add each entry as a root, expanding
  // path tokens per entry rather than for the list as a whole.
  void add_script_roots(const std::string &value);
  // Loads what the settings walk recorded. Called once the interpreter is up.
  void load_pending_scripts();
};
