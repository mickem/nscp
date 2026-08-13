// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_secure_channel.hpp"

// Windows.h must precede lm.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <lm.h>

#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/utf8.hpp>
#include <string>

#include "netapi_buffer.hpp"
#include "win32_error.hpp"

// Older SDK headers spell only some of the netlogon function codes out.
#ifndef NETLOGON_CONTROL_TC_QUERY
#define NETLOGON_CONTROL_TC_QUERY 6
#endif
#ifndef NETLOGON_CONTROL_TC_VERIFY
#define NETLOGON_CONTROL_TC_VERIFY 10
#endif

namespace po = boost::program_options;

namespace secure_channel_filter {

// The state of the machine-account secure channel to one trusted domain,
// as reported (and optionally re-established) by the netlogon service.
struct filter_obj {
  filter_obj() : error_code(0) {}

  std::string get_domain() const { return domain; }
  std::string get_dc() const { return dc; }
  std::string get_error_message() const { return error_message; }
  long long get_error_code() const { return error_code; }
  long long get_healthy() const { return error_code == 0 ? 1 : 0; }

  std::string show() const { return domain; }

  std::string domain;         // the trusted domain the channel points at
  std::string dc;             // the DC the channel is established with ("" when broken)
  std::string error_message;  // "OK" or the formatted win32 failure
  long long error_code;       // win32 status of the channel (0 = healthy)
};

typedef std::shared_ptr<filter_obj> filter_obj_ptr;

typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler() {
    using parsers::where::type_bool;
    using parsers::where::type_int;
    // clang-format off
    registry_.add_string_var("domain", &filter_obj::get_domain, "The trusted domain the secure channel points at")
        .add_string_var("dc", &filter_obj::get_dc, "The domain controller the secure channel is established with")
        .add_string_var("error_message", &filter_obj::get_error_message, "Human readable channel state (OK or the failure message)");
    registry_.add_int_var("error_code", type_int, &filter_obj::get_error_code, "Win32 status of the secure channel (0 = healthy)");
    registry_.add_int_var("healthy", type_bool, &filter_obj::get_healthy, "True when the secure channel is established and verified");
    // clang-format on
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace secure_channel_filter

namespace check_secure_channel_command {

namespace {

// Query (or verify, which re-pings the DC) the secure channel for `domain` on
// `server` (empty = local). Returns an empty pointer (with `error` set) when
// the netlogon call itself failed — service stopped, access denied, RPC
// failure — which says nothing about the channel and must not be scored as a
// broken one; only netlog2_tc_connection_status judges channel health.
secure_channel_filter::filter_obj_ptr query_channel(const std::string &server, const std::string &domain, bool verify, std::string &error) {
  secure_channel_filter::filter_obj_ptr obj = std::make_shared<secure_channel_filter::filter_obj>();
  obj->domain = domain;

  const std::wstring server_w = utf8::cvt<std::wstring>(server);
  // I_NetLogonControl2 takes the trusted domain name by address, so the buffer
  // has to outlive the call. Take it by &[0] rather than data(): the non-const
  // data() overload is C++17 and the oldest supported MSVC toolset compiles
  // this as C++14. check() has already rejected an empty domain, so index 0 is
  // a real character, and netlogon only reads through the pointer.
  std::wstring domain_w = utf8::cvt<std::wstring>(domain);
  LPWSTR domain_ptr = &domain_w[0];
  PNETLOGON_INFO_2 raw_info = nullptr;
  const DWORD rc = I_NetLogonControl2(server.empty() ? nullptr : server_w.c_str(), verify ? NETLOGON_CONTROL_TC_VERIFY : NETLOGON_CONTROL_TC_QUERY, 2,
                                      reinterpret_cast<LPBYTE>(&domain_ptr), reinterpret_cast<LPBYTE *>(&raw_info));
  const check_ad::net_api_ptr<NETLOGON_INFO_2> info = check_ad::adopt_net_api(raw_info);
  if (rc != NERR_Success || !info) {
    error = "Failed to query the netlogon service" + (server.empty() ? std::string() : " on " + server) + ": " + check_ad::win32_error(rc);
    return secure_channel_filter::filter_obj_ptr();
  }
  obj->error_code = info->netlog2_tc_connection_status;
  if (info->netlog2_trusted_dc_name != nullptr) {
    std::string dc = utf8::cvt<std::string>(std::wstring(info->netlog2_trusted_dc_name));
    while (!dc.empty() && dc[0] == '\\') dc.erase(0, 1);
    obj->dc = dc;
  }
  obj->error_message = obj->error_code == 0 ? "OK" : check_ad::win32_error(static_cast<unsigned long>(obj->error_code));
  return obj;
}

}  // namespace

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<secure_channel_filter::filter> filter_helper(request, response, data);

  std::string domain;
  std::string server;
  bool verify = true;

  secure_channel_filter::filter filter;
  filter_helper.add_options("", "healthy = 0", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "secure channel to ${domain} via ${dc}: ${error_message}", "${domain}", "", "");
  // The bool threshold keyword is for alerting, not graphing.
  filter_helper.set_default_perf_config("healthy(ignored:true)error_code(ignored:true)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("domain", po::value<std::string>(&domain), "The trusted domain to check the channel to (default: the domain this machine is joined to).")
    ("server", po::value<std::string>(&server), "The computer whose secure channel to check (default: the local machine).")
    ("verify", po::value<bool>(&verify)->implicit_value(true)->default_value(true),
        "Actively verify the channel by contacting the DC (netlogon TC_VERIFY). Set verify=false for a passive status query only.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  if (domain.empty()) {
    // The join state must come from the machine being checked: with server=
    // given, the local join state is the wrong computer's (and a workgroup
    // monitoring host would wrongly conclude there is nothing to check).
    const std::wstring server_w = utf8::cvt<std::wstring>(server);
    const std::string target = server.empty() ? "This machine" : server;
    LPWSTR raw_name = nullptr;
    NETSETUP_JOIN_STATUS status = NetSetupUnknownStatus;
    if (NetGetJoinInformation(server.empty() ? nullptr : server_w.c_str(), &raw_name, &status) == NERR_Success) {
      const check_ad::net_api_ptr<WCHAR> name_buf = check_ad::adopt_net_api(raw_name);
      const std::string join_name = name_buf ? utf8::cvt<std::string>(std::wstring(name_buf.get())) : "";
      if (status != NetSetupDomainName) {
        return nscapi::protobuf::functions::set_response_bad(*response, target + " is not joined to a domain (" +
                                                                            (join_name.empty() ? std::string("standalone") : "workgroup " + join_name) +
                                                                            "); there is no secure channel to check");
      }
      if (join_name.empty()) {
        // A domain join with no name to query the channel for: nothing sane to
        // pass to netlogon, so say so rather than asking about the empty domain.
        return nscapi::protobuf::functions::set_response_bad(
            *response, target + " reports a domain join but no domain name; pass domain= to name the trusted domain explicitly");
      }
      domain = join_name;
    } else {
      return nscapi::protobuf::functions::set_response_bad(*response,
                                                           "Failed to read the domain join state" + (server.empty() ? std::string() : " of " + server) +
                                                               " (and no domain= was given)");
    }
  }

  std::string error;
  const secure_channel_filter::filter_obj_ptr channel = query_channel(server, domain, verify, error);
  if (!channel) return nscapi::protobuf::functions::set_response_bad(*response, error);
  filter.match(channel);
  filter_helper.post_process(filter);
}

}  // namespace check_secure_channel_command
