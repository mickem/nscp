// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_kdc.hpp"

// boost/asio must precede Windows.h so winsock2.h is included first.
#include <boost/asio.hpp>

// Windows.h must precede dsgetdc.h/lm.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <dsgetdc.h>
#include <lm.h>

#include <array>
#include <boost/optional.hpp>
#include <chrono>
#include <deque>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>
#include <vector>

#include "kdc_probe.hpp"
#include "netapi_buffer.hpp"
#include "win32_error.hpp"

namespace po = boost::program_options;

namespace kdc_filter {

// The result of probing one KDC: did it answer the AS-REQ, with what, and how fast.
struct filter_obj {
  filter_obj() : port(0), responding(0), error_code(-1) {}

  std::string get_kdc() const { return kdc; }
  std::string get_realm() const { return realm; }
  std::string get_response() const { return response; }
  long long get_port() const { return port; }
  long long get_responding() const { return responding; }
  long long get_error_code() const { return error_code; }
  boost::optional<long long> get_time() const { return time; }

  std::string show() const { return kdc; }

  std::string kdc;       // host probed
  std::string realm;     // realm the AS-REQ was for
  std::string response;  // what came back ("KRB-ERROR ..." / "AS-REP ..." / transport error)
  long long port;
  long long responding;  // 1 when a well-formed Kerberos answer arrived
  long long error_code;  // KRB-ERROR code (-1 when none)
  // Round-trip in milliseconds, empty when the exchange never started (a name
  // that will not resolve). There is no round trip to report then, and a
  // sentinel would put a negative latency into the graph for good.
  boost::optional<long long> time;
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler() {
    using parsers::where::type_bool;
    using parsers::where::type_int;
    // clang-format off
    registry_.add_string_var("kdc", &filter_obj::get_kdc, "The KDC host that was probed")
        .add_string_var("realm", &filter_obj::get_realm, "The Kerberos realm the probe requested a ticket for")
        .add_string_var("response", &filter_obj::get_response, "What the KDC answered (or the transport error)");
    registry_
        .add_optional_int_var("time", type_int, &filter_obj::get_time, "?",
                              "Probe round-trip time in milliseconds (none when the host never resolved)")
        .add_int_perf("ms");
    registry_.add_int_var("port", type_int, &filter_obj::get_port, "TCP port probed");
    registry_.add_int_var("responding", type_bool, &filter_obj::get_responding, "True when the KDC answered the AS-REQ with a well-formed Kerberos message");
    registry_.add_int_var("error_code", type_int, &filter_obj::get_error_code, "KRB-ERROR code from the response (-1 when none)");
    // clang-format on
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace kdc_filter

namespace check_kdc_command {

namespace {

struct probe_outcome {
  bool exchanged;
  std::string error;
  kdc_probe::bytes response;
  boost::optional<long long> time_ms;  // empty until the exchange actually starts

  probe_outcome() : exchanged(false) {}
};

// The in-flight state of one KDC probe on the shared io_context.
struct probe_state {
  explicit probe_state(boost::asio::io_context &io) : resolver(io), socket(io), header{}, done(false), timed(false) {}

  boost::asio::ip::tcp::resolver resolver;
  boost::asio::ip::tcp::socket socket;
  std::array<unsigned char, 4> header;  // RFC 4120 7.2.2 length prefix
  bool done;
  // Round-trip time is measured from when the resolver answered (timed set),
  // so a slow DNS server is not billed to the KDC.
  bool timed;
  std::chrono::steady_clock::time_point exchange_start;
  probe_outcome out;

  void start_exchange() {
    exchange_start = std::chrono::steady_clock::now();
    timed = true;
  }
  // Terminal handlers stamp the time here; measuring after the event loop
  // exits would bill a fast KDC for the full deadline whenever a slow one
  // keeps the loop running.
  void finish(const std::string &error) {
    out.error = error;
    if (timed) out.time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - exchange_start).count();
    done = true;
  }
};

// One framed request/response exchange (RFC 4120 7.2.2: 4-byte big-endian
// length prefix) per host against host:port. All hosts are probed concurrently
// on one io_context with a single deadline, so the worst case costs one
// timeout rather than one per unreachable KDC. A DNS lookup the OS never
// answers can still hold the io_context destructor past the deadline (asio
// runs getaddrinfo on a worker thread it joins on shutdown), but at most once
// for the whole batch.
std::vector<probe_outcome> exchange_with_kdcs(const std::vector<std::string> &hosts, int port, int timeout_ms, const kdc_probe::bytes &request) {
  namespace asio = boost::asio;
  using boost::asio::ip::tcp;

  const std::size_t size = request.size();
  const std::array<unsigned char, 4> prefix = {static_cast<unsigned char>((size >> 24) & 0xff), static_cast<unsigned char>((size >> 16) & 0xff),
                                               static_cast<unsigned char>((size >> 8) & 0xff), static_cast<unsigned char>(size & 0xff)};
  kdc_probe::bytes framed;
  framed.reserve(size + prefix.size());
  framed.insert(framed.end(), prefix.begin(), prefix.end());
  framed.insert(framed.end(), request.begin(), request.end());

  asio::io_context io;
  // deque: the completion handlers hold references into the container.
  std::deque<probe_state> states;

  for (const std::string &host : hosts) {
    states.emplace_back(io);
    probe_state &st = states.back();
    st.resolver.async_resolve(host, str::xtos(port), [&st, &framed](const boost::system::error_code &ec, tcp::resolver::results_type results) {
      if (ec) {
        st.finish("resolve failed: " + ec.message());
        return;
      }
      st.start_exchange();
      asio::async_connect(st.socket, results, [&st, &framed](const boost::system::error_code &ec, const tcp::endpoint &) {
        if (ec) {
          st.finish("connect failed: " + ec.message());
          return;
        }
        asio::async_write(st.socket, asio::buffer(framed), [&st](const boost::system::error_code &ec, std::size_t) {
          if (ec) {
            st.finish("send failed: " + ec.message());
            return;
          }
          asio::async_read(st.socket, asio::buffer(st.header), [&st](const boost::system::error_code &ec, std::size_t) {
            if (ec) {
              st.finish("read failed: " + ec.message());
              return;
            }
            const std::size_t len = (static_cast<std::size_t>(st.header[0]) << 24) | (static_cast<std::size_t>(st.header[1]) << 16) |
                                    (static_cast<std::size_t>(st.header[2]) << 8) | static_cast<std::size_t>(st.header[3]);
            if (len == 0 || len > 512 * 1024) {
              st.finish("invalid response length");
              return;
            }
            st.out.response.resize(len);
            asio::async_read(st.socket, asio::buffer(st.out.response), [&st](const boost::system::error_code &ec, std::size_t) {
              if (ec) {
                st.out.response.clear();
                st.finish("read failed: " + ec.message());
                return;
              }
              st.out.exchanged = true;
              st.finish("");
            });
          });
        });
      });
    });
  }

  io.run_for(std::chrono::milliseconds(timeout_ms));

  std::vector<probe_outcome> out;
  for (probe_state &st : states) {
    if (!st.done) {
      boost::system::error_code ignored;
      st.socket.close(ignored);
      st.resolver.cancel();
      st.finish("timeout after " + str::xtos(timeout_ms) + "ms");
    }
    out.push_back(std::move(st.out));
  }
  return out;
}

std::string strip_leading_backslashes(std::string s) {
  while (!s.empty() && s[0] == '\\') s.erase(0, 1);
  return s;
}

// Kerberos realms are conventionally the uppercase DNS domain, and AD always
// reports them that way. Uppercase ASCII only: std::toupper is locale
// dependent and would mangle the UTF-8 bytes of an internationalised realm.
std::string upper_ascii(std::string s) {
  for (char &c : s) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  }
  return s;
}

// A Kerberos realm is a domain name, so it fits comfortably; the cap keeps a
// caller from making the agent write a large payload at an arbitrary host.
const std::size_t kMaxRealmLength = 255;

}  // namespace

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<kdc_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> servers;
  std::string realm;
  int port = 88;
  int timeout_ms = 5000;

  kdc_filter::filter filter;
  filter_helper.add_options("time > 1000", "responding = 0", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${kdc}: ${response} (${time}ms)", "${kdc}", "", "%(status): all %(count) KDC(s) are responding");
  // The bool threshold keyword is for alerting; time (registered with perf) is
  // the series worth graphing.
  filter_helper.set_default_perf_config("responding(ignored:true)error_code(ignored:true)port(ignored:true)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("server", po::value<std::vector<std::string>>(&servers), "KDC host to probe; can be given multiple times (default: the KDC located via the domain join).")
    ("realm", po::value<std::string>(&realm), "Kerberos realm to request a ticket for (default: the joined domain; required when not domain-joined).")
    ("port", po::value<int>(&port)->default_value(88), "TCP port to probe.")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000), "Timeout in milliseconds. All KDCs are probed concurrently, so this also bounds the whole check.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (realm.size() > kMaxRealmLength) {
    return nscapi::protobuf::functions::set_response_bad(*response,
                                                         "realm= is too long (max " + str::xtos(kMaxRealmLength) + " characters, got " +
                                                             str::xtos(realm.size()) + ")");
  }

  const bool realm_was_given = !realm.empty();
  if (servers.empty() || realm.empty()) {
    DOMAIN_CONTROLLER_INFOW *raw_info = nullptr;
    const DWORD rc = DsGetDcNameW(nullptr, nullptr, nullptr, nullptr, DS_KDC_REQUIRED | DS_RETURN_DNS_NAME, &raw_info);
    const check_ad::net_api_ptr<DOMAIN_CONTROLLER_INFOW> info = check_ad::adopt_net_api(raw_info);
    if (rc == ERROR_SUCCESS && info) {
      if (servers.empty() && info->DomainControllerName != nullptr) {
        servers.push_back(strip_leading_backslashes(utf8::cvt<std::string>(std::wstring(info->DomainControllerName))));
      }
      if (realm.empty() && info->DomainName != nullptr) realm = utf8::cvt<std::string>(std::wstring(info->DomainName));
    } else if (servers.empty()) {
      return nscapi::protobuf::functions::set_response_bad(
          *response, "Failed to locate a KDC (is this machine domain-joined?): " + check_ad::win32_error(rc) + ". Specify server= and realm=.");
    }
  }
  if (realm.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "realm= is required when no realm can be discovered from the domain join");
  }
  if (servers.empty()) {
    // The join lookup succeeded but named no controller. Probing nothing would
    // otherwise render the ok-syntax as "all 0 KDC(s) are responding".
    return nscapi::protobuf::functions::set_response_bad(*response, "No KDC could be discovered from the domain join; specify server=");
  }
  // Only normalise what we discovered: an explicit realm= is passed through as
  // typed, since Kerberos realms are case sensitive and a non-AD KDC may well
  // serve a lowercase one.
  if (!realm_was_given) realm = upper_ascii(realm);

  const kdc_probe::bytes as_req = kdc_probe::build_as_req(realm, "nscp-probe", 12345678UL);

  const std::vector<probe_outcome> outcomes = exchange_with_kdcs(servers, port, timeout_ms, as_req);

  parsers::where::constants::reset();
  for (std::size_t i = 0; i < servers.size(); ++i) {
    kdc_filter::filter_obj_ptr obj = std::make_shared<kdc_filter::filter_obj>();
    obj->kdc = servers[i];
    obj->realm = realm;
    obj->port = port;
    const probe_outcome &outcome = outcomes[i];
    obj->time = outcome.time_ms;
    if (outcome.exchanged) {
      const kdc_probe::classification c = kdc_probe::classify_response(outcome.response);
      obj->responding = c.alive() ? 1 : 0;
      obj->response = c.describe();
      obj->error_code = c.error_code;
    } else {
      obj->responding = 0;
      obj->response = outcome.error;
    }
    filter.match(obj);
  }
  filter_helper.post_process(filter);
}

}  // namespace check_kdc_command
