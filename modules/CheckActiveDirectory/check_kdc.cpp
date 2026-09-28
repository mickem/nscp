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
#include <nscapi/macros.hpp>
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

#include "bounded_worker.hpp"
#include "dsrole_buffer.hpp"
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
  std::string get_principal() const { return principal; }
  std::string get_response() const { return response; }
  long long get_port() const { return port; }
  long long get_responding() const { return responding; }
  long long get_error_code() const { return error_code; }
  boost::optional<long long> get_time() const { return time; }

  std::string show() const { return kdc; }

  std::string kdc;        // host probed
  std::string realm;      // realm the AS-REQ was for
  std::string principal;  // client principal the AS-REQ named
  std::string response;   // what came back ("KRB-ERROR ..." / "AS-REP ..." / transport error)
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
        .add_string_var("principal", &filter_obj::get_principal, "The client principal the probe named (this machine's account unless principal= was given)")
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
  explicit probe_state(boost::asio::io_context &io) : socket(io), header{}, done(false), timed(false) {}

  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::socket socket;
  std::array<unsigned char, 4> header;  // RFC 4120 7.2.2 length prefix
  bool done;
  // Round-trip time is measured from when the connect starts (timed set), so
  // a slow DNS server is not billed to the KDC.
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

// A name lookup, filled in on a worker the check may stop waiting for.
struct lookup_result {
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  std::string error;
};

// Start the framed request/response exchange (RFC 4120 7.2.2: 4-byte
// big-endian length prefix) with a host whose endpoints are known.
void begin_exchange(probe_state &st, const kdc_probe::bytes &framed) {
  namespace asio = boost::asio;
  using boost::asio::ip::tcp;
  st.start_exchange();
  asio::async_connect(st.socket, st.endpoints, [&st, &framed](const boost::system::error_code &ec, const tcp::endpoint &) {
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
}

// How often the event loop looks up from the exchanges to pick up finished
// name lookups. It only delays when an exchange starts, never what it measures.
const std::chrono::milliseconds kLookupPoll(10);

// Probe every host concurrently under one deadline covering the name lookups
// and the exchange, so the worst case costs one timeout rather than one per
// unreachable KDC.
//
// Names are not looked up with async_resolve on this io_context: asio runs
// getaddrinfo on a private thread that the io_context joins when it is
// destroyed, so a DNS server that never answered held the check open for the
// OS resolver timeout however small timeout= was. Each lookup runs on a parked
// worker instead (see bounded_worker.hpp); one that misses the deadline is
// reported as such and left to finish on its own. A host starts its exchange
// as soon as its own lookup is done, so a slow name never costs the others
// their time, and an IP address needs no lookup at all.
std::vector<probe_outcome> exchange_with_kdcs(const std::vector<std::string> &hosts, int port, int timeout_ms, const kdc_probe::bytes &request) {
  namespace asio = boost::asio;
  using boost::asio::ip::tcp;

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  const std::string service = str::xtos(port);
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
  std::vector<std::pair<check_ad::bounded_workers::handle, std::shared_ptr<lookup_result>>> lookups(hosts.size());

  for (std::size_t i = 0; i < hosts.size(); ++i) {
    states.emplace_back(io);
    probe_state &st = states.back();
    boost::system::error_code ec;
    const asio::ip::address address = asio::ip::make_address(hosts[i], ec);
    if (!ec) {
      st.endpoints.push_back(tcp::endpoint(address, static_cast<unsigned short>(port)));
      begin_exchange(st, framed);
      continue;
    }
    const std::shared_ptr<lookup_result> result = std::make_shared<lookup_result>();
    const std::string host = hosts[i];
    const check_ad::bounded_workers::handle worker = check_ad::module_workers().start(
        "kdc resolve " + host + ":" + service, "check_kdc resolve " + host,
        [result, host, service]() {
          asio::io_context lookup_io;
          tcp::resolver resolver(lookup_io);
          boost::system::error_code resolve_ec;
          const tcp::resolver::results_type found = resolver.resolve(host, service, resolve_ec);
          if (resolve_ec) {
            result->error = "resolve failed: " + resolve_ec.message();
            return;
          }
          for (const tcp::resolver::results_type::value_type &entry : found) result->endpoints.push_back(entry.endpoint());
        },
        NSC_THREAD_REPORTER);
    if (!worker) {
      st.finish("resolve failed: the previous lookup of " + host + " has still not returned");
      continue;
    }
    lookups[i] = std::make_pair(worker, result);
  }

  // Keeps run_for() blocking for its whole slice while the only thing left to
  // wait for is a lookup; without work it would return at once and spin.
  asio::steady_timer keep_alive(io, deadline);
  keep_alive.async_wait([](const boost::system::error_code &) {});

  while (std::chrono::steady_clock::now() < deadline) {
    bool waiting = false;
    for (std::size_t i = 0; i < states.size(); ++i) {
      if (lookups[i].first && lookups[i].first->is_done()) {
        const lookup_result &result = *lookups[i].second;
        if (!result.error.empty()) {
          states[i].finish(result.error);
        } else {
          states[i].endpoints = result.endpoints;
          begin_exchange(states[i], framed);
        }
        lookups[i].first.reset();
      }
      if (!states[i].done) waiting = true;
    }
    if (!waiting) break;
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
    if (left.count() <= 0) break;
    io.run_for((std::min)(left, kLookupPoll));
  }

  std::vector<probe_outcome> out;
  for (std::size_t i = 0; i < states.size(); ++i) {
    probe_state &st = states[i];
    if (!st.done) {
      boost::system::error_code ignored;
      st.socket.close(ignored);
      st.finish(lookups[i].first ? "resolve failed: no answer from DNS in time" : "timeout after " + str::xtos(timeout_ms) + "ms");
    }
    out.push_back(std::move(st.out));
  }
  return out;
}

// This machine's account in the domain it is joined to, as an AS-REQ names it
// (the sAMAccountName, "HOST$"), and that domain's DNS name. Both empty when
// the machine is not joined to an Active Directory domain. Local calls only:
// nothing here goes to the network.
void local_machine_account(std::string &domain, std::string &account) {
  const check_ad::ds_role_ptr info = check_ad::primary_domain_info(nullptr);
  if (!info || info->DomainNameDns == nullptr) return;
  if (info->MachineRole == DsRole_RoleStandaloneWorkstation || info->MachineRole == DsRole_RoleStandaloneServer) return;
  wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
  if (!GetComputerNameExW(ComputerNamePhysicalNetBIOS, name, &size) || size == 0) return;
  domain = utf8::cvt<std::string>(std::wstring(info->DomainNameDns));
  account = utf8::cvt<std::string>(std::wstring(name, size)) + "$";
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

// A Kerberos realm is a domain name and a principal an account name, so both
// fit comfortably; the cap keeps a caller from making the agent write a large
// payload at an arbitrary host.
const std::size_t kMaxRealmLength = 255;
const std::size_t kMaxPrincipalLength = 255;

}  // namespace

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<kdc_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> servers;
  std::string realm;
  std::string principal;
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
    ("principal", po::value<std::string>(&principal),
        "Client principal to name in the AS-REQ (default: this machine's account, HOST$, when probing the domain it is joined to; required for any other realm). Name an account that exists and requires pre-authentication.")
    ("port", po::value<int>(&port)->default_value(88), "TCP port to probe.")
    ("timeout", po::value<int>(&timeout_ms)->default_value(5000),
        "Timeout in milliseconds for the probes, name lookups included. All KDCs are probed concurrently, so this also bounds the whole check.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (realm.size() > kMaxRealmLength) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "realm= is too long (max " + str::xtos(kMaxRealmLength) + " characters, got " + str::xtos(realm.size()) + ")");
  }
  if (principal.size() > kMaxPrincipalLength) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "principal= is too long (max " + str::xtos(kMaxPrincipalLength) + " characters, got " + str::xtos(principal.size()) + ")");
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
          *response, "Failed to locate a KDC (is this machine domain-joined?): " + check_ad::win32_error(rc) + ". Specify server=, realm= and principal=.");
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

  // An AS-REQ for a name that does not exist gets KDC_ERR_C_PRINCIPAL_UNKNOWN
  // back and is logged on the DC as a failed ticket request (4768): run
  // against every DC, that is the pattern user-enumeration detections alert
  // on. So the probe names an account that exists - by default this machine's
  // own, whose healthy answer is KDC_ERR_PREAUTH_REQUIRED - and asks for one
  // rather than invent a name when it knows of none in the realm.
  std::string joined_domain, machine_account;
  local_machine_account(joined_domain, machine_account);
  const std::string client = kdc_probe::choose_principal(principal, realm, joined_domain, machine_account);
  if (client.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "principal= is required to probe " + realm +
                                                                        ": this machine has no account in that realm. Name an account that exists "
                                                                        "there and requires pre-authentication.");
  }

  const kdc_probe::bytes as_req = kdc_probe::build_as_req(realm, client, 12345678UL);

  const std::vector<probe_outcome> outcomes = exchange_with_kdcs(servers, port, timeout_ms, as_req);

  parsers::where::constants::reset();
  for (std::size_t i = 0; i < servers.size(); ++i) {
    kdc_filter::filter_obj_ptr obj = std::make_shared<kdc_filter::filter_obj>();
    obj->kdc = servers[i];
    obj->realm = realm;
    obj->principal = client;
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
