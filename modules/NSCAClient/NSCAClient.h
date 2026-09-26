// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/command.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

class NSCAClient : public nscapi::impl::simple_plugin {
  std::string channel_;
  std::string hostname_;
  std::string encoding_;

  client::configuration client_;

 public:
  NSCAClient();
  virtual ~NSCAClient();
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  void query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message);
  bool commandLineExec(int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response);
  void handleNotification(const std::string &channel, const PB::Commands::SubmitRequestMessage &request_message,
                          PB::Commands::SubmitResponseMessage *response_message);

 private:
  void add_command(const std::string &key, const std::string &args);
  void add_target(const std::string &key, const std::string &args);
  // What `nsca install` was asked for. An empty field means the option was not
  // given, and the value on disk stands - a re-run to change one thing does not
  // reset the rest.
  struct install_args {
    std::string host;
    std::string port;
    std::string password;
    std::string encryption;
    std::string hostname;
  };

  // `nscp nsca install`: configure NSCA in one command instead of three
  // `nscp settings --set` calls. Parses the options and hands off to one of the
  // two sides below; which one is the whole point of --server.
  bool cli_install(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) const;
  // The default: submission. Writes the default target (address, port,
  // encryption and the shared key with the daemon) and enables NSCAClient.
  bool install_client(const install_args &args, PB::Commands::ExecuteResponseMessage::Response *response) const;
  // `--server`: the listening side. Writes /settings/NSCA/server (port,
  // encryption and the key the submitting hosts use) and enables NSCAServer.
  // A separate key on purpose: it is shared with different peers than the
  // client's, and NSCAServer reads neither the client's nor /settings/default.
  bool install_server(const install_args &args, PB::Commands::ExecuteResponseMessage::Response *response) const;
  // Resolves a cipher name to its id. False when the agent does not know the
  // name, having filled in `response` with the refusal and the names it does
  // know; `from_command_line` says whether the name was given this run, which
  // only changes the wording.
  bool resolve_cipher(const std::string &encryption, bool from_command_line, int &resolved,
                      PB::Commands::ExecuteResponseMessage::Response *response) const;
};
