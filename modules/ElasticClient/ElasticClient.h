// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/log.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nscapi/settings/snapshot.hpp>

#include <atomic>
#include <string>
#include <vector>

class ElasticClient : public nscapi::impl::simple_plugin {
 private:
  // Read by the event/metrics/log callbacks and written when the module is
  // unloaded; those run on different threads, so it is not a plain bool.
  std::atomic<bool> started;

 public:
  // Everything the three send paths read.
  //
  // They run on three different threads - the event thread, the metrics thread
  // and the logger's worker - and loadModuleEx re-runs on every reload while
  // all three are live. `started` stays true across a reload, so it fences
  // nobody out, and handleLogMessage is deliberately not dispatch-gated by the
  // core: an NSC_LOG_ERROR raised inside loadModuleEx itself comes straight
  // back into this module while the same function is mid-assignment. Reading
  // the settings into a local and publishing it in one store is what makes
  // that safe rather than merely unlikely: a send either uses the whole
  // previous configuration or the whole new one, never half-old credentials
  // against a half-new address.
  struct config {
    std::string hostname;

    std::string address;
    std::string user;
    std::string password;
    std::string api_key;
    std::string tls_version;
    std::string verify_mode;
    std::string ca;
    // Seconds; read as a signed value so a negative setting is rejected at load
    // rather than wrapping into an effectively infinite unsigned timeout.
    int timeout = 30;

    std::string event_index;
    std::string event_type;

    std::string metrics_index;
    std::string metrics_type;

    std::string nsclient_index;
    std::string nsclient_type;
  };

 private:
  nscapi::settings::snapshot<config> config_;

 public:
  ElasticClient();
  virtual ~ElasticClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void submitMetrics(const PB::Metrics::MetricsMessage &response);
  void onEvent(const PB::Commands::EventMessage &request, const std::string &buffer);

  void handleLogMessage(const PB::Log::LogEntry::Entry &message);

 private:
  // Takes the caller's snapshot rather than re-reading the member: one
  // configuration per submission, start to finish.
  void send_to_elastic(const config &cfg, const std::string &index, const std::string &type, const std::vector<std::string> &payloads, bool log_errors) const;
};
