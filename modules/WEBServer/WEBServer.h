// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Server.h>

#include <client/simple_client.hpp>
#include <memory>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nscapi/settings/kvp_map.hpp>

#include "error_handler_interface.hpp"
#include "event_store.hpp"
#include "session_manager_interface.hpp"
#include "user_config.hpp"

class WEBServer : public nscapi::impl::simple_plugin {
  using role_map = nscapi::settings::kvp_map<std::string>;

 public:
  WEBServer();
  virtual ~WEBServer();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

  void ensure_role(role_map &roles, const nscapi::settings_helper::settings_registry &settings, const std::string &role_path, const std::string &role,
                   const std::string &value, const std::string &reason);
  void ensure_user(const nscapi::settings_helper::settings_registry &settings, const std::string &path, const std::string &user, const std::string &role,
                   const std::string &value, const std::string &reason);

  void prepareShutdown();
  bool unloadModule();
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void submitMetrics(const PB::Metrics::MetricsMessage &response) const;
  bool install_server(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_add_user(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_add_role(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool password(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_install_ui(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_uninstall_ui(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);
  bool cli_ui_status(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response);

 private:
  void add_user(const std::string &key, const std::string &arg);

  std::shared_ptr<error_handler_interface> log_handler;
  std::shared_ptr<client::cli_client> client;
  std::shared_ptr<session_manager_interface> session;
  std::shared_ptr<event_store> events_;
  std::shared_ptr<Mongoose::Server> server;

  web_server::user_config users_;
  unsigned long last_log_index;
};
