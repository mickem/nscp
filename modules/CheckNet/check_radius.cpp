// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "check_radius.hpp"

#include <boost/asio.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <net/address_family.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/utf8.hpp>
#include <threads/guarded_io_context.hpp>

#include "check_radius_protocol.hpp"

namespace check_net {
namespace {
struct radius_result {
  std::string server, result = "timeout", reply = "none";
  long long port = 1812, elapsed = 0;
  std::string show() const { return server + ": " + result; }
};
struct radius_handler : parsers::where::filter_handler_impl<std::shared_ptr<radius_result>> {
  radius_handler() {
    registry_.add_string_var("server", [](auto o) { return o->server; }, "RADIUS server queried");
    registry_.add_string_var("result", [](auto o) { return o->result; }, "ok, unexpected_response, timeout, or a protocol/transport error");
    registry_.add_string_var("reply", [](auto o) { return o->reply; }, "Authenticated reply type; none until the reply passes both authenticators");
    registry_.add_int_var("port", parsers::where::type_int, [](auto o) { return o->port; }, "Remote UDP port");
    registry_.add_int_var(
                 "time", parsers::where::type_int, [](auto o) { return o->elapsed; }, "Elapsed query time including resolution, in milliseconds")
        .add_int_perf("ms", "", "_time");
  }
};
using radius_filter = modern_filter::modern_filters<radius_result, radius_handler>;

#ifdef USE_SSL
std::string read_secret(const std::string &path, const char *kind, std::size_t max_size) {
#ifdef _WIN32
  boost::filesystem::ifstream input(boost::filesystem::path(utf8::cvt<std::wstring>(path)), std::ios::binary);
#else
  boost::filesystem::ifstream input(boost::filesystem::path(path), std::ios::binary);
#endif
  if (!input) throw std::runtime_error(std::string("Unable to read ") + kind + " file");
  std::string value;
  char c;
  while (input.get(c)) {
    value += c;
    if (value.size() > max_size + 2) throw std::runtime_error(std::string(kind) + " file is too large");
  }
  if (!input.eof()) throw std::runtime_error(std::string("Unable to read ") + kind + " file");
  if (!value.empty() && value.back() == '\n') {
    value.pop_back();
    if (!value.empty() && value.back() == '\r') value.pop_back();
  }
  if (value.empty() || value.size() > max_size || value.find_first_of("\r\n") != std::string::npos)
    throw std::runtime_error(std::string(kind) + " file must contain one nonempty line within the length limit");
  return value;
}

void query_radius(radius_result &out, const radius::packet &request, const std::string &secret, const std::string &mode, net::address_family family,
                  int timeout) {
  using boost::asio::ip::udp;
  boost::asio::io_context io;
  udp::resolver resolver(io);
  udp::socket socket(io);
  boost::asio::steady_timer timer(io);
  std::array<unsigned char, 65536> buffer{};
  const auto start = std::chrono::steady_clock::now();
  bool done = false;
  const auto finish = [&](const std::string &result) {
    if (done) return;
    done = true;
    out.result = result;
    resolver.cancel();
    boost::system::error_code ignored;
    socket.close(ignored);
    try {
      timer.cancel();
    } catch (...) {
    }
  };
  timer.expires_after(std::chrono::milliseconds(timeout));
  timer.async_wait([&](const boost::system::error_code &ec) {
    if (!ec) finish(out.result);
  });
  std::function<void()> receive;
  receive = [&]() {
    socket.async_receive(boost::asio::buffer(buffer), [&](const boost::system::error_code &ec, std::size_t bytes) {
      if (done) return;
      if (ec) return finish("receive_failed");
      const radius::packet response(buffer.begin(), buffer.begin() + bytes);
      const std::string error = radius::validate(response, request, secret);
      if (!error.empty()) {
        out.result = error;
        receive();  // Discard invalid replies, but retain the original deadline.
        return;
      }
      out.reply = radius::reply_name(response[0]);
      finish(radius::expected(mode, response[0]) ? "ok" : "unexpected_response");
    });
  };
  const auto resolved = [&](const boost::system::error_code &ec, udp::resolver::results_type endpoints) {
    if (done) return;
    if (ec || endpoints.empty()) return finish("resolve_failed");
    boost::system::error_code error;
    // A connected UDP socket accepts traffic only from the selected endpoint.
    socket.connect(endpoints.begin()->endpoint(), error);
    if (error) return finish("connect_failed");
    socket.async_send(boost::asio::buffer(request), [&](const boost::system::error_code &send_error, std::size_t) {
      if (done) return;
      if (send_error) return finish("send_failed");
      receive();
    });
  };
  if (family == net::address_family::any)
    resolver.async_resolve(out.server, std::to_string(out.port), resolved);
  else
    resolver.async_resolve(family == net::address_family::ipv4 ? udp::v4() : udp::v6(), out.server, std::to_string(out.port), resolved);
  threads::run_io_context_guarded("RADIUS query", io, [&](const std::string &message) {
    NSC_LOG_ERROR_STD(message);
    finish("internal_error");
  });
  out.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}
#endif
}  // namespace

void check_radius(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  namespace po = boost::program_options;
  modern_filter::data_container data;
  modern_filter::cli_helper<radius_filter> helper(request, response, data);
  radius_filter filter;
  std::string host, secret_file, password_file, username, mode, family_arg, nas;
  int port = 1812, timeout = 3000;
  helper.add_options("", "result != 'ok'", "", filter.get_filter_syntax(), "unknown");
  helper.add_syntax("${status}: ${list}", "${server}:${port} ${result}, reply=${reply}, time=${time}ms", "${server}_${port}", "No RADIUS server checked", "");
  // clang-format off
  helper.get_desc().add_options()
    ("host", po::value<std::string>(&host), "RADIUS host; the first address in the requested family is tested.")
    ("port", po::value<int>(&port)->default_value(1812), "UDP port (1812 for authentication, usually 1813 for accounting Status-Server).")
    ("timeout", po::value<int>(&timeout)->default_value(3000), "Overall deadline in milliseconds (1..60000); one request, no retries.")
    ("mode", po::value<std::string>(&mode)->default_value("auth"), "auth: PAP and expect Access-Accept; reject: expect Access-Reject for a fictional user; status: RFC 5997 responsiveness only.")
    ("username", po::value<std::string>(&username), "Test identity for auth; fictional identity for reject (default nsclient-radius-probe).")
    ("secret-file", po::value<std::string>(&secret_file), "Protected file containing the shared secret (1..4096 bytes); one trailing newline is removed.")
    ("password-file", po::value<std::string>(&password_file), "Protected file containing the PAP test password (1..128 bytes). Required only for auth.")
    ("nas-identifier", po::value<std::string>(&nas)->default_value("nsclient-monitor"), "NAS-Identifier sent to select the intended policy (1..253 bytes).")
    ("address-family", po::value<std::string>(&family_arg), net::address_family_option_help());
  // clang-format on
  if (!helper.parse_options()) return;
  if (!helper.build_filter(filter)) return;
  auto bad = [&](const std::string &message) { nscapi::protobuf::functions::set_response_bad(*response, message); };
  if (host.empty() || secret_file.empty()) return bad("host and secret-file are required");
  if (port < 1 || port > 65535 || timeout < 1 || timeout > 60000) return bad("port must be 1..65535 and timeout 1..60000 milliseconds");
  if (mode != "auth" && mode != "reject" && mode != "status") return bad("mode must be auth, reject or status");
  if (nas.empty() || nas.size() > 253 || username.size() > 253) return bad("NAS-Identifier must be 1..253 bytes and username at most 253 bytes");
  if (mode == "auth" && (username.empty() || password_file.empty())) return bad("auth mode requires username and password-file");
  if (mode != "auth" && !password_file.empty()) return bad("password-file is only used in auth mode");
  if (mode == "status" && !username.empty()) return bad("status mode does not send a username");
  net::address_family family;
  if (!net::parse_address_family(family_arg, family)) return bad("Invalid address-family (expected any, ipv4 or ipv6)");
#ifdef USE_SSL
  try {
    const std::string secret = read_secret(secret_file, "Shared secret", 4096);
    const std::string password = mode == "auth" ? read_secret(password_file, "Password", 128) : "";
    if (mode == "reject" && username.empty()) username = "nsclient-radius-probe";
    const auto packet = radius::random_request(secret, username, password, mode == "status", nas);
    auto item = std::make_shared<radius_result>();
    item->server = host;
    item->port = port;
    query_radius(*item, packet, secret, mode, family, timeout);
    filter.add_manual_perf("time");
    filter.match(item);
    helper.post_process(filter);
  } catch (const std::exception &) {
    // No exception text: file paths, identities or credentials must not leak.
    bad("Unable to prepare RADIUS query: check credential files, lengths and OpenSSL MD5 support");
  }
#else
  bad("check_radius requires a build with OpenSSL support");
#endif
}
}  // namespace check_net
