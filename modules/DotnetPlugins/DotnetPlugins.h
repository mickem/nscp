// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <dotnet/bridge.hpp>
#include <dotnet/host.hpp>
#include <dotnet/runtime.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <string>
#include <vector>


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
    bool messages = false;  // exposes an IMessageHandler: gets the log entries
  };

  DotnetPlugins() = default;
  virtual ~DotnetPlugins() = default;

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const PB::Commands::QueryRequestMessage &request_message);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
  bool commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                       PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &request_message);
  void handleLogMessage(const PB::Log::LogEntry::Entry &message);

  // A configured plugin value that means "do not load this plugin".
  static bool is_disabled(const std::string &value);

  // Resolve a configured plugin value ("enabled", a file name or a path) to
  // the assembly to load: the value as configured wins when that file exists,
  // then the same name with ".dll" appended (assembly names carry dots, so a
  // missing extension cannot be told from a dotted name). `exists` is a
  // parameter so the rule can be unit-tested without a file system.
  static boost::filesystem::path resolve_assembly(const boost::filesystem::path &root, const std::string &alias, const std::string &value,
                                                  const std::function<bool(const boost::filesystem::path &)> &exists);

  // The core callback handed to managed code (see include/dotnet/bridge.hpp).
  static std::int32_t NSCP_DOTNET_CALL core_callback(void *ctx, std::int32_t op, const char *str, const std::uint8_t *data, std::int32_t len,
                                                     dotnet::write_fn write, void *wctx);

 private:
  void add_plugin(const std::string &key, const std::string &value);
  bool start_runtime();
  bool resolve_bridge();
  bool load_plugin(plugin_entry &entry, NSCAPI::moduleLoadMode mode);
  boost::filesystem::path resolve_plugin_root() const;
  std::int32_t dispatch(std::int32_t op, const char *str, const std::string &request, std::string &response);

  std::string settings_path_;
  std::string runtime_root_;
  std::string plugin_path_;
  std::string default_factory_;
  boost::filesystem::path root_;
  std::map<std::string, std::string> configured_;
  // Guards plugins_: queries, submissions and log entries arrive on other
  // threads than the one loading and unloading the module.
  std::mutex plugins_mutex_;
  std::vector<plugin_entry> plugins_;
  dotnet::bridge_functions bridge_;
  std::shared_ptr<dotnet::host> host_;
};
