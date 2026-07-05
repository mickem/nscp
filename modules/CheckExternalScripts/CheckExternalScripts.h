// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/command_alias.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

#include "commands.hpp"
#include "script_interface.hpp"

class CheckExternalScripts : public nscapi::impl::simple_plugin {
 private:
  std::shared_ptr<script_provider_interface> provider_;
  alias::command_handler aliases_;
  unsigned int timeout;
  bool kill_tree;
  std::string root_;
  bool allowArgs_;
  bool allowNasty_;

 public:
  CheckExternalScripts();
  virtual ~CheckExternalScripts();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void query_fallback(const PB::Commands::QueryRequestMessage_Request &request, PB::Commands::QueryResponseMessage_Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage_Request &request,
                       PB::Commands::ExecuteResponseMessage_Response *response, const PB::Commands::ExecuteRequestMessage &request_message);

 private:
  void handle_command(const commands::command_object &cd, const std::list<std::string> &args, PB::Commands::QueryResponseMessage_Response *response);
  void handle_alias(const alias::command_object &cd, const std::list<std::string> &args, PB::Commands::QueryResponseMessage_Response *response);
  void addAllScriptsFrom(std::string str_path);
  void add_command(std::string key, std::string arg);
  void add_alias(std::string key, std::string command);
  void add_wrapping(std::string key, std::string command);
};
