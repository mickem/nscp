/*
 * Copyright (C) 2004-2026 Michael Medin
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

#pragma once

#include <client/command_line_parser.hpp>
#include <net/socket/socket_helpers.hpp>
#include <openssl/ssl.h>

#include <sstream>
#include <string>

namespace nsca_ng_client {

// Default PSK cipher list used when no explicit ciphers are configured.
// Only TLS 1.2 PSK ciphersuites are listed; PSK is not directly available
// via the legacy SSL_CTX_set_psk_client_callback API under TLS 1.3.
extern const char kDefaultPskCiphers[];

// Default upper bound on Nagios plugin output forwarded over the wire. Made
// configurable per target via the `max output length` setting (B5 fix).
constexpr std::size_t kDefaultMaxOutputBytes = 65536;

// Max number of attempts (initial try + retries) for the entire submit
// transaction when the network/TLS phase fails. Honours the per-target
// `retries` setting (B2 fix).
constexpr int kDefaultMaxAttempts = 3;

// Connection-level configuration extracted from a target's CLI/settings
// destination_container.
struct connection_data : public socket_helpers::connection_info {
  std::string password;
  std::string identity;
  std::string sender_hostname;
  std::string tls_ciphers;
  std::size_t max_output_length = kDefaultMaxOutputBytes;
  bool use_psk = true;
  // When true, results submitted on this target without an explicit
  // `service` are interpreted as Nagios host checks (PROCESS_HOST_CHECK_RESULT).
  // Default false so service checks are the unsurprising fallback.
  bool host_check_default = false;

  connection_data(client::destination_container arguments, client::destination_container sender);

  std::string to_string() const;
};

// Client handler — implements client::handler_interface for NSCANgClient.
struct nsca_ng_client_handler final : public client::handler_interface {
  bool query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage &,
             PB::Commands::QueryResponseMessage &) override;
  bool exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage &,
            PB::Commands::ExecuteResponseMessage &) override;
  bool metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage &) override;
  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override;
};

// --- visible for unit tests ---

// Generate a base64-encoded session ID (6 random bytes, 8 base64 chars).
// Throws std::runtime_error if OpenSSL's RNG fails (B3 fix).
std::string generate_session_id();

// PSK client callback. Reads identity/key from data attached to the SSL via
// SSL_set_ex_data (L1 fix: was thread_local). max_*_len of 0 is treated as
// "no buffer" and the call returns 0 cleanly rather than UB-ing on the
// underflowed `len - 1` arithmetic (L2 fix).
unsigned int psk_client_cb(SSL *ssl, const char *hint, char *identity, unsigned int max_identity_len, unsigned char *psk, unsigned int max_psk_len);

// Index used with SSL_get_ex_data / SSL_set_ex_data to hand per-connection
// PSK credentials to the client callback. Allocated lazily, once.
int get_psk_ex_data_index();

// Per-connection PSK credentials. The lifetime is the calling submit() — the
// pointer is stored on the SSL via SSL_set_ex_data and read back from the
// callback. Replaces the previous thread_local storage.
struct psk_credentials {
  std::string identity;
  std::string psk;
};

}  // namespace nsca_ng_client
