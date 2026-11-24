// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_smb_client.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <cmath>
#include <functional>
#include <memory>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/format_functions.hpp>

namespace smb_client_check {

const char counter_set[] = "SMB Client Shares";

namespace {
const char c_sec_read[] = "Avg. sec/Read";
const char c_sec_write[] = "Avg. sec/Write";
const char c_sec_request[] = "Avg. sec/Data Request";
const char c_credit_stalls[] = "Credit Stalls/sec";
const char c_queue[] = "Current Data Queue Length";
const char c_avg_queue[] = "Avg. Data Queue Length";
const char c_read_bytes[] = "Read Bytes/sec";
const char c_write_bytes[] = "Write Bytes/sec";
const char c_reads[] = "Read Requests/sec";
const char c_writes[] = "Write Requests/sec";
const char c_metadata[] = "Metadata Requests/sec";

double value_of(const std::map<std::string, double> &values, const char *counter) {
  const auto it = values.find(counter);
  return it == values.end() ? 0.0 : it->second;
}

// The counters' fractions of a second as milliseconds, to the microsecond:
// enough to tell a 0.3 ms LAN round trip from a 3 ms one, without printing
// the float noise of the formatted counter.
double seconds_to_ms(const double seconds) { return std::round(seconds * 1000.0 * 1000.0) / 1000.0; }

// Rates and averages to three decimals, for the same reason.
double rounded(const double value) { return std::round(value * 1000.0) / 1000.0; }

long long whole(const double value) { return static_cast<long long>(std::llround(value)); }
}  // namespace

const std::vector<std::string> &counter_names() {
  static const std::vector<std::string> names = {c_sec_read,   c_sec_write,   c_sec_request, c_credit_stalls, c_queue,   c_avg_queue,
                                                 c_read_bytes, c_write_bytes, c_reads,       c_writes,        c_metadata};
  return names;
}

bool share_activity::is_ipc() const { return boost::algorithm::iequals(share, "IPC$"); }

bool split_instance(const std::string &instance, std::string &server, std::string &share) {
  std::string::size_type start = instance.find_first_not_of('\\');
  if (start == std::string::npos) return false;
  const std::string::size_type sep = instance.find('\\', start);
  if (sep == std::string::npos) {
    server = instance.substr(start);
    share.clear();
  } else {
    server = instance.substr(start, sep - start);
    share = instance.substr(sep + 1);
  }
  return !server.empty();
}

std::vector<share_activity> build_shares(const instance_values &instances) {
  std::vector<share_activity> out;
  for (const auto &instance : instances) {
    share_activity a;
    a.name = instance.first;
    if (!split_instance(instance.first, a.server, a.share)) a.server = instance.first;
    const std::map<std::string, double> &v = instance.second;
    a.read_latency = seconds_to_ms(value_of(v, c_sec_read));
    a.write_latency = seconds_to_ms(value_of(v, c_sec_write));
    a.total_latency = seconds_to_ms(value_of(v, c_sec_request));
    a.credit_stalls = rounded(value_of(v, c_credit_stalls));
    a.queue_length = whole(value_of(v, c_queue));
    a.avg_queue_length = rounded(value_of(v, c_avg_queue));
    a.read_bytes_per_sec = whole(value_of(v, c_read_bytes));
    a.write_bytes_per_sec = whole(value_of(v, c_write_bytes));
    a.reads_per_sec = rounded(value_of(v, c_reads));
    a.writes_per_sec = rounded(value_of(v, c_writes));
    a.metadata_per_sec = rounded(value_of(v, c_metadata));
    out.push_back(a);
  }
  return out;
}

namespace check {

namespace {
typedef share_activity filter_obj;
typedef std::shared_ptr<filter_obj> obj_type;

typedef parsers::where::filter_handler_impl<obj_type> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

filter_obj_handler::filter_obj_handler() {
  using parsers::where::type_bool;
  // clang-format off
  registry_.add_string_var("name", [](obj_type o) { return o->name; }, "The share as the counter set names it: \\\\server\\share")
      .add_string_var("server", [](obj_type o) { return o->server; },
                      "The file server, as the client addressed it (a host name, an FQDN or an address; a DFS path resolves to the server behind it)")
      .add_string_var("share", [](obj_type o) { return o->share; }, "The share name on the server");
  registry_.add_int_var("ipc", type_bool, [](obj_type o) { return o->is_ipc() ? 1LL : 0LL; },
                        "1 for the IPC$ share, which carries named pipes (RPC, DFS referrals) rather than file data: a pipe read waits until the server "
                        "has something to say, so its latency is not storage latency. Left out by the default filter")
      .no_perf();

  registry_.add_float("latency", [](obj_type o) { return o->get_latency(); },
                      "The slower of read_latency and write_latency, in milliseconds")
      .add_float_perf("ms", "", "_latency")
      .add_float("read_latency", [](obj_type o) { return o->read_latency; },
                 "Average time one read from the share took, in milliseconds (counter 'Avg. sec/Read', sampled over one second; 0 when "
                 "nothing was read)")
      .add_float_perf("ms", "", "_read_latency")
      .add_float("write_latency", [](obj_type o) { return o->write_latency; },
                 "Average time one write to the share took, in milliseconds (counter 'Avg. sec/Write'; 0 when nothing was written)")
      .add_float_perf("ms", "", "_write_latency")
      .add_float("total_latency", [](obj_type o) { return o->total_latency; },
                 "Average time one read or write took, in milliseconds (counter 'Avg. sec/Data Request')")
      .add_float_perf("ms", "", "_total_latency")
      .add_float("credit_stalls", [](obj_type o) { return o->credit_stalls; },
                 "Requests per second that had to wait because the client ran out of SMB credits (counter 'Credit Stalls/sec'): the server "
                 "is granting fewer credits than the load needs, which delays I/O whatever the network and the disks behind the share do")
      .add_float_perf("", "", "_credit_stalls")
      .add_float("avg_queue_length", [](obj_type o) { return o->avg_queue_length; },
                 "Average number of read and write requests outstanding on the share over the sample (counter 'Avg. Data Queue Length')")
      .add_float_perf("", "", "_avg_queue_length")
      .add_float("reads_per_sec", [](obj_type o) { return o->reads_per_sec; }, "Read requests per second (counter 'Read Requests/sec')")
      .add_float_perf("", "", "_reads_per_sec")
      .add_float("writes_per_sec", [](obj_type o) { return o->writes_per_sec; }, "Write requests per second (counter 'Write Requests/sec')")
      .add_float_perf("", "", "_writes_per_sec")
      .add_float("metadata_per_sec", [](obj_type o) { return o->metadata_per_sec; },
                 "Metadata requests (opens, closes, directory and attribute queries) per second (counter 'Metadata Requests/sec')")
      .add_float_perf("", "", "_metadata_per_sec");

  registry_.add_int_var("queue_length", [](obj_type o) { return o->queue_length; },
                        "Read and write requests outstanding on the share right now (counter 'Current Data Queue Length')")
      .add_int_perf("", "", "_queue_length")
      .add_int_var("read_bytes_per_sec", [](obj_type o) { return o->read_bytes_per_sec; }, "Bytes read from the share per second")
      .add_int_perf("B", "", "_read_bytes_per_sec")
      .add_int_var("write_bytes_per_sec", [](obj_type o) { return o->write_bytes_per_sec; }, "Bytes written to the share per second")
      .add_int_perf("B", "", "_write_bytes_per_sec");
  // clang-format on

  // convert_bytes() and friends, so a template can print the byte rates
  // scaled; the filter grammar has no arithmetic.
  parsers::where::format_functions::register_format_functions(registry_);
}
}  // namespace

void check_smb_client_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           const std::function<counter_snapshot()> &source) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // Latency is the slower direction, so a read-mostly workload cannot hide
  // slow writes in the average. Any credit stall is worth a warning: an
  // idle or healthy share has none, and a stalled request waits for the
  // server whatever the network does. IPC$ is left out by the default
  // filter (see the ipc keyword). A host with no share open is fine: SMB
  // sessions close when idle, so an empty set is the normal state between
  // bursts.
  filter_helper.add_options("latency > 25 or credit_stalls > 0", "latency > 50", "ipc = 0", filter.get_filter_syntax(), "ok");
  filter_helper.add_short_threshold("latency", modern_filter::shorthand::above, modern_filter::shorthand::unit_plain, "25", "50");
  filter_helper.add_short_warning("${latency} or credit_stalls > 0");
  filter_helper.add_syntax("${status}: ${list}",
                           "${name}: read ${read_latency}ms, write ${write_latency}ms, ${credit_stalls} credit stalls/s, queue ${queue_length}",
                           "${server}/${share}", "%(status): No SMB client shares in use", "");
  filter_helper.set_default_perf_config("extra(read_latency;write_latency;queue_length)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const counter_snapshot snapshot = source();
  if (!snapshot.error.empty()) return nscapi::protobuf::functions::set_response_bad(*response, snapshot.error);

  for (const share_activity &a : build_shares(snapshot.instances)) {
    const obj_type record(new filter_obj(a));
    filter.match(record);
  }
  filter_helper.post_process(filter);
}

void check_smb_client_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                           const counter_snapshot &snapshot) {
  check_smb_client_with(request, response, [&snapshot]() { return snapshot; });
}

}  // namespace check

}  // namespace smb_client_check
