// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <metrics/metrics_store_map.hpp>
#include <nscapi/log_handler.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace client {
struct cli_handler : nscapi::log_handler {
  virtual void output_message(const std::string &msg) = 0;
  virtual int get_plugin_id() const = 0;
  virtual const nscapi::core_wrapper *get_core() const = 0;
};
class cli_client {
  typedef std::shared_ptr<cli_handler> cli_handler_ptr;
  cli_handler_ptr handler;
  metrics::metrics_store metrics_store;

 public:
  cli_client(const cli_handler_ptr &handler) : handler(handler) {}
  void handle_command(const std::string &command);
  void push_metrics(const PB::Metrics::MetricsMessage &response);
};
typedef std::shared_ptr<cli_handler> cli_handler_ptr;
}  // namespace client
