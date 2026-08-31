// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "collector_thread.hpp"

class CheckDisk : public nscapi::impl::simple_plugin {
  bool show_errors_;
  std::shared_ptr<collector_thread> collector_;

 public:
  CheckDisk();
  virtual ~CheckDisk() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  std::wstring get_filter(unsigned int drvType);

  // Check commands
  NSCAPI::nagiosReturn check_filesize(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg,
                                      std::string &perf);
  NSCAPI::nagiosReturn check_files(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg,
                                   std::string &perf);
  void check_files(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_single_file(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_drivesize(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_disk_io(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_disk_write(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_disk_health(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_uncpath(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_storagepool(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_shadowcopy(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_share(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_mount(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // Metrics
  void fetchMetrics(PB::Metrics::MetricsMessage::Response *response);

  // Legacy checks
  void checkDriveSize(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void checkFiles(PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

  // Read-only view of the collector the module owns; null until loadModuleEx()
  // has created it. Exists so the unit test can assert on the intervals
  // loadModuleEx parsed out of the settings - the clamping and the fallbacks
  // it applies have no other observable effect.
  const collector_thread *get_collector() const { return collector_.get(); }
};
