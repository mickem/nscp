// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <cstdint>
#include <dotnet/bridge.hpp>
#include <dotnet/host.hpp>
#include <dotnet/runtime.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

/**
 * Runs PowerShell scripts inside NSClient++, the way LUAScript and PythonScript
 * run Lua and Python: a script registers the checks it answers, and can query,
 * submit, read settings and log back through the agent.
 *
 * PowerShell is a .NET library, so the engine lives on the managed side of the
 * same boundary the DotnetPlugins module uses: this module starts the .NET
 * runtime through hostfxr (see include/dotnet/host.hpp), loads NSCP.Core.dll
 * (the managed plugin API) and hands it one fixed plugin -
 * NSCP.PowerShell.dll, the script host in libs/powershell-host. That plugin
 * reads the configured scripts, creates a runspace per script and routes the
 * registered commands into them.
 *
 * The PowerShell engine itself is not shipped: it is loaded from an installed
 * PowerShell 7 (Windows PowerShell 5.1 is built on the .NET Framework and
 * cannot be loaded into a .NET 8 process at all).
 */
class PowerShellScript : public nscapi::impl::simple_plugin {
 public:
  PowerShellScript() = default;
  virtual ~PowerShellScript() = default;

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);

  // `nscp powershell <cmd>` arrives with an empty command and the sub command
  // as the first argument, while `--exec <cmd>` puts it in the command. Pick
  // the one that is there, and default to "help" when neither is.
  static std::string cli_command(const std::string &command, const std::vector<std::string> &arguments, bool module_target);

  // A script reports its result the way a Nagios plugin does, as
  // "message|perfdata", and the host hands that through as the line message.
  // Split it and parse the perf data with the same parser the external-script
  // path uses, so it reaches consumers as structured values. Lines that
  // already carry perf data, or carry no '|', are left alone.
  static void split_perfdata(PB::Commands::QueryResponseMessage::Response *response);

  // The core callback handed to managed code (see include/dotnet/bridge.hpp).
  static std::int32_t NSCP_DOTNET_CALL core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len,
                                                     dotnet::write_fn write, void *wctx);

 private:
  bool start_runtime();
  bool load_host(NSCAPI::moduleLoadMode mode);
  // Start the host if it is not up yet. With no scripts configured nothing is
  // started on load, but `nscp powershell execute --script ...` still has to
  // work - that is the one command that runs a script the settings never
  // mentioned.
  bool ensure_host();
  boost::filesystem::path resolve_plugin_root() const;
  std::int32_t dispatch(std::int32_t op, const char *str, const std::string &request, std::string &response);

  std::string settings_path_;
  std::string alias_;
  std::string runtime_root_;
  std::string plugin_path_;
  // Registered here so they are documented and validated like any other key;
  // the managed host reads (and expands) them itself.
  std::string script_path_;
  std::string powershell_home_;
  std::string execution_policy_;
  boost::filesystem::path root_;
  // Only registered so the scripts show up in the settings UI, in
  // `nscp settings --generate` and in the reference; the managed host reads
  // the same section itself (it needs the file contents anyway).
  std::map<std::string, std::string> scripts_;

  // Serialises starting the runtime and loading the host: a reload and a
  // command-line command can both reach ensure_host().
  std::mutex start_mutex_;
  // Guards handle_: queries and submissions arrive on other threads than the
  // one loading and unloading the module.
  std::mutex handle_mutex_;
  void *handle_ = nullptr;
  std::string host_name_;
  std::string host_version_;
  dotnet::bridge_functions bridge_;
  std::shared_ptr<dotnet::host> host_;
};
