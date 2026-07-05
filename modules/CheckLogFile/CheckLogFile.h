// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

struct real_time_thread;
class CheckLogFile : public nscapi::impl::simple_plugin {
 private:
  std::shared_ptr<real_time_thread> thread_;

 public:
  CheckLogFile() {}
  virtual ~CheckLogFile() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();
  void check_logfile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
