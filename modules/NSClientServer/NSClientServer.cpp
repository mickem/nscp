// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NSClientServer.h"

#include "check_nt_commands.hpp"

#include <time.h>

#include <boost/assign.hpp>
#include <net/socket/socket_settings_helper.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_common_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/constant_time.hpp>
#include <str/utils.hpp>

namespace sh = nscapi::settings_helper;
namespace ntc = check_nt_commands;

NSClientServer::NSClientServer() : noPerfData_(false), allowNasty_(false), allowArgs_(false) {}
NSClientServer::~NSClientServer() {}

bool NSClientServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("NSClient", alias, "server");

  std::string allow_spec;

  settings.alias().add_path_to_settings()("NSCLIENT SERVER SECTION", "Section for NSClient (NSClientServer.dll) (check_nt) protocol options.");

  settings.alias()
      .add_key_to_settings()

      .add_bool("performance data", sh::bool_fun_key([this](auto value) { this->set_perf_data(value); }, true), "PERFORMANCE DATA",
                "Send performance data back to Nagios (set this to 0 to remove all performance data).")

      .add_string("allow", sh::string_key(&allow_spec, "any"), "ALLOWED COMMANDS",
                  "Comma separated list of which check_nt commands this server will answer. Each entry is a group, the keyword 'any'/'all', or an "
                  "individual command name. Groups: 'metrics' (cpuload, uptime, useddiskspace, memuse), 'info' (clientversion), 'service' (servicestate), "
                  "'process' (procstate), 'counters' (counter, instances), 'files' (fileage). Individual commands: clientversion, cpuload, uptime, "
                  "useddiskspace, servicestate, procstate, memuse, counter, fileage, instances. Default 'any' answers everything (full check_nt "
                  "compatibility). To expose only harmless system metrics use e.g. 'metrics, info'; this denies the arbitrary-read commands (counter, "
                  "fileage, instances) and the service/process enumeration commands.");

  socket_helpers::settings_helper::add_port_server_opts(settings, info_, "12489");
  // Default SSL on: the legacy check_nt protocol carries the password in every
  // request, so an operator who needs to interoperate with very old clients
  // that don't speak TLS has to consent explicitly by setting `ssl = false`.
  // Note that the SSL path here is best-effort — many third-party check_nt
  // clients never implemented it — but the toggle still serves as a clear
  // "I know I'm running this without transport security" gate.
  socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true, "", "${certificate-path}/certificate.pem", "",
                                                       "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
  socket_helpers::settings_helper::add_core_server_opts(settings, info_);

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()

      .add_password("password", sh::string_fun_key([this](auto value) { this->set_password(value); }, ""), DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC);

  settings.register_all();
  settings.notify();

  {
    std::set<std::string> unknown;
    allowed_commands_ = ntc::parse_allowed_commands(allow_spec, unknown);
    for (const std::string &u : unknown) {
      NSC_LOG_ERROR_STD("Ignoring unknown entry '" + u + "' in the check_nt 'allow' setting (expected a group, 'any', or a command name).");
    }
    if (allowed_commands_.empty()) {
      NSC_LOG_ERROR_STD("The check_nt 'allow' setting did not enable any commands; the server will reject every request. Set 'allow = any' to restore the "
                        "default (answer all commands).");
    }
    NSC_DEBUG_MSG_STD("check_nt 'allow' = " + allow_spec + " (" + str::xtos(allowed_commands_.size()) + " of 10 commands enabled).");
  }

#ifndef USE_SSL
  if (info_.use_ssl) {
    NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
  }
#endif
  // The legacy check_nt protocol predates modern transport security and the
  // server side has no working TLS path; we can't refuse to start, only
  // warn so that the operator knows what they're exposing.
  if (!info_.ssl.enabled) {
    NSC_LOG_ERROR_STD(
        "NSClient legacy server (check_nt) is running without TLS. The protocol transmits all traffic in clear, including any configured "
        "password. Consider switching to REST or NRPE for modern transport security.");
  }
  if (!get_password().empty()) {
    NSC_LOG_ERROR_STD(
        "NSClient legacy server (check_nt) has a password configured. The check_nt protocol carries the password in every request and offers "
        "no replay protection; an attacker on the wire can capture and reuse it. Consider switching to REST or NRPE.");
  }
  NSC_LOG_ERROR_LISTS(info_.validate());

  std::list<std::string> errors;
  info_.allowed_hosts.refresh(errors);
  for (const std::string &e : errors) {
    NSC_LOG_ERROR_STD(e);
  }
  NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());

  boost::asio::io_context io_service_;

  if (mode == NSCAPI::normalStart) {
    try {
#ifndef USE_SSL
      if (info_.use_ssl) {
        NSC_LOG_ERROR_STD(_T("SSL is not supported (not compiled with openssl)"));
        return false;
      }
#endif
      server_.reset(new check_nt::server::server(info_, this));
      if (!server_) {
        NSC_LOG_ERROR_STD("Failed to create server instance!");
        return false;
      }
      server_->start();
    } catch (std::exception &e) {
      NSC_LOG_ERROR_EXR("start", e);
      return false;
    } catch (...) {
      NSC_LOG_ERROR_EX("start");
      return false;
    }
  }
  return true;
}
void NSClientServer::prepareShutdown() {
  // Stop accepting new connections and join the I/O threads while every peer
  // plugin is still loaded, so any in-flight check_nt query can complete
  // cleanly before unloadModule tears state down.
  try {
    if (server_) {
      server_->stop();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool NSClientServer::unloadModule() {
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}

bool NSClientServer::isPasswordOk(std::string remotePassword) {
  const std::string localPassword = get_password();
  // No password configured: refuse all requests. Previously the server allowed
  // any client that sent the literal word "None" through, which turned a
  // forgotten password into an open listener.
  if (localPassword.empty()) {
    NSC_LOG_ERROR_STD("Using check_nt without a password is a security risk, please configure passwords (or better yet switch protocols).");
    return false;
  }
  return str::constant_time_eq(localPassword, remotePassword);
}

void log_bad_command(const std::string &cmd) {
  if (cmd == "check_cpu" || cmd == "check_uptime" || cmd == "check_memory") {
    NSC_LOG_ERROR(cmd + std::string(" failed to execute have you loaded CheckSystem? (CheckSystem=enabled under modules)"));
  } else {
    NSC_LOG_ERROR("Unknown command: " + cmd);
  }
}

std::string NSClientServer::list_instance(std::string counter) {
  std::list<std::string> exeresult;
  nscapi::core_helper ch(get_core(), get_id());
  ch.exec_simple_command("CheckSystem", "pdh", boost::assign::list_of(std::string("--list"))("--porcelain")("--counter")(counter)("--no-counters"), exeresult);
  std::string result;

  typedef boost::tokenizer<boost::escaped_list_separator<char>, std::string::const_iterator, std::string> Tokenizer;
  for (const std::string &s : exeresult) {
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line, '\n')) {
      // Each porcelain enumeration line has three comma-separated fields:
      // the literal "instance", the object name and the instance name. We
      // want the third (the instance name). Advance to it, but stop if the
      // line has fewer fields (e.g. an "ERROR: ..." line from a failed PDH
      // enumeration - list_instance does not gate on the command's return
      // code) so we never dereference the end iterator.
      Tokenizer tok(line);
      Tokenizer::const_iterator cit = tok.begin();
      int advanced = 0;
      for (; advanced < 2 && cit != tok.end(); ++advanced) ++cit;
      if (advanced == 2 && cit != tok.end()) {
        if (!result.empty()) result += ",";
        result += *cit;
      } else {
        NSC_LOG_ERROR("Invalid line: " + line);
      }
    }
  }
  return result;
}

check_nt::packet NSClientServer::handle(check_nt::packet p) {
  std::string buffer = p.get_payload();

  std::string::size_type pos = buffer.find_first_of("\n\r");
  if (pos != std::string::npos) {
    std::string::size_type pos2 = buffer.find_first_not_of("\n\r", pos);
    if (pos2 != std::string::npos) {
      std::string rest = buffer.substr(pos2);
    }
    buffer = buffer.substr(0, pos);
  }

  // Distinct error strings (Invalid password vs No command specified) used to
  // give attackers a clean true/false oracle for online password guessing.
  // Collapse them into one generic error.
  static const char *kBadRequest = "ERROR: Bad request.";
  str::utils::token pwd = str::utils::getToken(buffer, '&');
  if (!isPasswordOk(pwd.first)) {
    return check_nt::packet(kBadRequest);
  }
  if (pwd.second.empty()) return check_nt::packet(kBadRequest);
  str::utils::token cmd = str::utils::getToken(pwd.second, '&');
  if (cmd.first.empty()) return check_nt::packet(kBadRequest);

  int c = 0;
  try {
    c = boost::lexical_cast<int>(cmd.first.c_str());
  } catch (const boost::bad_lexical_cast &) {
    return check_nt::packet("ERROR: Non-numeric command code: " + cmd.first);
  }

  // Honour the `allow` setting: a request for a command the operator has not
  // permitted is rejected before it is dispatched.
  if (!is_command_allowed(c)) {
    NSC_DEBUG_MSG_STD("Rejected check_nt command code " + str::xtos(c) + ": not permitted by the 'allow' setting.");
    return check_nt::packet("ERROR: Command not allowed.");
  }

  // The two codes that are answered inline rather than by dispatching a
  // modern query.
  if (c == ntc::REQ_CLIENTVERSION) {
    return check_nt::packet(get_core()->getApplicationName() + " " + get_core()->getApplicationVersionString());
  }
  if (c == ntc::REQ_INSTANCES) {
    return check_nt::packet(list_instance(cmd.second));
  }

  ntc::mapped_command real_command;
  if (!ntc::map_request(c, cmd.second, real_command)) {
    return check_nt::packet("ERROR: Unknown command.");
  }

  std::string response;
  nscapi::core_helper ch(get_core(), get_id());
  NSC_DEBUG_MSG("Real command: " + real_command.command + " " + str::utils::joinEx(real_command.arguments, " "));
  if (!ch.simple_query(real_command.command, real_command.arguments, response)) {
    log_bad_command(real_command.command);
    return check_nt::packet("ERROR: Could not complete the request check log file for more information.");
  }

  ::PB::Commands::QueryResponseMessage message;
  if (!message.ParseFromString(response)) return check_nt::packet("ERROR: Failed to parse data from: " + real_command.command);
  if (message.payload_size() != 1)
    return check_nt::packet("ERROR: Command returned invalid number of payloads: " + real_command.command + ", " + str::xtos(message.payload_size()));
  return check_nt::packet(ntc::format_response(c, real_command.command, message.payload(0)));
}