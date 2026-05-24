/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NRPEServer.h"

#include <config.h>

#include <net/socket/socket_settings_helper.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>

namespace sh = nscapi::settings_helper;

NRPEServer::NRPEServer() {}
NRPEServer::~NRPEServer() {}

bool NRPEServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
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
  settings.set_alias("NRPE", alias, "server");

  bool insecure;
  settings.alias().add_key_to_settings().add_bool("insecure", sh::bool_key(&insecure, false), "ALLOW INSECURE CHIPHERS and ENCRYPTION",
                                                  "Only enable this if you are using legacy check_nrpe client.");

  settings.register_all();
  settings.notify();

  settings.alias().add_path_to_settings()("NRPE Server", "Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.");

  settings.alias()
      .add_key_to_settings()
      .add_string("port", sh::string_key(&info_.port_, "5666"), "PORT NUMBER", "Port to use for NRPE.")

      .add_int("payload length", sh::uint_key(&payload_length_, 1024), "PAYLOAD LENGTH",
               "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use "
               "the same value for it to work.",
               true)

      .add_bool("allow arguments", sh::bool_key(&allowArgs_, false), "COMMAND ARGUMENT PROCESSING",
                "This option determines whether or not the we will allow clients to specify arguments to commands that are executed.")

      .add_bool("allow nasty characters", sh::bool_key(&allowNasty_, false), "COMMAND ALLOW NASTY META CHARS",
                "This option determines whether or not the we will allow clients to specify nasty (as in |`&><'\"\\[]{}) characters in arguments.")

      .add_bool("performance data", sh::bool_fun_key([this](auto value) { this->set_perf_data(value); }, true), "PERFORMANCE DATA",
                "Send performance data back to nagios (set this to 0 to remove all performance data).", true)

      ;

  socket_helpers::settings_helper::add_core_server_opts(settings, info_);
  // The "insecure" preset relaxes SECLEVEL and removes the cert path so it
  // can interop with very old check_nrpe builds that have no certificate.
  // It deliberately keeps !ADH: anonymous Diffie-Hellman gives encryption
  // with no peer authentication, which means any reachable client speaks the
  // protocol successfully and the channel can be MITMed undetected. Legacy
  // check_nrpe works fine with non-anonymous suites, so there is no
  // compatibility reason to permit ADH.
  std::string certificate = insecure ? "" : "${certificate-path}/certificate.pem";
  std::string opts = insecure ? "ALL:!ADH:!MD5:@STRENGTH:@SECLEVEL=0" : "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
  socket_helpers::settings_helper::add_ssl_server_opts(settings, info_, true, "${certificate-path}/nrpe_dh_2048.pem", certificate, "", opts);

  settings.alias().add_key_to_settings().add_bool(
      "extended response", sh::bool_key(&multiple_packets_, !insecure), "EXTENDED RESPONSE",
      "Send more then 1 return packet to allow response to go beyond payload size (requires modified client if legacy is true this defaults to false).");

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()

      .add_string("encoding", sh::string_key(&encoding_, ""), "NRPE PAYLOAD ENCODING", "", true);

  settings.alias().add_key_to_settings().add_string("client identity source", sh::string_key(&client_identity_source_, "none"), "CLIENT IDENTITY SOURCE",
                                                    "How to resolve the principal stamped on the request for the core permission system. "
                                                    "'none' (default) leaves the principal empty (subject becomes bare 'NRPEServer'). "
                                                    "'cn' uses the Common Name value of the verified client certificate (e.g. 'icinga-master'); "
                                                    "this requires SSL with verify_mode containing 'peer' and 'fail-if-no-peer-cert' plus a 'ca path' "
                                                    "pointing at the trusted issuer, otherwise the module refuses to start. "
                                                    "CN-only (not full DN) because INI key syntax conflicts with the '=' in RFC 2253 DNs.");

  settings.register_all();
  settings.notify();

#ifndef USE_SSL
  if (info_.use_ssl) {
    NSC_LOG_ERROR_STD(_T("SSL not available! (not compiled with openssl support)"));
    return false;
  }
#endif
  if (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart) {
    // Reject pathologically small payload lengths. The handler computes
    // `max_len = payload_length - 1` and feeds it into substr-based
    // multi-packet split logic; a configured value of 0 would underflow to
    // SIZE_MAX and the split loop would never trim, eventually exhausting
    // memory. 16 is well below any sane real value (default 1024).
    if (payload_length_ < 16) {
      NSC_LOG_ERROR_STD("NRPE payload length " + str::xtos(payload_length_) +
                        " is too small (minimum 16). Refusing to start; raise the value in /settings/NRPE/server.");
      return false;
    }
    if (payload_length_ != 1024)
      NSC_DEBUG_MSG_STD("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = " +
                        str::xtos(payload_length_));
    if (insecure) {
      // Surface the loss of cert-based peer auth as a real error so it lands
      // in monitoring dashboards. Operators routinely flip this on for
      // legacy check_nrpe compatibility without realising they have lost
      // server identity verification.
      NSC_LOG_ERROR_STD(
          "NRPEServer is starting in insecure mode: SECLEVEL=0 and no server certificate. "
          "Clients have no way to authenticate this server, traffic can be MITMed. "
          "Use insecure=true only for legacy check_nrpe interop on a trusted network.");
    }
    // Validate the client identity source knob before accepting any
    // traffic. The CN-based mode is only meaningful if the TLS layer is
    // actually verifying the client cert against a trusted CA - without
    // that, the CN is whatever the attacker chose to put on a self-signed
    // cert and would be a security regression rather than a feature.
    if (client_identity_source_ != "none" && client_identity_source_ != "cn") {
      NSC_LOG_ERROR_STD("NRPEServer: unknown 'client identity source' value '" + client_identity_source_ + "'; expected 'none' or 'cn'. Refusing to start.");
      return false;
    }
    if (client_identity_source_ == "cn") {
#ifdef USE_SSL
      // Check the PARSED verify_mode bitmask, not the raw config string.
      // The string side has aliases (e.g. `peer-cert` expands to
      // `verify_peer | verify_fail_if_no_peer_cert` in the parser, see
      // socket_helpers.cpp::get_verify_mode), so substring-matching on
      // the raw string would reject perfectly valid configs and accept
      // confusingly-spelled ones. Going through the parsed value is the
      // only spelling-independent check.
      const bool ssl_on = info_.ssl.enabled;
      const auto parsed_vm = info_.ssl.get_verify_mode();
      const bool wants_peer = (parsed_vm & boost::asio::ssl::context_base::verify_peer) != 0;
      const bool wants_fail = (parsed_vm & boost::asio::ssl::context_base::verify_fail_if_no_peer_cert) != 0;
      const bool has_ca = !info_.ssl.ca_path.empty();
      if (!ssl_on || !wants_peer || !wants_fail || !has_ca) {
        NSC_LOG_ERROR_STD(
            "NRPEServer: 'client identity source = cn' requires SSL with verify_mode containing 'peer' and "
            "'fail-if-no-peer-cert' (or the 'peer-cert' alias) plus a non-empty 'ca path'. Current: ssl=" +
            std::string(ssl_on ? "on" : "off") + ", verify_mode='" + info_.ssl.verify_mode + "' (parsed bits: peer=" + std::string(wants_peer ? "yes" : "no") +
            ", fail-if-no-peer-cert=" + std::string(wants_fail ? "yes" : "no") + "), ca_path='" + info_.ssl.ca_path +
            "'. Refusing to start with attacker-supplied identity.");
        return false;
      }
#else
      NSC_LOG_ERROR_STD("NRPEServer: 'client identity source = cn' requires SSL support, but this build was compiled without it. Refusing to start.");
      return false;
#endif
    }
    NSC_LOG_ERROR_LISTS(info_.validate());

    std::list<std::string> errors;
    info_.allowed_hosts.refresh(errors);
    NSC_LOG_ERROR_LISTS(errors);
    NSC_DEBUG_MSG_STD("Allowed hosts definition: " + info_.allowed_hosts.to_string());
    NSC_DEBUG_MSG_STD("Server config: " + info_.to_string());

    boost::asio::io_context io_service_;

    // Failures here (typically a bad `bind to` address that the resolver
    // can't look up, or a port already in use) used to escape loadModuleEx
    // and cause the whole module to fail to load. That's overkill: the
    // module's other surfaces (settings, registered commands) are still
    // useful for diagnostics and reconfiguration even when the listener
    // can't come up. Log the failure clearly, drop the half-constructed
    // server, and let the module load so the operator can fix the config
    // without restarting the service into a broken state.
    try {
      server_.reset(new nrpe::server::server(info_, this));
      if (!server_) {
        NSC_LOG_ERROR_STD("Failed to create server instance!");
        return true;
      }
      server_->start();
    } catch (const std::exception &e) {
      NSC_LOG_ERROR_STD("NRPEServer listener failed to start (module remains loaded; fix the configuration and reload): " + utf8::utf8_from_native(e.what()));
      server_.reset();
    } catch (...) {
      NSC_LOG_ERROR_STD("NRPEServer listener failed to start with unknown error (module remains loaded; fix the configuration and reload)");
      server_.reset();
    }
  }
  return true;
}

void NRPEServer::prepareShutdown() {
  // Stop accepting new connections and join the I/O threads while every peer
  // plugin is still loaded, so any in-flight request that calls into another
  // module can complete cleanly before unloadModule tears state down.
  try {
    if (server_) {
      server_->stop();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool NRPEServer::unloadModule() {
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

std::list<nrpe::packet> NRPEServer::handle(nrpe::packet p, const std::string &peer_identity) {
  std::list<nrpe::packet> packets;
  str::utils::token cmd = str::utils::getToken(p.getPayload(), '!');
  // Trace incoming requests so the log shows what the upstream server actually
  // asked for (previously only the connection IP appeared in the log; the
  // command name + argument blob were invisible). Gated because building the
  // string traverses the payload and does string concatenation.
  NSC_TRACE_ENABLED() {
    NSC_TRACE_MSG("NRPE request: command='" + cmd.first + "' args='" + cmd.second + "' (payload_length=" + str::xtos(p.get_payload_length()) +
                  ", peer_identity='" + peer_identity + "')");
  }
  if (cmd.first == "_NRPE_CHECK") {
    packets.push_back(nrpe::packet::create_response(
        p.getVersion(), NSCAPI::query_return_codes::returnOK,
        "I (" + utf8::cvt<std::string>(nscapi::plugin_singleton->get_core()->getApplicationVersionString()) + ") seem to be doing fine...",
        p.get_payload_length()));
    // Literal payload — NSC_TRACE_MSG already gates internally so no outer
    // NSC_TRACE_ENABLED block is required here.
    NSC_TRACE_MSG("NRPE response: _NRPE_CHECK ping reply");
    return packets;
  }
  if (!allowArgs_) {
    if (!cmd.second.empty()) {
      NSC_LOG_ERROR("Request contained arguments (not currently allowed, check the allow arguments option).");
      throw nrpe::nrpe_exception("Request contained arguments (not currently allowed, check the allow arguments option).");
    }
  }
  if (!allowNasty_) {
    if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
      NSC_LOG_ERROR("Request command contained illegal metachars!");
      throw nrpe::nrpe_exception("Request command contained illegal metachars!");
    }
    if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
      NSC_LOG_ERROR("Request arguments contained illegal metachars!");
      throw nrpe::nrpe_exception("Request command contained illegal metachars!");
    }
  }
  std::string wmsg, wperf;
  NSCAPI::nagiosReturn ret = -3;
  nscapi::core_helper ch(get_core(), get_id());
  try {
    const std::size_t max_len = p.get_payload_length() - 1;
    std::string wcmd, wargs;
    if (encoding_.empty()) {
      wcmd = utf8::cvt<std::string>(utf8::to_unicode(cmd.first));
      wargs = utf8::cvt<std::string>(utf8::to_unicode(cmd.second));
    } else {
      wcmd = utf8::cvt<std::string>(utf8::from_encoding(cmd.first, encoding_));
      wargs = utf8::cvt<std::string>(utf8::from_encoding(cmd.second, encoding_));
    }
    // When the transport has resolved a peer identity (e.g. the client
    // cert CN under `client identity source = cn`), stamp it as the
    // principal so the policy sees `NRPEServer:<cn>` rather than a
    // bare `NRPEServer` subject. The transport may extract the
    // identity unconditionally for any SSL handshake; honouring it is
    // gated on the `client identity source` knob so flipping verify_mode
    // for an unrelated reason doesn't silently change the policy subject.
    const bool use_principal = client_identity_source_ == "cn" && !peer_identity.empty();
    if (!use_principal)
      ret = ch.simple_query_from_nrpe(wcmd, wargs, wmsg, wperf, multiple_packets_ ? -1 : max_len);
    else
      ret = ch.simple_query_from_nrpe_as(peer_identity, wcmd, wargs, wmsg, wperf, multiple_packets_ ? -1 : max_len);
    switch (ret) {
      case NSCAPI::query_return_codes::returnOK:
      case NSCAPI::query_return_codes::returnWARN:
      case NSCAPI::query_return_codes::returnCRIT:
      case NSCAPI::query_return_codes::returnUNKNOWN:
        break;
      default:
        throw nrpe::nrpe_exception("UNKNOWN: Internal error.");
    }
    std::string data, msg, perf;
    if (encoding_.empty()) {
      msg = utf8::to_system(utf8::cvt<std::wstring>(wmsg));
      perf = utf8::to_system(utf8::cvt<std::wstring>(wperf));
    } else {
      msg = utf8::to_encoding(utf8::cvt<std::wstring>(wmsg), encoding_);
      perf = utf8::to_encoding(utf8::cvt<std::wstring>(wperf), encoding_);
    }
    if (perf.empty() || noPerfData_) {
      data = msg;
    } else {
      data = msg + "|" + perf;
    }
    if (multiple_packets_ && p.getVersion() == nrpe::data::version2) {
      std::size_t data_len = data.size();
      for (std::size_t i = 0; i < data_len; i += max_len) {
        if (data_len - i <= max_len)
          packets.push_back(nrpe::packet::create_response(p.getVersion(), ret, data.substr(i, max_len), p.get_payload_length()));
        else
          packets.push_back(nrpe::packet::create_more_response(ret, data.substr(i, max_len), p.get_payload_length()));
      }
    } else {
      if (p.getVersion() == nrpe::data::version2 && data.length() >= max_len) {
        data = data.substr(0, max_len);
      }
      packets.push_back(nrpe::packet::create_response(p.getVersion(), ret, data, p.get_payload_length()));
    }
    // Trace what we are about to put on the wire. Note: this records the
    // response NSClient++ produced — actually getting it to the client is the
    // connection layer's job; if the upstream has already disconnected, the
    // socket write will fail and that is logged separately by the connection.
    NSC_TRACE_ENABLED() {
      NSC_TRACE_MSG("NRPE response: command='" + cmd.first + "' rc=" + str::xtos(ret) + " message_bytes=" + str::xtos(wmsg.size()) +
                    " perf_bytes=" + str::xtos(wperf.size()) + " packets=" + str::xtos(packets.size()));
    }
  } catch (...) {
    packets.push_back(
        nrpe::packet::create_response(p.getVersion(), NSCAPI::query_return_codes::returnUNKNOWN, "UNKNOWN: Internal exception", p.get_payload_length()));
    NSC_LOG_ERROR("NRPE response: command='" + cmd.first + "' produced internal exception, returning UNKNOWN");
    return packets;
  }

  return packets;
}