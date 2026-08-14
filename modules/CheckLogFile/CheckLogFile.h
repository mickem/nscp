// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

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

 public:
  CheckLogFile() {}
  virtual ~CheckLogFile() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void check_logfile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
