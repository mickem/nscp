// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <net/socket/client.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include <memory>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

namespace collectd_client {
struct collectd_client_handler;
}

class CollectdClient : public nscapi::impl::simple_plugin {
 private:
  std::string hostname_;

  std::shared_ptr<collectd_client::collectd_client_handler> handler_;
  client::configuration client_;

 public:
  CollectdClient();
  virtual ~CollectdClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void submitMetrics(const PB::Metrics::MetricsMessage &response);

 private:
  void add_target(std::string key, std::string args);
};
