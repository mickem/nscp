// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/scoped_ptr.hpp>
#include <client/simple_client.hpp>
#include <memory>
#include <nscapi/plugin.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <string>

#include "console_editor.hpp"

struct client_handler : public client::cli_handler {
 private:
  nscapi::core_wrapper *core;
  int plugin_id;

 public:
  client_handler(nscapi::core_wrapper *core, int plugin_id) : core(core), plugin_id(plugin_id) {}
  int get_plugin_id() const { return plugin_id; }
  nscapi::core_wrapper *get_core() const { return core; }
  virtual void output_message(const std::string &msg);
  virtual void log_debug(std::string module, std::string file, int line, std::string msg) const;
  virtual void log_error(std::string module, std::string file, int line, std::string msg) const;
};

class CommandClient : public nscapi::impl::simple_plugin {
  boost::scoped_ptr<client::cli_client> client;

  // Settings, read in loadModuleEx.
  std::string history_file_;
  int history_size_ = 500;
  bool color_ = true;

  // The prompt, when we have a terminal to draw one on. The interactive and
  // non-interactive read loops are genuinely different animals - one is a
  // full-screen line editor, the other polls a pipe - so they are separate
  // functions rather than one with branches.
  void interactive_input_loop(const std::shared_ptr<command_client::console_editor> &editor) const;
  void piped_input_loop() const;
  std::shared_ptr<command_client::console_editor> make_editor() const;

 public:
  CommandClient() {}
  virtual ~CommandClient() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void submitMetrics(const PB::Metrics::MetricsMessage &response);
};
