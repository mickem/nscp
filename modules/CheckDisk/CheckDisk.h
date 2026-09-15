// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <check/path_access_policy.hpp>
#include <memory>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>

#include "collector_thread.hpp"

class CheckDisk : public nscapi::impl::simple_plugin {
  bool show_errors_;
  // A reload replaces the collector while checks are running: the core calls
  // loadModuleEx(reloadStart) on the live module without waiting for the
  // threads that are inside handleCommand. Publishing the pointer atomically
  // and taking a copy in every check keeps the old instance alive until the
  // last check that observed it returns, instead of freeing its mutexes and
  // trend snapshot under a check that is reading them.
  std::shared_ptr<collector_thread> collector_;
  // Which paths a caller may name in check_files, check_single_file and
  // check_disk_write. Open by default, so an upgrade changes nothing; see
  // docs/docs/concepts/check-access.md.
  check::access::path_policy file_access_;

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

  // The collector as every caller must read it: a shared_ptr, never a raw
  // pointer into one. Returning `get_collector_ptr().get()` would hand back a
  // pointer whose owning temporary dies at the end of the caller's expression,
  // so a reload replacing the member could free the collector under a caller
  // that still held it - the very race the atomic publication above exists to
  // close. Never dereference the member directly either: a reload can replace
  // it between the test and the call.
  std::shared_ptr<collector_thread> get_collector_ptr() const { return std::atomic_load(&collector_); }
};
