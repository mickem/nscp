// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "DotnetPlugins.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

namespace sh = nscapi::settings_helper;
namespace fs = boost::filesystem;

namespace {

const char *const default_factory = "NSCP.Plugin.PluginFactory";

void NSCP_DOTNET_CALL append_to_string(void *wctx, const std::uint8_t *data, std::int32_t len) {
  if (wctx == nullptr || data == nullptr || len <= 0) return;
  static_cast<std::string *>(wctx)->append(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len));
}

std::string collect(dotnet::managed_describe_fn describe, void *handle) {
  std::string out;
  if (describe && handle) describe(handle, &append_to_string, &out);
  return out;
}

const std::uint8_t *bytes_of(const std::string &s) { return reinterpret_cast<const std::uint8_t *>(s.data()); }
std::int32_t length_of(const std::string &s) { return static_cast<std::int32_t>(s.size()); }

template <typename Fn>
bool resolve(dotnet::host &host, const fs::path &assembly, const char *name, Fn &out, std::string &error) {
  void *fn = host.get_function(assembly, dotnet::bridge_type_name, name, error);
  if (fn == nullptr) return false;
  out = reinterpret_cast<Fn>(fn);
  return true;
}

bool is_enabled_word(const std::string &value) {
  return value.empty() || boost::iequals(value, "enabled") || boost::iequals(value, "1") || boost::iequals(value, "true") || boost::iequals(value, "on") ||
         boost::iequals(value, "yes");
}

}  // namespace

bool DotnetPlugins::is_disabled(const std::string &value) {
  return boost::iequals(value, "disabled") || boost::iequals(value, "0") || boost::iequals(value, "false") || boost::iequals(value, "off") ||
         boost::iequals(value, "no");
}

fs::path DotnetPlugins::resolve_assembly(const fs::path &root, const std::string &alias, const std::string &value,
                                         const std::function<bool(const fs::path &)> &exists) {
  const std::string file = is_enabled_word(value) ? alias : value;
  fs::path candidate = dotnet::to_path(file);
  if (!candidate.is_absolute()) candidate = root / candidate;
  // As configured first: a plugin may well be an .exe assembly or carry no
  // extension at all.
  if (exists(candidate)) return candidate;
  const bool names_assembly_file = boost::iends_with(file, ".dll") || boost::iends_with(file, ".exe");
  if (names_assembly_file) return candidate;
  fs::path with_dll = candidate;
  with_dll += ".dll";
  return with_dll;  // The usual case; also what the error names when nothing exists.
}

bool DotnetPlugins::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "dotnet");
    settings_path_ = settings.alias().get_settings_path("");

    // clang-format off
    settings.alias().add_path_to_settings()
      ("DOTNET PLUGINS", "Section for the DotnetPlugins module: hosts modules written for .NET.")

      ("plugins", sh::fun_values_path([this] (auto key, auto value) { this->add_plugin(key, value); }),
        ".NET plugins", "Plugins to load: <alias> = <assembly> where <assembly> is a file in the plugin path (the .dll extension is optional), "
        "an absolute path, 'enabled' to load an assembly named after the alias, or 'disabled' to skip it. Set the factory class for one "
        "plugin under plugins/<alias> with the key 'factory class'.",
        "PLUGIN", "A .NET plugin assembly to load")
      ;

    settings.alias().add_key_to_settings()
      .add_string("plugin path", sh::path_key(&plugin_path_, "${module-path}/dotnet"),
        "Plugin path", "Folder holding NSCP.Core.dll (the managed plugin API shipped with NSClient++) and the .NET plugin assemblies. "
        "Falls back to ${exe-path}/modules/dotnet when the folder does not exist.")
      .add_string("runtime path", sh::string_key(&runtime_root_, ""),
        ".NET runtime root", "Root folder of the .NET installation to host (the folder containing 'host/fxr'). Leave empty to use "
        "DOTNET_ROOT, the registered install location or the platform's default install folders.", true)
      .add_string("factory class", sh::string_key(&default_factory_, default_factory),
        "Default factory class", "Fully qualified name of the IPluginFactory implementation instantiated in a plugin assembly unless the "
        "plugin overrides it.", true)
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    if (configured_.empty()) {
      NSC_DEBUG_MSG_STD("No .NET plugins configured under " + settings_path_ + "/plugins");
      return true;
    }
    if (mode == NSCAPI::dontStart) {
      NSC_DEBUG_MSG_STD("Not loading .NET plugins (module loaded without start)");
      return true;
    }

    root_ = resolve_plugin_root();
    if (!start_runtime() || !resolve_bridge()) return true;  // Reported already; keep the module loaded so the error stays visible.

    // Each configured plugin gets its own section, plugins/<alias>, registered
    // like any other key so it shows up in the settings UI, in
    // `nscp settings --generate` and in the reference, and so a misspelt key
    // there is reported instead of silently ignored.
    settings.clear();
    std::map<std::string, std::string> factories;
    for (const auto &kv : configured_) {
      // clang-format off
      settings.alias().add_path_to_settings()
        ("plugins/" + kv.first, "Plugin " + kv.first, "Settings for the .NET plugin " + kv.first + " (its assembly is configured under plugins).")
        ;
      settings.alias().add_key_to_settings("plugins/" + kv.first)
        .add_string("factory class", sh::string_key(&factories[kv.first], default_factory_),
          "Factory class", "Fully qualified name of the IPluginFactory implementation to instantiate in this plugin's assembly; overrides the "
          "module's default factory class.", true)
        ;
      // clang-format on
    }
    settings.register_all();
    settings.notify();
    settings.clear();

    for (const auto &kv : configured_) {
      plugin_entry entry;
      entry.alias = kv.first;
      boost::system::error_code ec;
      entry.assembly = dotnet::path_to_utf8(resolve_assembly(root_, kv.first, kv.second, [&ec](const fs::path &p) { return fs::is_regular_file(p, ec); }));
      entry.factory = factories[kv.first];
      if (load_plugin(entry, mode)) {
        std::lock_guard<std::mutex> lock(plugins_mutex_);
        plugins_.push_back(entry);
      }
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("load");
    return false;
  }
  return true;
}

void DotnetPlugins::add_plugin(const std::string &key, const std::string &value) {
  if (key.empty()) return;
  if (is_disabled(value)) {
    NSC_DEBUG_MSG_STD("Skipping disabled .NET plugin: " + key);
    return;
  }
  configured_[key] = value;
}

fs::path DotnetPlugins::resolve_plugin_root() const {
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

bool DotnetPlugins::start_runtime() {
  host_ = dotnet::host::instance();
  if (host_->initialized()) return true;
  const dotnet::hostfxr_location location = dotnet::find_hostfxr(dotnet::default_roots(runtime_root_));
  if (!location.found()) {
    NSC_LOG_ERROR("No " + std::string(dotnet::architecture_name(dotnet::process_architecture())) + " .NET runtime found (looked for host/fxr/<version>/" +
                  dotnet::hostfxr_library_name() + " under: " + boost::algorithm::join(location.searched, ", ") +
                  "). Install the .NET runtime for this architecture or set 'runtime path' in " + settings_path_ + ".");
    return false;
  }
  const fs::path runtimeconfig = root_ / dotnet::bridge_runtimeconfig;
  boost::system::error_code ec;
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
  return true;
}

bool DotnetPlugins::resolve_bridge() {
  if (bridge_.load != nullptr) return true;
  const fs::path bridge_assembly = root_ / dotnet::bridge_assembly;
  std::string error;
  bridge_functions fns;
  const bool ok = resolve(*host_, bridge_assembly, "Load", fns.load, error) && resolve(*host_, bridge_assembly, "Start", fns.start, error) &&
                  resolve(*host_, bridge_assembly, "Unload", fns.unload, error) && resolve(*host_, bridge_assembly, "Describe", fns.describe, error) &&
                  resolve(*host_, bridge_assembly, "Query", fns.query, error) && resolve(*host_, bridge_assembly, "Submit", fns.submit, error) &&
                  resolve(*host_, bridge_assembly, "Exec", fns.exec, error) && resolve(*host_, bridge_assembly, "Message", fns.message, error) &&
                  resolve(*host_, bridge_assembly, "HasMessageHandler", fns.has_message, error);
  if (!ok) {
    NSC_LOG_ERROR("Failed to load the managed plugin API from " + dotnet::path_to_utf8(bridge_assembly) + ": " + error);
    return false;
  }
  bridge_ = fns;
  return true;
}

bool DotnetPlugins::load_plugin(plugin_entry &entry, NSCAPI::moduleLoadMode mode) {
  boost::system::error_code ec;
  if (!fs::is_regular_file(dotnet::to_path(entry.assembly), ec)) {
    NSC_LOG_ERROR("Plugin " + entry.alias + " not found: " + entry.assembly);
    return false;
  }
  entry.handle = bridge_.load(&DotnetPlugins::core_callback, this, entry.assembly.c_str(), entry.factory.c_str(), entry.alias.c_str(), get_id());
  if (entry.handle == nullptr) {
    NSC_LOG_ERROR("Failed to load plugin " + entry.alias + " from " + entry.assembly + " (see previous errors)");
    return false;
  }
  std::vector<std::string> info;
  boost::split(info, collect(bridge_.describe, entry.handle), boost::is_any_of("\n"));
  entry.name = info.size() > 0 ? info[0] : entry.alias;
  entry.version = info.size() > 1 ? info[1] : "";
  if (bridge_.start(entry.handle, mode) == 0) {
    NSC_LOG_ERROR("Plugin " + entry.alias + " (" + entry.name + ") failed to start");
    bridge_.unload(entry.handle);
    entry.handle = nullptr;
    return false;
  }
  entry.messages = bridge_.has_message(entry.handle) == 1;
  NSC_DEBUG_MSG_STD("Loaded .NET plugin " + entry.alias + ": " + entry.name + " " + entry.version + " from " + entry.assembly +
                    (entry.messages ? " (receives log entries)" : ""));
  return true;
}

bool DotnetPlugins::unloadModule() {
  std::vector<plugin_entry> plugins;
  {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    plugins.swap(plugins_);
  }
  for (plugin_entry &entry : plugins) {
    if (entry.handle && bridge_.unload) {
      try {
        bridge_.unload(entry.handle);
      } catch (...) {
        NSC_LOG_ERROR_EX("unload " + entry.alias);
      }
    }
  }
  configured_.clear();
  return true;
}

void DotnetPlugins::query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                   const PB::Commands::QueryRequestMessage &request_message) {
  const std::string command = boost::algorithm::to_lower_copy(request.command());
  std::vector<plugin_entry> plugins;
  {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    plugins = plugins_;
  }
  if (plugins.empty() || bridge_.query == nullptr) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No .NET plugin loaded to handle: " + command);
  }
  // The plugin sees a complete QueryRequestMessage holding just this request.
  PB::Commands::QueryRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  for (plugin_entry &entry : plugins) {
    std::string response_buffer;
    const std::int32_t rc =
        bridge_.query(entry.handle, command.c_str(), bytes_of(request_buffer), length_of(request_buffer), &append_to_string, &response_buffer);
    if (rc == dotnet::query_ignored) continue;
    if (rc != dotnet::query_handled) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Command " + command + " failed in .NET plugin " + entry.alias);
    }
    PB::Commands::QueryResponseMessage reply;
    if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
      return nscapi::protobuf::functions::set_response_bad(*response, ".NET plugin " + entry.alias + " returned no response for " + command);
    }
    const std::string original_command = response->command();
    response->Swap(reply.mutable_payload(0));
    if (response->command().empty()) response->set_command(original_command);
    return;
  }
  nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + command);
}

void DotnetPlugins::handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                                       PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  std::vector<plugin_entry> plugins;
  {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    plugins = plugins_;
  }
  if (plugins.empty() || bridge_.submit == nullptr) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No .NET plugin loaded to handle channel: " + channel);
  }
  PB::Commands::SubmitRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.set_channel(channel);
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  for (plugin_entry &entry : plugins) {
    std::string response_buffer;
    const std::int32_t rc =
        bridge_.submit(entry.handle, channel.c_str(), bytes_of(request_buffer), length_of(request_buffer), &append_to_string, &response_buffer);
    if (rc == dotnet::query_ignored) continue;
    if (rc != dotnet::query_handled) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Submission on " + channel + " failed in .NET plugin " + entry.alias);
    }
    PB::Commands::SubmitResponseMessage reply;
    if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
      // A handler that answers nothing has still consumed the submission.
      return nscapi::protobuf::functions::set_response_good(*response, "Submitted to .NET plugin " + entry.alias);
    }
    const std::string original_command = response->command();
    response->Swap(reply.mutable_payload(0));
    if (response->command().empty()) response->set_command(original_command);
    return;
  }
  nscapi::protobuf::functions::set_response_bad(*response, "No .NET plugin handles channel: " + channel);
}

bool DotnetPlugins::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                                    PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message) {
  std::vector<plugin_entry> plugins;
  {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    plugins = plugins_;
  }
  const std::string command = request.command();
  if (target_mode == NSCAPI::target_module && (command.empty() || command == "help" || command == "list")) {
    std::string text = "Loaded .NET plugins:";
    for (const plugin_entry &entry : plugins) text += "\n  " + entry.alias + ": " + entry.name + " " + entry.version + " (" + entry.assembly + ")";
    if (plugins.empty()) text += " none";
    nscapi::protobuf::functions::set_response_good(*response, text);
    return true;
  }
  if (plugins.empty() || bridge_.exec == nullptr) return false;
  PB::Commands::ExecuteRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  for (plugin_entry &entry : plugins) {
    std::string response_buffer;
    const std::int32_t rc = bridge_.exec(entry.handle, request_message.header().recipient_id().c_str(), command.c_str(), bytes_of(request_buffer),
                                         length_of(request_buffer), &append_to_string, &response_buffer);
    if (rc == dotnet::query_ignored) continue;
    if (rc != dotnet::query_handled) {
      nscapi::protobuf::functions::set_response_bad(*response, "Command " + command + " failed in .NET plugin " + entry.alias);
      return true;
    }
    PB::Commands::ExecuteResponseMessage reply;
    if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
      nscapi::protobuf::functions::set_response_bad(*response, ".NET plugin " + entry.alias + " returned no response for " + command);
      return true;
    }
    const std::string original_command = response->command();
    response->Swap(reply.mutable_payload(0));
    if (response->command().empty()) response->set_command(original_command);
    return true;
  }
  return false;
}

void DotnetPlugins::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  // Never log from here: the entry would come straight back to this handler.
  // Entries written by the plugins themselves are not echoed to them either,
  // or a message handler that logs would keep the logger busy forever.
  // This runs on the logger's thread for every line the agent writes, so do
  // nothing at all unless a loaded plugin actually exposes a message handler.
  std::vector<plugin_entry> targets;
  {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    for (const plugin_entry &entry : plugins_) {
      if (message.sender() == entry.alias) return;
    }
    for (const plugin_entry &entry : plugins_) {
      if (entry.messages) targets.push_back(entry);
    }
  }
  if (targets.empty() || bridge_.message == nullptr) return;
  PB::Log::LogEntry single;
  single.add_entry()->CopyFrom(message);
  const std::string buffer = single.SerializeAsString();
  for (plugin_entry &entry : targets) {
    try {
      bridge_.message(entry.handle, bytes_of(buffer), length_of(buffer));
    } catch (...) {
      // Loggers cannot log.
    }
  }
}

std::int32_t DotnetPlugins::dispatch(std::int32_t op, const char *str, const std::string &request, std::string &response) {
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
    default:
      NSC_LOG_ERROR("Unknown core operation requested by .NET plugin: " + std::to_string(op));
      return 0;
  }
}

std::int32_t NSCP_DOTNET_CALL DotnetPlugins::core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len,
                                                           dotnet::write_fn write, void *wctx) {
  DotnetPlugins *self = static_cast<DotnetPlugins *>(ctx);
  if (self == nullptr) return 0;
  try {
    const std::string request = (data != nullptr && len > 0) ? std::string(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len)) : std::string();
    std::string response;
    const std::int32_t rc = self->dispatch(op, str, request, response);
    if (write != nullptr && !response.empty()) write(wctx, bytes_of(response), length_of(response));
    return rc;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("core call from .NET plugin", e);
  } catch (...) {
    NSC_LOG_ERROR_EX("core call from .NET plugin");
  }
  return 0;
}
