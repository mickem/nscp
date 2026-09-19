// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/protobuf/command.hpp>

#include "commands.hpp"
#include "script_interface.hpp"

class extscr_cli {
  std::shared_ptr<script_provider_interface> provider_;

 public:
  extscr_cli(const std::shared_ptr<script_provider_interface> &provider_);

  bool run(std::string cmd, const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void add_script(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void configure(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void list(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void show(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);
  void delete_script(const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response);

 private:
  bool validate_sandbox(boost::filesystem::path pscript, PB::Commands::ExecuteResponseMessage::Response *response);
  // `add --import` copies a file into the script root, where `show` then
  // returns its bytes: without this the sandbox that confines show and delete
  // is a formality, since any file the service can read can be brought inside
  // it first. The source has to live in the script root, in ${shared-path} or
  // in the upload staging area under ${temp} - the three places a script is
  // legitimately imported from.
  bool validate_import_source(const boost::filesystem::path &source, PB::Commands::ExecuteResponseMessage::Response *response);
};
