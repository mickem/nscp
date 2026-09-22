// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/tuple/tuple.hpp>
#include <client/command_line_parser.hpp>
#include <net/http/http_client_protocol.hpp>
#include <net/socket/client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <str/format.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

#include <string>
#include <utility>
#include <vector>

namespace nscp_client {

// NSClient++ talking to another NSClient++ over its REST API.
//
// This used to POST a serialized QueryRequestMessage to /query.pb. That route
// is gone: it handed the remote core a message whose header the caller wrote,
// which is how a caller picked the identity the permission layer attributed
// the call to. The versioned API takes the command and its arguments and
// stamps the identity from the authenticated session instead.
//
// The shape below is the one NRPEClient's forward path already uses - a
// request per payload, the response rebuilt with
// append_simple_query_response_payload - because that is what a transport
// carrying "a command, its arguments, and a Nagios result" supports.

// The Nagios result the v2 query endpoint encodes in the HTTP status when it
// is asked for text/plain (query_controller::execute_query_text).
inline NSCAPI::nagiosReturn status_to_nagios(const unsigned int status_code) {
  switch (status_code) {
    case 200:
      return NSCAPI::query_return_codes::returnOK;
    case 202:
      return NSCAPI::query_return_codes::returnWARN;
    case 500:
      return NSCAPI::query_return_codes::returnCRIT;
    default:
      // 503 is the endpoint's own "unknown"; anything else (401/403/404, a
      // proxy error, a response we do not recognise) is equally unknown to a
      // check, and the caller gets the status text alongside it.
      return NSCAPI::query_return_codes::returnUNKNOWN;
  }
}

// "<base>/<command>/commands/execute?a=1&b=2". Arguments arrive in the
// `key=value` form the command line and the other transports use; a bare
// argument (no '=') is passed as a valueless parameter, which is how the
// endpoint's own parser reads it back into the request payload.
inline std::string build_query_path(const std::string &base, const std::string &command, const std::vector<std::string> &arguments) {
  std::string path = base;
  if (!path.empty() && path[path.size() - 1] == '/') path.erase(path.size() - 1);
  path += "/" + http::uri_encode(command) + "/commands/execute";
  bool first = true;
  for (const std::string &argument : arguments) {
    path += first ? "?" : "&";
    first = false;
    const std::string::size_type pos = argument.find('=');
    if (pos == std::string::npos) {
      path += http::uri_encode(argument);
    } else {
      path += http::uri_encode(argument.substr(0, pos)) + "=" + http::uri_encode(argument.substr(pos + 1));
    }
  }
  return path;
}

// Split a text/plain query body into (message, perfdata).
//
// execute_query_text writes one "message[|perf]\n" per response line, so a
// check with long output answers with several of them. Splitting the whole
// body on its first '|' would keep only the first line as the message and
// hand everything after it - newlines, later lines and their own '|' - to the
// performance-data parser, which then reports garbage counters and loses the
// output. Split per line instead: the messages rejoin with newlines, the way
// a Nagios plugin writes long output, and the perfdata concatenates with a
// space, the way multiple perf strings combine.
inline std::pair<std::string, std::string> parse_query_body(const std::string &raw_body) {
  // The endpoint terminates every line, so the body normally ends in a
  // newline. Drop the trailing ones here as well as at the call site, so this
  // never yields a spurious empty last message.
  std::string body = raw_body;
  while (!body.empty() && (body[body.size() - 1] == '\n' || body[body.size() - 1] == '\r')) body.erase(body.size() - 1);

  std::string message, perf;
  std::string::size_type pos = 0;
  while (pos <= body.size()) {
    std::string::size_type eol = body.find('\n', pos);
    std::string line = body.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);

    const str::utils::token parts = str::utils::getToken(line, '|');
    if (!message.empty()) message += "\n";
    message += parts.first;
    if (!parts.second.empty()) {
      if (!perf.empty()) perf += " ";
      perf += parts.second;
    }

    if (eol == std::string::npos) break;
    pos = eol + 1;
  }
  return std::make_pair(message, perf);
}

struct connection_data : public socket_helpers::connection_info {
  std::string password;
  std::string path;
  std::shared_ptr<socket_helpers::client::client_handler> handler;

  connection_data(client::destination_container source, client::destination_container target, std::shared_ptr<socket_helpers::client::client_handler> handler)
      : handler(handler) {
    address = target.address.host;
    port_ = target.address.get_port_string("8443");

    ssl.certificate = "";  // target.get_string_data("certificate", "${certificate-path}/certificate.pem");
    ssl.certificate_key = target.get_string_data("certificate key");
    ssl.certificate_key_format = target.get_string_data("certificate format", "PEM");
    ssl.ca_path = target.get_string_data("ca", "${ca-path}");
    ssl.allowed_ciphers = target.get_string_data("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    ssl.dh_key = target.get_string_data("dh");
    ssl.verify_mode = target.get_string_data("verify mode", "peer");
    // An empty verify mode parses to verify_none, so a key left blank in the
    // ini would silently disable verification rather than fall back to the
    // default. Treat blank as "not set", as NRDPClient does; `none` is the
    // spelling that opts out.
    if (ssl.verify_mode.empty()) ssl.verify_mode = "peer";
    if (ssl.ca_path.empty()) ssl.ca_path = "${ca-path}";
    if (!ssl.certificate.empty()) ssl.certificate = handler->expand_path(ssl.certificate);
    if (!ssl.certificate_key.empty()) ssl.certificate_key = handler->expand_path(ssl.certificate_key);
    // The settings layer expands a configured `ca` (it is registered as a path
    // key), but the defaults above and a `ca=` passed on the command line or
    // over REST reach us verbatim - and "${ca-path}" is not a filename.
    ssl.ca_path = handler->expand_path(ssl.ca_path);

    timeout = target.timeout;
    retry = target.retry;
    password = target.get_string_data("password", "");
    // The base path of the remote agent's query collection, not a single
    // endpoint: build_query_path() appends the command and its arguments.
    path = target.get_string_data("path", "/api/v2/queries");

    // The endpoint is the remote agent's REST API, which listens on TLS by
    // default (port 8443 above is its https port), so TLS is on unless the
    // target turns it off - and the peer is verified against ${ca-path},
    // because an unverified relay hands the target's password to whichever
    // host answers for the address. An agent that still presents the
    // self-signed certificate it generates on first start is reached with
    // `verify mode = peer-cert` and `ca` pointing at that certificate;
    // `verify mode = none` keeps the old behaviour and is never the default.
    ssl.enabled = true;
    if (target.has_data("no ssl")) ssl.enabled = !target.get_bool_data("no ssl");
    if (target.has_data("ssl")) ssl.enabled = target.get_bool_data("ssl");
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << get_endpoint_string();
    ss << ", path: " << path;
    // Never log the actual password: this is emitted at trace level on every
    // operation and historically leaked the shared secret into operator
    // debug logs.
    ss << ", password: " << (password.empty() ? "<unset>" : "<set>");
    ss << ", ssl: " << ssl.to_string();
    return ss.str();
  }
};

struct client_handler : public socket_helpers::client::client_handler {
  void log_debug(std::string file, int line, std::string msg) const {
    if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
      GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
    }
  }
  void log_error(std::string file, int line, std::string msg) const {
    if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
      GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
    }
  }
  std::string expand_path(std::string path) { return GET_CORE()->expand_path(path); }
};

template <class THandler = client_handler>
struct nscp_client_handler : public client::handler_interface {
  std::shared_ptr<THandler> handler_;
  nscp_client_handler() : handler_(std::make_shared<THandler>()) {}

  std::string get_command(std::string alias, std::string command = "") {
    if (!alias.empty()) return alias;
    if (!command.empty()) return command;
    return "";
  }

  bool query(client::destination_container sender, client::destination_container target, const PB::Commands::QueryRequestMessage &request_message,
             PB::Commands::QueryResponseMessage &response_message) {
    const PB::Common::Header &request_header = request_message.header();
    nscp_client::connection_data con(sender, target, handler_);

    handler_->log_debug(__FILE__, __LINE__, "Connecting to: " + con.to_string());

    for (const std::string &e : con.validate()) {
      handler_->log_error(__FILE__, __LINE__, e);
    }

    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    // One request per payload. The remote runs each command on its own and
    // answers with a Nagios result, so the request message is never put on
    // the wire - which is also why nothing here can carry the local caller's
    // identity to the other host.
    if (request_message.payload_size() == 0) {
      // Nothing to ask for. The endpoint addresses the command by path, so
      // there is no request to make without one - answer rather than send a
      // URL with an empty command in it.
      nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), "", NSCAPI::query_return_codes::returnUNKNOWN,
                                                                        "No command to run: the request carried no payload.");
      return true;
    }
    for (int i = 0; i < request_message.payload_size(); i++) {
      const PB::Commands::QueryRequestMessage::Request &p = request_message.payload(i);
      const std::string command = get_command(p.alias(), p.command());
      const std::vector<std::string> arguments(p.arguments().begin(), p.arguments().end());
      const boost::tuple<NSCAPI::nagiosReturn, std::string, std::string> ret = send_query(con, command, arguments);
      nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>(), ret.get<2>());
    }
    return true;
  }

  // Submitting a result to a remote agent has no endpoint in the REST API,
  // and never worked over this transport: the old code posted an NRPE-style
  // "command!arg!arg" string to the protobuf route, which the remote parsed
  // as an empty message and answered with nothing - and this function then
  // reported success regardless, because it compared a bool against
  // returnUNKNOWN. Say so instead of pretending: a passive result that is
  // silently dropped is worse than one that is refused.
  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) {
    const PB::Common::Header &request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    const std::string error =
        "Submitting results over NSCP is not supported: the agent's REST API has no endpoint for it. Use NSCA, NRDP or another submit client.";
    handler_->log_error(__FILE__, __LINE__, error);
    for (int i = 0; i < request_message.payload_size(); i++) {
      const std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
      nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), command, false, error);
    }
    return true;
  }

  // As for submit(): there is no remote-execution endpoint in the REST API,
  // and the old code posted the same malformed request the submit path did.
  bool exec(client::destination_container sender, client::destination_container target, const PB::Commands::ExecuteRequestMessage &request_message,
            PB::Commands::ExecuteResponseMessage &response_message) {
    const PB::Common::Header &request_header = request_message.header();
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

    const std::string error = "Executing commands on a remote agent over NSCP is not supported: the agent's REST API has no endpoint for it.";
    handler_->log_error(__FILE__, __LINE__, error);
    for (int i = 0; i < request_message.payload_size(); i++) {
      const std::string command = get_command(request_message.payload(i).command());
      nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), command, NSCAPI::query_return_codes::returnUNKNOWN,
                                                                      error);
    }
    return true;
  }

  bool metrics(client::destination_container sender, client::destination_container target, const PB::Metrics::MetricsMessage &request_message) { return false; }

  //////////////////////////////////////////////////////////////////////////
  // Protocol implementations
  //

  // Run one command on the remote agent and return (result, message, perf).
  //
  // text/plain rather than the default JSON: the endpoint then answers exactly
  // as a Nagios plugin does - the result in the HTTP status, "message|perf" in
  // the body - which is what a check result is. The JSON shape carries the
  // same data in a form we would only have to flatten again.
  boost::tuple<NSCAPI::nagiosReturn, std::string, std::string> send_query(nscp_client::connection_data con, const std::string &command,
                                                                          const std::vector<std::string> &arguments) {
    try {
#ifndef USE_SSL
      if (con.ssl.enabled)
        return boost::make_tuple(NSCAPI::query_return_codes::returnUNKNOWN, std::string("SSL support not available (compiled without USE_SSL)"), std::string());
#endif
      http::request packet("GET", con.get_endpoint_string(), nscp_client::build_query_path(con.path, command, arguments));
      packet.add_header("Accept", "text/plain");
      // The same header Icinga's check_nscp_api uses: the remote maps it to
      // its `admin` user, which needs the `queries.execute` grant. Before
      // this, the password was read from the configuration and never sent.
      if (!con.password.empty()) packet.add_header("password", con.password);

      socket_helpers::client::client<http::client::protocol> client(con, handler_);
      const http::response response = client.process_request(packet);

      std::string body = response.get_payload();
      // The endpoint writes a newline after every line, including the last;
      // drop it so the trailing one does not become an empty final message.
      while (!body.empty() && (body[body.size() - 1] == '\n' || body[body.size() - 1] == '\r')) body.erase(body.size() - 1);

      const NSCAPI::nagiosReturn code = nscp_client::status_to_nagios(response.status_code_);
      if (body.empty()) {
        // No body to explain the status: say what the remote answered rather
        // than returning an empty check result.
        return boost::make_tuple(code, "Remote agent answered " + str::xtos(response.status_code_) + " " + response.status_message_, std::string());
      }
      const std::pair<std::string, std::string> rdata = nscp_client::parse_query_body(body);
      return boost::make_tuple(code, rdata.first, rdata.second);
    } catch (std::runtime_error &e) {
      return boost::make_tuple(NSCAPI::query_return_codes::returnUNKNOWN, "Socket error: " + utf8::utf8_from_native(e.what()), std::string());
    } catch (std::exception &e) {
      return boost::make_tuple(NSCAPI::query_return_codes::returnUNKNOWN, "Error: " + utf8::utf8_from_native(e.what()), std::string());
    } catch (...) {
      return boost::make_tuple(NSCAPI::query_return_codes::returnUNKNOWN, std::string("Unknown error -- REPORT THIS!"), std::string());
    }
  }
};
}  // namespace nscp_client