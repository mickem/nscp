// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "PowerShellScript.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

namespace sh = nscapi::settings_helper;
namespace fs = boost::filesystem;

namespace {

// The managed script host: one fixed plugin, unlike DotnetPlugins where the
// assembly and the factory are what the user configures.
const char *const host_assembly = "NSCP.PowerShell.dll";
const char *const host_factory = "NSCP.PowerShell.PluginFactory";

// Kept in sync with the same text in libs/powershell-host/PowerShellPlugin.cs,
// which is what answers `help` once the host is up.
const char *const cli_usage =
    "Usage: nscp powershell [help|list|execute]\n"
    "  help                       Show this text\n"
    "  list                       List the loaded scripts and what they answer\n"
    "  execute --script <file>    Run a script's main function and print what it returned";

}  // namespace

std::string PowerShellScript::cli_command(const std::string &command, const std::vector<std::string> &arguments, const bool module_target) {
  if (command == "ext-scr" && !arguments.empty()) return arguments[0];
  if (!command.empty()) return command;
  if (module_target && !arguments.empty()) return arguments[0];
  if (module_target) return "help";
  return command;
}

void PowerShellScript::split_perfdata(PB::Commands::QueryResponseMessage::Response *response) {
  for (int i = 0; i < response->lines_size(); ++i) {
    PB::Commands::QueryResponseMessage::Response::Line *line = response->mutable_lines(i);
    if (line->perf_size() > 0) continue;
    const std::string message = line->message();
    const std::string::size_type pos = message.find('|');
    if (pos == std::string::npos) continue;
    line->set_message(message.substr(0, pos));
    nscapi::protobuf::functions::parse_performance_data(line, message.substr(pos + 1));
  }
}

bool PowerShellScript::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  try {
    // A settings reload calls this again on the live module: let go of the
    // scripts the previous call loaded before reading the section again, or
    // every reload leaves another host (and another set of runspaces) behind.
    unloadModule();

    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "powershell");
    settings_path_ = settings.alias().get_settings_path("");
    alias_ = sh::alias_extension::get_alias(alias, "powershell");

    // clang-format off
    settings.alias().add_path_to_settings()
      ("POWERSHELL", "Section for the PowerShellScript module: runs PowerShell scripts inside NSClient++.")

      ("scripts", sh::string_map_path(&scripts_),
        "PowerShell scripts", "A list of scripts to load: <alias> = <script.ps1>, where the script is looked for under the script path "
        "(and in a 'powershell' folder below it) unless it is an absolute path. Every script runs once on load so it can register the "
        "checks it answers.",
        "SCRIPT", "A PowerShell script to load")
      ;

    settings.alias().add_key_to_settings()
      .add_string("script path", sh::path_key(&script_path_, "${scripts}"),
        "Script path", "Folder the configured scripts are looked for in (a 'powershell' folder below it is searched as well).")
      .add_string("execution policy", sh::string_key(&execution_policy_, ""),
        "Execution policy", "PowerShell execution policy to run the scripts under (Restricted, AllSigned, RemoteSigned, Unrestricted, Bypass). "
        "Leave empty to use the machine's policy, which is what pwsh itself would apply. Windows only: there is no execution policy on "
        "other platforms.", true)
      .add_string("powershell path", sh::string_key(&powershell_home_, ""),
        "PowerShell installation", "Folder of the PowerShell 7 installation to load the engine from (the folder holding pwsh and "
        "System.Management.Automation.dll). Leave empty to use the PSHOME environment variable, the pwsh on PATH or the platform's "
        "default install folders. Windows PowerShell 5.1 cannot be used: it is built on the .NET Framework.", true)
      .add_string("plugin path", sh::path_key(&plugin_path_, "${module-path}/dotnet"),
        "Plugin path", "Folder holding NSCP.Core.dll and NSCP.PowerShell.dll (the managed script host shipped with NSClient++). "
        "Falls back to ${exe-path}/modules/dotnet when the folder does not exist.", true)
      .add_string("runtime path", sh::string_key(&runtime_root_, ""),
        ".NET runtime root", "Root folder of the .NET installation to host (the folder containing 'host/fxr'). Leave empty to use "
        "DOTNET_ROOT, the registered install location or the platform's default install folders.", true)
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    if (scripts_.empty()) {
      // Nothing to run: do not start a .NET runtime, and do not complain about
      // a missing PowerShell, for a module that was merely enabled.
      NSC_DEBUG_MSG_STD("No PowerShell scripts configured under " + settings_path_ + "/scripts");
      return true;
    }
    if (mode == NSCAPI::dontStart) {
      NSC_DEBUG_MSG_STD("Not starting the PowerShell script host (module loaded without start)");
      return true;
    }

    std::lock_guard<std::mutex> starting(start_mutex_);
    root_ = resolve_plugin_root();
    if (!start_runtime()) return true;  // Reported already; keep the module loaded so the error stays visible.
    load_host(mode);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("load");
    return false;
  }
  return true;
}

fs::path PowerShellScript::resolve_plugin_root() const {
  boost::system::error_code ec;
  fs::path root = dotnet::to_path(plugin_path_);
  if (!root.empty() && fs::is_directory(root, ec)) return root;
  const fs::path fallback = dotnet::to_path(get_core()->expand_path("${exe-path}")) / "modules" / "dotnet";
  if (fs::is_directory(fallback, ec)) {
    NSC_DEBUG_MSG_STD("Plugin path " + dotnet::path_to_utf8(root) + " not found, using " + dotnet::path_to_utf8(fallback));
    return fallback;
  }
  return root;
}

bool PowerShellScript::start_runtime() {
  host_ = dotnet::host::instance();
  const fs::path runtimeconfig = root_ / dotnet::bridge_runtimeconfig;
  boost::system::error_code ec;
  if (!host_->initialized()) {
    const dotnet::hostfxr_location location = dotnet::find_hostfxr(dotnet::default_roots(runtime_root_));
    if (!location.found()) {
      NSC_LOG_ERROR("No " + std::string(dotnet::architecture_name(dotnet::process_architecture())) + " .NET runtime found (looked for host/fxr/<version>/" +
                    dotnet::hostfxr_library_name() + " under: " + boost::algorithm::join(location.searched, ", ") +
                    "). Install the .NET runtime for this architecture or set 'runtime path' in " + settings_path_ + ".");
      return false;
    }
    if (!fs::is_regular_file(runtimeconfig, ec)) {
      NSC_LOG_ERROR("The managed plugin API is missing: " + dotnet::path_to_utf8(runtimeconfig) +
                    " not found. NSCP.Core.dll and its runtimeconfig.json must be installed in the plugin path (" + dotnet::path_to_utf8(root_) + ").");
      return false;
    }
    std::string error;
    if (!host_->initialize(location, runtimeconfig, error)) {
      NSC_LOG_ERROR("Failed to start the .NET runtime from " + dotnet::path_to_utf8(location.library) + ": " + error);
      return false;
    }
    NSC_DEBUG_MSG_STD("Started .NET runtime: " + host_->describe());
  }
  const fs::path bridge_assembly = root_ / dotnet::bridge_assembly;
  std::string error;
  if (!dotnet::resolve_bridge(*host_, bridge_assembly, bridge_, error)) {
    NSC_LOG_ERROR("Failed to load the managed plugin API from " + dotnet::path_to_utf8(bridge_assembly) + ": " + error);
    return false;
  }
  return true;
}

bool PowerShellScript::load_host(NSCAPI::moduleLoadMode mode) {
  const fs::path assembly = root_ / host_assembly;
  boost::system::error_code ec;
  if (!fs::is_regular_file(assembly, ec)) {
    NSC_LOG_ERROR("The PowerShell script host is missing: " + dotnet::path_to_utf8(assembly) +
                  " not found. It is built with the dotnet SDK and installed next to NSCP.Core.dll.");
    return false;
  }
  const std::string path = dotnet::path_to_utf8(assembly);
  // Pass the resolved alias, not the configured one: the managed host derives
  // its settings root from it ("/settings/" + alias) and must land on the same
  // section this module just registered.
  void *handle = bridge_.load(&PowerShellScript::core_callback, this, path.c_str(), host_factory, alias_.c_str(), get_id());
  if (handle == nullptr) {
    NSC_LOG_ERROR("Failed to load the PowerShell script host from " + path + " (see previous errors)");
    return false;
  }
  std::vector<std::string> info;
  boost::split(info, dotnet::describe_plugin(bridge_.describe, handle), boost::is_any_of("\n"));
  if (bridge_.start(handle, mode) == 0) {
    NSC_LOG_ERROR("The PowerShell script host failed to start (see previous errors)");
    bridge_.unload(handle);
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    handle_ = handle;
    host_name_ = info.size() > 0 ? info[0] : "PowerShell";
    host_version_ = info.size() > 1 ? info[1] : "";
  }
  NSC_DEBUG_MSG_STD("Loaded the PowerShell script host: " + host_name_ + " " + host_version_ + " from " + path);
  return true;
}

bool PowerShellScript::ensure_host() {
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    if (handle_ != nullptr) return true;
  }
  std::lock_guard<std::mutex> starting(start_mutex_);
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    if (handle_ != nullptr) return true;
  }
  if (root_.empty()) root_ = resolve_plugin_root();
  return start_runtime() && load_host(NSCAPI::normalStart);
}

bool PowerShellScript::unloadModule() {
  void *handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    std::swap(handle, handle_);
  }
  if (handle && bridge_.unload) {
    try {
      bridge_.unload(handle);
    } catch (...) {
      NSC_LOG_ERROR_EX("unload");
    }
  }
  scripts_.clear();
  return true;
}

void PowerShellScript::query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                      const PB::Commands::QueryRequestMessage &request_message) {
  const std::string command = boost::algorithm::to_lower_copy(request.command());
  void *handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    handle = handle_;
  }
  if (handle == nullptr || bridge_.query == nullptr) {
    return nscapi::protobuf::functions::set_response_bad(*response, "The PowerShell script host is not loaded, cannot run: " + command);
  }
  // The host sees a complete QueryRequestMessage holding just this request.
  PB::Commands::QueryRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  std::string response_buffer;
  const std::int32_t rc =
      bridge_.query(handle, command.c_str(), dotnet::bytes_of(request_buffer), dotnet::length_of(request_buffer), &dotnet::append_to_string, &response_buffer);
  if (rc == dotnet::query_ignored) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + command);
  }
  if (rc != dotnet::query_handled) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Command " + command + " failed in the PowerShell script host");
  }
  PB::Commands::QueryResponseMessage reply;
  if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
    return nscapi::protobuf::functions::set_response_bad(*response, "The PowerShell script host returned no response for " + command);
  }
  const std::string original_command = response->command();
  response->Swap(reply.mutable_payload(0));
  if (response->command().empty()) response->set_command(original_command);
  split_perfdata(response);
}

void PowerShellScript::handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  void *handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    handle = handle_;
  }
  if (handle == nullptr || bridge_.submit == nullptr) {
    return nscapi::protobuf::functions::set_response_bad(*response, "The PowerShell script host is not loaded, cannot handle channel: " + channel);
  }
  PB::Commands::SubmitRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.set_channel(channel);
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  std::string response_buffer;
  const std::int32_t rc =
      bridge_.submit(handle, channel.c_str(), dotnet::bytes_of(request_buffer), dotnet::length_of(request_buffer), &dotnet::append_to_string, &response_buffer);
  if (rc == dotnet::query_ignored) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No PowerShell script handles channel: " + channel);
  }
  if (rc != dotnet::query_handled) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Submission on " + channel + " failed in the PowerShell script host");
  }
  PB::Commands::SubmitResponseMessage reply;
  if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
    // A handler that answers nothing has still consumed the submission.
    return nscapi::protobuf::functions::set_response_good(*response, "Submitted to the PowerShell script host");
  }
  const std::string original_command = response->command();
  response->Swap(reply.mutable_payload(0));
  if (response->command().empty()) response->set_command(original_command);
}

bool PowerShellScript::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                                       PB::Commands::ExecuteResponseMessage::Response *response,
                                       const PB::Commands::ExecuteRequestMessage &request_message) {
  const std::vector<std::string> arguments(request.arguments().begin(), request.arguments().end());
  const bool module_target = target_mode == NSCAPI::target_module;
  const std::string command = cli_command(request.command(), arguments, module_target);
  if (command.empty()) return false;

  // `execute` runs a script that need not be configured at all, so the host is
  // started on demand here rather than only when scripts are configured.
  ensure_host();
  void *handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    handle = handle_;
  }
  if (handle == nullptr || bridge_.exec == nullptr) {
    if (!module_target) return false;
    // The host answers `help` itself when it is up; with no scripts configured
    // it is never started, and a bare `nscp powershell` should still say what
    // the module can do rather than only that nothing is loaded.
    if (command == "help") {
      nscapi::protobuf::functions::set_response_good(*response, std::string(cli_usage) + "\n\nNo PowerShell scripts are loaded.");
      return true;
    }
    nscapi::protobuf::functions::set_response_bad(*response, "The PowerShell script host is not loaded (see the log for why)\n" + std::string(cli_usage));
    return true;
  }

  // Hand the host the sub command in the command field, with the argument it
  // was read from removed, so it sees the same shape either way in.
  PB::Commands::ExecuteRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  PB::Commands::ExecuteRequestMessage::Request *payload = single.add_payload();
  payload->CopyFrom(request);
  payload->set_command(command);
  if (request.command() != command || (request.command() == "ext-scr" && !arguments.empty())) {
    payload->clear_arguments();
    for (std::size_t i = 1; i < arguments.size(); ++i) payload->add_arguments(arguments[i]);
  }
  const std::string request_buffer = single.SerializeAsString();

  std::string response_buffer;
  const std::int32_t rc = bridge_.exec(handle, request_message.header().recipient_id().c_str(), command.c_str(), dotnet::bytes_of(request_buffer),
                                       dotnet::length_of(request_buffer), &dotnet::append_to_string, &response_buffer);
  if (rc == dotnet::query_ignored) return false;
  if (rc != dotnet::query_handled) {
    nscapi::protobuf::functions::set_response_bad(*response, "Command " + command + " failed in the PowerShell script host");
    return true;
  }
  PB::Commands::ExecuteResponseMessage reply;
  if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
    nscapi::protobuf::functions::set_response_bad(*response, "The PowerShell script host returned no response for " + command);
    return true;
  }
  const std::string original_command = response->command();
  response->Swap(reply.mutable_payload(0));
  if (response->command().empty()) response->set_command(original_command);
  return true;
}

std::int32_t PowerShellScript::dispatch(std::int32_t op, const char *str, const std::string &request, std::string &response) {
  const std::string text = str ? str : "";
  switch (op) {
    case dotnet::op_query:
      return get_core()->query(request, response) ? 1 : 0;
    case dotnet::op_exec:
      return get_core()->exec_command(text, request, response) ? 1 : 0;
    case dotnet::op_submit:
      return get_core()->submit_message(text, request, response) ? 1 : 0;
    case dotnet::op_reload:
      return get_core()->reload(text) ? 1 : 0;
    case dotnet::op_settings:
      return get_core()->settings_query(request, response) ? 1 : 0;
    case dotnet::op_registry:
      return get_core()->registry_query(request, response) ? 1 : 0;
    case dotnet::op_log:
      get_core()->log(request);
      return 1;
    case dotnet::op_expand_path:
      response = get_core()->expand_path(text);
      return 1;
    default:
      NSC_LOG_ERROR("Unknown core operation requested by the PowerShell script host: " + std::to_string(op));
      return 0;
  }
}

std::int32_t NSCP_DOTNET_CALL PowerShellScript::core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len,
                                                              dotnet::write_fn write, void *wctx) {
  PowerShellScript *self = static_cast<PowerShellScript *>(ctx);
  if (self == nullptr) return 0;
  try {
    const std::string request = (data != nullptr && len > 0) ? std::string(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len)) : std::string();
    std::string response;
    const std::int32_t rc = self->dispatch(op, str, request, response);
    if (write != nullptr && !response.empty()) write(wctx, dotnet::bytes_of(response), dotnet::length_of(response));
    return rc;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("core call from the PowerShell script host", e);
  } catch (...) {
    NSC_LOG_ERROR_EX("core call from the PowerShell script host");
  }
  return 0;
}
