// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/command_alias.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/snapshot.hpp>
#include <string>

#include "commands.hpp"
#include "script_interface.hpp"

class CheckExternalScripts : public nscapi::impl::simple_plugin {
 public:
  // The scalar settings every query reads.
  //
  // loadModuleEx is re-entered with reloadStart on the live module, so these
  // used to be rewritten field by field while handle_command was midway
  // through building an exec_arguments out of them - a check could be launched
  // with the new root and the old timeout, or with a torn root_ string.
  struct config {
    unsigned int timeout = 60;
    bool kill_tree = false;
    std::string root;
    bool allow_args = false;
    bool allow_nasty = false;
  };

 private:
  nscapi::settings::snapshot<config> config_;

  // Both are replaced wholesale by a reload while queries are running, so both
  // are published rather than assigned: a query takes its own reference and
  // the generation it is using stays alive until it is done with it. Read only
  // through get_provider() / get_aliases(); loadModuleEx builds the
  // replacements in locals and publishes them once they are complete.
  std::shared_ptr<script_provider_interface> provider_;
  std::shared_ptr<alias::command_handler> aliases_;

  std::shared_ptr<script_provider_interface> get_provider() const { return std::atomic_load(&provider_); }
  std::shared_ptr<alias::command_handler> get_aliases() const { return std::atomic_load(&aliases_); }

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
  void handle_command(const config &cfg, const commands::command_object &cd, const std::list<std::string> &args,
                      PB::Commands::QueryResponseMessage_Response *response);
  void handle_alias(const alias::command_object &cd, const std::list<std::string> &args, PB::Commands::QueryResponseMessage_Response *response);
  // These run from loadModuleEx and its settings callbacks only, and they fill
  // the generation being built - which is not the one queries are reading yet -
  // so the target is passed in rather than taken from the member.
  void addAllScriptsFrom(const std::shared_ptr<script_provider_interface> &provider, bool allow_args, std::string str_path);
  void add_command(const std::shared_ptr<script_provider_interface> &provider, bool allow_args, std::string key, std::string arg);
  void add_alias(const std::shared_ptr<alias::command_handler> &aliases, std::string key, std::string command);
  void add_wrapping(const std::shared_ptr<script_provider_interface> &provider, bool allow_args, std::string key, std::string command);
};
