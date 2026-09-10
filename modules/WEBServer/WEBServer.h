// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <Server.h>

#include <boost/thread/mutex.hpp>
#include <client/simple_client.hpp>
#include <memory>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nscapi/settings/kvp_map.hpp>

#include "error_handler_interface.hpp"
#include "event_store.hpp"
#include "result_store.hpp"
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
                   const std::string &value, const std::string &reason, bool seed = true);
  void ensure_user(const nscapi::settings_helper::settings_registry &settings, const std::string &path, const std::string &user, const std::string &role,
                   const std::string &value, const std::string &reason);

  void prepareShutdown();
  bool unloadModule();
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
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
  std::shared_ptr<result_store> results_;
  // `result_key_` and `local_hostname_` are rebuilt by loadModuleEx, which a
  // settings reload re-enters on the live module while results keep arriving
  // on the channel. Swapping the formatter's token vector under a reader
  // iterating it is a use-after-free, so both are only ever touched under
  // this lock (result_store has its own).
  mutable boost::mutex result_config_mutex_;
  result_key_formatter result_key_;
  // This machine's name, used for results whose header names no sender
  // (locally scheduled checks, mostly).
  std::string local_hostname_;
  // The submission channel actually registered with the core, which only
  // happens on a full start: the core cannot unregister one, so a reload can
  // neither start listening on a channel nor move to a different one. Empty
  // until the cache has been switched on across a restart.
  std::string registered_result_channel_;
  std::shared_ptr<Mongoose::Server> server;

  web_server::user_config users_;
  std::atomic<unsigned long> last_log_index;
};
