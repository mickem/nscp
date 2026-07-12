// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

class CheckSecurity : public nscapi::impl::simple_plugin {
 public:
  CheckSecurity();
  virtual ~CheckSecurity() {}

  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Check commands
  void check_certificate(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_firewall(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_users(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_nla(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_antivirus(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_bitlocker(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_secureboot(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_defender(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_local_accounts(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_group_members(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
