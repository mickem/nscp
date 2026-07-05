// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <net/check_mk/server/server_protocol.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <scripts/script_nscp.hpp>

#include "handler_impl.hpp"

class CheckMKServer : public nscapi::impl::simple_plugin {
 public:
  CheckMKServer();
  virtual ~CheckMKServer();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void prepareShutdown();
  bool unloadModule();
  // Receives the periodic metrics tick and stashes the values in
  // check_mk::shared_metrics_store() so Lua scripts can read them.
  void submitMetrics(const PB::Metrics::MetricsMessage &metrics);
  // Passive check submission. Two channels are registered (configurable):
  // `check_mk-mrpe`  - emitted as cached <<<mrpe>>> entries on next fetch.
  // `check_mk-local` - emitted as cached <<<local>>> entries on next fetch.
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);

 private:
  bool add_script(std::string alias, std::string file);

  socket_helpers::connection_info info_;
  std::shared_ptr<check_mk::server::server> server_;
  std::shared_ptr<handler_impl> handler_;
  std::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
  std::shared_ptr<lua::lua_runtime> lua_runtime_;
  std::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
  boost::filesystem::path root_;

  // Channel names (configurable).
  std::string channel_mrpe_;
  std::string channel_local_;
  // How long submitted results are advertised as fresh in cached(...) headers.
  int submission_ttl_;
};
