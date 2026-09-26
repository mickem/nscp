// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NSCAServer.h"

#include <net/socket/socket_settings_helper.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_common_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscp/password_hash.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>

namespace CryptoPP {
const std::string DEFAULT_CHANNEL = "";
}  // namespace CryptoPP

namespace sh = nscapi::settings_helper;

std::string NSCAServer::read_client_target_password() const {
  namespace pf = nscapi::protobuf::functions;
  const std::string target_path = "/settings/NSCA/client/targets/default";

  pf::settings_query q(get_id());
  q.get(target_path, "password", "");
  get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    // Not fatal: no key here just means the checks below report an empty one.
    return "";
  }
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (val.matches(target_path, "password")) {
      const std::string password = val.get_string();
      if (!password.empty()) {
        NSC_DEBUG_MSG_STD("NSCA server has no password of its own, using the key from " + target_path);
      }
      return password;
    }
  }
  return "";
}

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

  // The encryption key, and *not* under /settings/default. That section is the
  // shared password inbound protocols check a caller against - the web UI,
  // check_nt, NRPE - and it is stored hashed. NSCA has no password check: the
  // string is the key the payload is encrypted with, and every submitting
  // client has to know it. Sharing one value between "what I verify callers
  // with" and "the key I share with a remote server" is the antipattern, so
  // this key lives in NSCA's own sections.
  settings.alias()
      .add_key_to_settings()

      .add_password("password", sh::string_key(&password_, ""), DEFAULT_PASSWORD_NAME,
                    "The NSCA encryption key: the same value every submitting client uses. Falls back to the default target of NSCAClient "
                    "(/settings/NSCA/client/targets/default/password) when unset, so an agent that both submits and receives NSCA needs one key, "
                    "not two. Never inherited from /settings/default - that is the password inbound protocols check against, and it is hashed.")

      ;

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()

      .add_string("inbox", sh::string_key(&channel_, "inbox"), "INBOX", "The default channel to post incoming messages on");

  settings.register_all();
  settings.notify();

  if (password_.empty()) {
    password_ = read_client_target_password();
  }

  try {
    encryption_ = nscp::encryption::helpers::encryption_to_int(encryption_name_);
  } catch (const nscp::encryption::encryption_exception &e) {
    NSC_LOG_ERROR_STD("Refusing to start NSCA server: " + utf8::utf8_from_native(e.what()));
    return false;
  }
  if (encryption_ != nscp::encryption::helpers::no_encryption && password_hash::is_hashed(password_)) {
    // The key is derived from the password string itself, so a hashed value is
    // not a usable key: no client knows it, and starting anyway would silently
    // reject every submission. Since this key is no longer inherited from
    // /settings/default it takes a deliberate paste to get here, but a stored
    // hash is never a key, so refuse rather than run deaf.
    NSC_LOG_ERROR_STD("Refusing to start NSCA server: the password is stored hashed (pbkdf2-sha256$...), but NSCA encryption (" + encryption_name_ +
                      ") derives its key from the clear-text password. Set the clear-text key under /settings/NSCA/server (password=...), or set "
                      "encryption = none.");
    return false;
  }
  if (encryption_ != nscp::encryption::helpers::no_encryption && password_.empty()) {
    NSC_LOG_ERROR_STD("NSCA encryption is enabled (" + encryption_name_ +
                      ") but the password is empty. The NSCA key is derived directly from the password, so an empty password is a well-known key: anyone who "
                      "can reach the port can decrypt and forge submissions. Set a password under /settings/NSCA/server (password=...) on both ends, or run "
                      "`nscp nsca install --host <server> --password <key>` to configure both directions at once.");
  }

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
  // The wire fields are attacker-influenced (anyone allowed to reach the
  // port past the shared secret): strip control characters from the
  // identity fields before they hit logs or the inbox channel, and clamp
  // the 16-bit wire return code to the Nagios range (anything else becomes
  // UNKNOWN instead of flowing downstream as an arbitrary integer).
  const std::string host = nsca::sanitize_identity(p.host);
  const std::string service = nsca::sanitize_identity(p.service);
  const unsigned int code = p.code > static_cast<unsigned int>(NSCAPI::query_return_codes::returnUNKNOWN) ? NSCAPI::query_return_codes::returnUNKNOWN : p.code;
  // Trace inbound NSCA submissions so an operator can see what hosts/services
  // a remote NSCA client is actually pushing through this server (the
  // connection layer only logs the IP). Gated because the result body can be
  // large and the string copy would otherwise be paid on every packet.
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG("NSCA submission: host='" + host + "' service='" + service + "' code=" + str::xtos(code) +
                  (code != p.code ? " (clamped from " + str::xtos(p.code) + ")" : "") + " result_bytes=" + str::xtos(p.result.size()));
  }
  std::string response;
  const std::string::size_type pos = p.result.find('|');
  nscapi::core_helper helper(get_core(), get_id());
  std::string msg = p.result, perf;
  if (pos != std::string::npos) {
    msg = p.result.substr(0, pos);
    // `performance data = false` promises perfdata is stripped before the
    // submission is forwarded.
    if (!noPerfData_) perf = p.result.substr(pos + 1);
  }
  helper.submit_simple_message(channel_, host, "", service, nscapi::plugin_helper::int2nagios(code), msg, perf, response);
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG("NSCA submission: host='" + host + "' service='" + service + "' channel='" + channel_ +
                  "' submit_response_bytes=" + str::xtos(response.size()));
  }
}
