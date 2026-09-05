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

void append_to_string(void *wctx, const std::uint8_t *data, std::int32_t len) {
  if (wctx == nullptr || data == nullptr || len <= 0) return;
  static_cast<std::string *>(wctx)->append(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len));
}

std::string collect(dotnet::managed_describe_fn describe, void *handle) {
  std::string out;
  if (describe && handle) describe(handle, &append_to_string, &out);
  return out;
}

}  // namespace

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
        "an absolute path, or 'enabled' to load an assembly named after the alias. Set the factory class for one plugin under "
        "plugins/<alias> with the key 'factory class'.",
        "PLUGIN", "A .NET plugin assembly to load")
      ;

    settings.alias().add_key_to_settings()
      .add_string("plugin path", sh::path_key(&plugin_path_, "${module-path}/dotnet"),
        "Plugin path", "Folder holding NSCP.Core.dll (the managed plugin API shipped with NSClient++) and the .NET plugin assemblies. "
        "Falls back to ${exe-path}/modules/dotnet when the folder does not exist.")
      .add_string("runtime path", sh::string_key(&runtime_root_, ""),
        ".NET runtime root", "Root folder of the .NET installation to host (the folder containing 'host/fxr'). Leave empty to use "
        "DOTNET_ROOT, the dotnet launcher on PATH or the platform's default install location.", true)
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
    if (!start_runtime()) return true;  // Reported already; keep the module loaded so the error stays visible.

    for (const auto &kv : configured_) {
      plugin_entry entry;
      entry.alias = kv.first;
      entry.assembly = resolve_assembly(root_, kv.first, kv.second).string();
      entry.factory =
          nscapi::settings_proxy::create(get_id(), get_core())->get_string(settings_path_ + "/plugins/" + kv.first, "factory class", default_factory_);
      if (load_plugin(entry, mode)) plugins_.push_back(entry);
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
  configured_[key] = value;
}

fs::path DotnetPlugins::resolve_plugin_root() const {
  boost::system::error_code ec;
  fs::path root = utf8::cvt<std::string>(plugin_path_);
  if (!root.empty() && fs::is_directory(root, ec)) return root;
  const fs::path fallback = fs::path(get_core()->expand_path("${exe-path}")) / "modules" / "dotnet";
  if (fs::is_directory(fallback, ec)) {
    NSC_DEBUG_MSG_STD("Plugin path " + root.string() + " not found, using " + fallback.string());
    return fallback;
  }
  return root;
}

fs::path DotnetPlugins::resolve_assembly(const fs::path &root, const std::string &alias, const std::string &value) {
  std::string file = value;
  if (file.empty() || boost::iequals(file, "enabled") || boost::iequals(file, "1") || boost::iequals(file, "true")) file = alias;
  fs::path path = utf8::cvt<std::string>(file);
  // Assembly names carry dots (NSCP.Plugin.Sample), so only a trailing .dll
  // counts as the extension already being there.
  if (!boost::iends_with(path.filename().string(), ".dll")) path += ".dll";
  if (path.is_absolute()) return path;
  return root / path;
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
    NSC_LOG_ERROR("The managed plugin API is missing: " + runtimeconfig.string() +
                  " not found. NSCP.Core.dll and its runtimeconfig.json must be installed in the plugin path (" + root_.string() + ").");
    return false;
  }
  std::string error;
  if (!host_->initialize(location, runtimeconfig, error)) {
    NSC_LOG_ERROR("Failed to start the .NET runtime from " + location.library.string() + ": " + error);
    return false;
  }
  NSC_DEBUG_MSG_STD("Started .NET runtime: " + host_->describe());
  return true;
}

bool DotnetPlugins::load_plugin(plugin_entry &entry, NSCAPI::moduleLoadMode mode) {
  boost::system::error_code ec;
  if (!fs::is_regular_file(entry.assembly, ec)) {
    NSC_LOG_ERROR("Plugin " + entry.alias + " not found: " + entry.assembly);
    return false;
  }
  if (bridge_.load == nullptr) {
    const fs::path bridge_assembly = root_ / dotnet::bridge_assembly;
    std::string error;
    bridge_.load = reinterpret_cast<dotnet::managed_load_fn>(host_->get_function(bridge_assembly, dotnet::bridge_type_name, "Load", error));
    if (bridge_.load)
      bridge_.start = reinterpret_cast<dotnet::managed_start_fn>(host_->get_function(bridge_assembly, dotnet::bridge_type_name, "Start", error));
    if (bridge_.start)
      bridge_.unload = reinterpret_cast<dotnet::managed_unload_fn>(host_->get_function(bridge_assembly, dotnet::bridge_type_name, "Unload", error));
    if (bridge_.unload)
      bridge_.describe = reinterpret_cast<dotnet::managed_describe_fn>(host_->get_function(bridge_assembly, dotnet::bridge_type_name, "Describe", error));
    if (bridge_.describe)
      bridge_.query = reinterpret_cast<dotnet::managed_query_fn>(host_->get_function(bridge_assembly, dotnet::bridge_type_name, "Query", error));
    if (bridge_.query == nullptr) {
      bridge_ = bridge_functions();
      NSC_LOG_ERROR("Failed to load the managed plugin API from " + bridge_assembly.string() + ": " + error);
      return false;
    }
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
  NSC_DEBUG_MSG_STD("Loaded .NET plugin " + entry.alias + ": " + entry.name + " " + entry.version + " from " + entry.assembly);
  return true;
}

bool DotnetPlugins::unloadModule() {
  for (plugin_entry &entry : plugins_) {
    if (entry.handle && bridge_.unload) {
      try {
        bridge_.unload(entry.handle);
      } catch (...) {
        NSC_LOG_ERROR_EX("unload " + entry.alias);
      }
    }
    entry.handle = nullptr;
  }
  plugins_.clear();
  configured_.clear();
  return true;
}

void DotnetPlugins::query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                                   const PB::Commands::QueryRequestMessage &request_message) {
  const std::string command = boost::algorithm::to_lower_copy(request.command());
  if (plugins_.empty() || bridge_.query == nullptr) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No .NET plugin loaded to handle: " + command);
  }
  // The plugin sees a complete QueryRequestMessage holding just this request.
  PB::Commands::QueryRequestMessage single;
  single.mutable_header()->CopyFrom(request_message.header());
  single.add_payload()->CopyFrom(request);
  const std::string request_buffer = single.SerializeAsString();

  for (plugin_entry &entry : plugins_) {
    if (entry.handle == nullptr) continue;
    std::string response_buffer;
    const std::int32_t rc = bridge_.query(entry.handle, command.c_str(), reinterpret_cast<const std::uint8_t *>(request_buffer.data()),
                                          static_cast<std::int32_t>(request_buffer.size()), &append_to_string, &response_buffer);
    if (rc == dotnet::query_ignored) continue;
    if (rc != dotnet::query_handled) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Command " + command + " failed in .NET plugin " + entry.alias);
    }
    PB::Commands::QueryResponseMessage reply;
    if (!reply.ParseFromString(response_buffer) || reply.payload_size() == 0) {
      return nscapi::protobuf::functions::set_response_bad(*response, ".NET plugin " + entry.alias + " returned no response for " + command);
    }
    const std::string original_command = response->command();
    response->CopyFrom(reply.payload(0));
    if (response->command().empty()) response->set_command(original_command);
    return;
  }
  nscapi::protobuf::functions::set_response_bad(*response, "Failed to find command: " + command);
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

std::int32_t DotnetPlugins::core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len, dotnet::write_fn write,
                                          void *wctx) {
  DotnetPlugins *self = static_cast<DotnetPlugins *>(ctx);
  if (self == nullptr) return 0;
  try {
    const std::string request = (data != nullptr && len > 0) ? std::string(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len)) : std::string();
    std::string response;
    const std::int32_t rc = self->dispatch(op, str, request, response);
    if (write != nullptr && !response.empty()) write(wctx, reinterpret_cast<const std::uint8_t *>(response.data()), static_cast<std::int32_t>(response.size()));
    return rc;
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("core call from .NET plugin", e);
  } catch (...) {
    NSC_LOG_ERROR_EX("core call from .NET plugin");
  }
  return 0;
}
