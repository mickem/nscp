// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NSCAServer.h"

#include <net/socket/socket_settings_helper.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_common_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/xtos.hpp>

namespace CryptoPP {
const std::string DEFAULT_CHANNEL = "";
}  // namespace CryptoPP

namespace sh = nscapi::settings_helper;

bool NSCAServer::loadModuleEx(const std::string &alias, const NSCAPI::moduleLoadMode mode) {
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_STD("Failed to stop server");
    return false;
  }

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("NSCA", alias, "server");

  settings.alias().add_path_to_settings()("NSCA SERVER SECTION", "Section for NSCA (NSCAServer) (check_nsca) protocol options.");

  settings.alias()
      .add_key_to_settings()
      .add_string("port", sh::string_key(&info_.port_, "5667"), "PORT NUMBER", "Port to use for NSCA.")

      .add_int("payload length", sh::uint_key(&payload_length_, 512), "PAYLOAD LENGTH",
               "Length of payload to/from the NSCA agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCA agent to use "
               "the same value for it to work.")

      .add_bool("performance data", sh::bool_fun_key([this](auto value) { this->set_perf_data(value); }, true), "PERFORMANCE DATA",
                "Send performance data back to nagios (set this to false to remove all performance data).")

      .add_string("encryption", sh::string_fun_key([this](auto value) { this->set_encryption(value); }, "aes256"), "ENCRYPTION",
                  std::string("Name of encryption algorithm to use.\nHas to be the same as your agent i using or it wont work at all."
                              "This is also independent of SSL and generally used instead of SSL.\nAvailable encryption algorithms are:\n") +
                      nscp::encryption::helpers::get_crypto_string("\n"))

      .add_string("timezone", sh::string_key(&timezone_, "utc"), "TIMEZONE",
                  "Reference timezone for the wire timestamp emitted in the IV packet and used by the replay-window check. The protocol "
                  "specification calls for UTC (default). Set to 'local' (or any POSIX TZ string) only when interoperating with a legacy "
                  "agent that wrote local-clock-as-Unix-time into the wire field. Both ends must agree on the value.");

  socket_helpers::settings_helper::add_core_server_opts(settings, info_);
  socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, false, "", "${certificate-path}/certificate.pem", "",
                                                       "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()

      .add_password("password", sh::string_key(&password_, ""), DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC)

      .add_string("inbox", sh::string_key(&channel_, "inbox"), "INBOX", "The default channel to post incoming messages on");

  settings.register_all();
  settings.notify();

#ifndef USE_SSL
  if (info_.ssl.enabled) {
    NSC_LOG_ERROR_STD("SSL not available! (not compiled with openssl support)");
    return false;
  }
#endif
  if (payload_length_ != 512)
    NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nsca changing #define MAX_PACKETBUFFER_LENGTH = " +
                      str::xtos(payload_length_));
  NSC_LOG_ERROR_LISTS(info_.validate());

  std::list<std::string> errors;
  info_.allowed_hosts.refresh(errors);
  NSC_LOG_ERROR_LISTS(errors);
  NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());
  NSC_DEBUG_MSG_STD("Starting server on: " + info_.to_string());

  if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
    server_.reset(new nsca::server::server(info_, this));
    if (!server_) {
      NSC_LOG_ERROR_STD("Failed to create server instance!");
      return false;
    }
    server_->start();
  }
  return true;
}

void NSCAServer::prepareShutdown() {
  // Stop accepting new connections and join the I/O threads while every peer
  // plugin is still loaded, so any in-flight submission can complete cleanly
  // before unloadModule tears state down.
  try {
    if (server_) {
      server_->stop();
    }
  } catch (...) {
    NSC_LOG_ERROR_STD("Exception caught while preparing shutdown");
  }
}

bool NSCAServer::unloadModule() {
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_STD("Exception caught: <UNKNOWN>");
    return false;
  }
  return true;
}

void NSCAServer::handle(nsca::packet p) {
  // Trace inbound NSCA submissions so an operator can see what hosts/services
  // a remote NSCA client is actually pushing through this server (the
  // connection layer only logs the IP). Gated because the result body can be
  // large and the string copy would otherwise be paid on every packet.
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG("NSCA submission: host='" + p.host + "' service='" + p.service + "' code=" + str::xtos(p.code) +
                  " result_bytes=" + str::xtos(p.result.size()));
  }
  std::string response;
  std::string::size_type pos = p.result.find('|');
  nscapi::core_helper helper(get_core(), get_id());
  if (pos != std::string::npos) {
    const std::string msg = p.result.substr(0, pos);
    const std::string perf = p.result.substr(++pos);
    helper.submit_simple_message(channel_, p.host, "", p.service, nscapi::plugin_helper::int2nagios(p.code), msg, perf, response);
  } else {
    const std::string empty, msg = p.result;
    helper.submit_simple_message(channel_, p.host, "", p.service, nscapi::plugin_helper::int2nagios(p.code), msg, empty, response);
  }
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG("NSCA submission: host='" + p.host + "' service='" + p.service + "' channel='" + channel_ +
                  "' submit_response_bytes=" + str::xtos(response.size()));
  }
}