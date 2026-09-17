// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/snapshot.hpp>

class CheckNet : public nscapi::impl::simple_plugin {
  // Path to the trusted CA bundle, resolved once from ${ca-path} during
  // loadModuleEx and reused by every check_http invocation. Avoids hitting
  // the path expander on the hot path and gives us a single, consistent
  // value across the module's lifetime.
  // Re-resolved by loadModuleEx on every reload while checks are running on
  // the server pools: published as a snapshot so a check reads one whole path.
  nscapi::settings::snapshot<std::string> default_ca_;

 public:
  CheckNet() {};

  bool loadModuleEx(const std::string &alias, NSCAPI::moduleLoadMode mode);

  // Check commands
  static void check_ping(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_tcp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_ssh(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_dns(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void check_http(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_nsclient_web_online(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_apache_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_nginx_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_phpfpm_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  void check_tomcat_status(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const;
  static void check_connections(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  static void check_ntp_offset(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
