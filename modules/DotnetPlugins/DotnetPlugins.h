// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <cstdint>
#include <map>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

#include "dotnet_bridge.hpp"
#include "dotnet_host.hpp"

/**
 * Hosts plugins written for .NET (C#, F#, ...) inside NSClient++.
 *
 * The runtime is located and started through hostfxr at load time (see
 * dotnet_host.hpp); the managed side of the boundary is NSCP.Core.dll
 * (libs/dotnet-plugin-api), which loads each configured plugin assembly,
 * instantiates its factory and routes queries to the plugin that registered
 * the command. Plugins live in the `plugin path` folder (default
 * ${module-path}/dotnet) next to NSCP.Core.dll.
 */
class DotnetPlugins : public nscapi::impl::simple_plugin {
 public:
  struct plugin_entry {
    std::string alias;
    std::string assembly;  // resolved path of the plugin assembly
    std::string factory;   // fully qualified factory type name
    void *handle = nullptr;
    std::string name;
    std::string version;
  };

  DotnetPlugins() = default;
  virtual ~DotnetPlugins() = default;

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);

  // Exposed for tests: resolve a configured plugin value ("enabled", a file
  // name or a path) to the assembly path to load.
  static boost::filesystem::path resolve_assembly(const boost::filesystem::path &root, const std::string &alias, const std::string &value);

  // The core callback handed to managed code (see dotnet_bridge.hpp).
  static std::int32_t NSCP_DOTNET_CALL core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len,
                                                     dotnet::write_fn write, void *wctx);

 private:
  struct bridge_functions {
    dotnet::managed_load_fn load = nullptr;
    dotnet::managed_start_fn start = nullptr;
    dotnet::managed_unload_fn unload = nullptr;
    dotnet::managed_describe_fn describe = nullptr;
    dotnet::managed_query_fn query = nullptr;
  };

  void add_plugin(const std::string &key, const std::string &value);
  bool start_runtime();
  bool load_plugin(plugin_entry &entry, NSCAPI::moduleLoadMode mode);
  boost::filesystem::path resolve_plugin_root() const;
  std::int32_t dispatch(std::int32_t op, const char *str, const std::string &request, std::string &response);

  std::string settings_path_;
  std::string runtime_root_;
  std::string plugin_path_;
  std::string default_factory_;
  boost::filesystem::path root_;
  std::map<std::string, std::string> configured_;
  std::vector<plugin_entry> plugins_;
  bridge_functions bridge_;
  std::shared_ptr<dotnet::host> host_;
};
