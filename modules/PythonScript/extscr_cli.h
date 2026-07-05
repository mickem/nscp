// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <memory>
#include <nscapi/protobuf/command.hpp>

#include "script_interface.hpp"

class extscr_cli {
 private:
  std::shared_ptr<script_provider_interface> provider_;
  std::string alias_;

 public:
  extscr_cli(std::shared_ptr<script_provider_interface> provider_, std::string alias_);

  bool run(std::string cmd, const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void add_script(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void configure(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void list(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void show(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void delete_script(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);

 private:
  bool validate_sandbox(boost::filesystem::path pscript, PB::Commands::ExecuteResponseMessage::Response *response) const;
};
