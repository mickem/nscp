// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <check/path_access_policy.hpp>
#include <cstdint>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <set>

#include "bookmarks.hpp"

struct real_time_thread;
class CheckLogFile : public nscapi::impl::simple_plugin {
 private:
  std::shared_ptr<real_time_thread> thread_;
  check_logfile::bookmarks bookmarks_;
  // Bookmark keys which were read from the core storage on load. A key which
  // is no longer live when we shut down is blanked out there, so a position
  // that has aged out (or whose filter was edited) does not keep its row in
  // nsclient.db forever.
  std::set<std::string> persisted_keys_;
  // Which files a caller may name in `file=`/`files=` (check_logfile) and
  // `file=` (check_csv): both return contents. Open by default, so an
  // upgrade changes nothing; see docs/docs/concepts/check-access.md.
  check::access::path_policy file_access_;
  // `max csv size`: the largest file check_csv reads, whatever max-size= a
  // caller passes (0 = no ceiling). Atomic: a reload rewrites it while checks
  // run.
  std::atomic<std::uint64_t> csv_max_bytes_{64ull * 1024 * 1024};

 public:
  CheckLogFile() : file_access_("file", "files", "/settings/logfile") {}
  virtual ~CheckLogFile() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void check_logfile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_csv(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);

 private:
  void set_csv_max_size(const std::string &value);
};
