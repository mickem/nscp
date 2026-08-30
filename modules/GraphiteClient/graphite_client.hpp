// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif
#include <algorithm>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tuple/tuple.hpp>
#include <chrono>
#include <client/command_line_parser.hpp>
#include <memory>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/nagios.hpp>
#include <stdexcept>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>

namespace graphite_client {
struct connection_data : public socket_helpers::connection_info {
  std::string ppath;
  std::string spath;
  std::string mpath;
  std::string sender_hostname;
  bool send_perf;
  bool send_status;

  connection_data(client::destination_container sender, client::destination_container target) {
    address = target.address.host;
    port_ = target.address.get_port_string("2003");
    // The well-known `timeout` key is routed into the container's typed field
    // (set_string_data), never into the free-form data map, so the old
    // get_int_data("timeout", 30) lookup could not see a configured value and
    // the default always won. Read the typed field, like NRPEClient; the
    // settings layer notifies the documented default (30) for target sections
    // that do not set one.
    if (target.timeout > 0) timeout = target.timeout;
    // No `retry` here: send() makes exactly one attempt per submission and
    // there is no retry loop to feed - reading the setting into the inherited
    // field only made it look honoured. A retry loop would also multiply the
    // worst-case time a submission can hold the submitting thread (the
    // recurring metrics flush lands here) by the retry count - the very thing the
    // whole-submission timeout below exists to bound. Mirrors SMTPClient.
    ppath = target.get_string_data("perf path");
    spath = target.get_string_data("status path");
    send_perf = target.get_bool_data("send perfdata");
    send_status = target.get_bool_data("send status");
    mpath = target.get_string_data("metric path");

    // Optional TLS. Carbon itself is plaintext, so this targets a TLS-terminating
    // proxy (stunnel / nginx / carbon-relay-ng) in front of carbon.
    ssl.enabled = target.get_bool_data("ssl", false);
    ssl.ca_path = target.get_string_data("ca");
    ssl.certificate = target.get_string_data("certificate");
    ssl.certificate_key = target.get_string_data("certificate key");
    ssl.certificate_key_format = target.get_string_data("certificate format", "PEM");
    ssl.allowed_ciphers = target.get_string_data("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    ssl.verify_mode = target.get_string_data("verify mode", "peer");
    ssl.tls_version = target.get_string_data("tls version", "1.2+");

    if (sender.has_data("host"))
      sender_hostname = sender.get_string_data("host");
    else
      sender_hostname = sender.get_host();
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << get_endpoint_string();
    return ss.str();
  }
};

struct g_data {
  std::string path;
  std::string value;
};

std::string fix_graphite_string(const std::string &s) {
  std::string sc = s;
  str::utils::replace(sc, " ", "_");
  str::utils::replace(sc, "\\", "_");
  str::utils::replace(sc, "[", "_");
  str::utils::replace(sc, "]", "_");
  str::utils::replace(sc, "(", "_");
  str::utils::replace(sc, ")", "_");
  str::utils::replace(sc, "%", "percent");
  // Graphite uses the plaintext line protocol "<path> <value> <ts>\n" - one
  // metric per line, fields separated by whitespace. A metric path or value
  // containing a newline would split into multiple Graphite records, and a
  // tab splits a field the same way a space does. `;` is the tag separator
  // in the Graphite carbon tag-aware protocol; an unintended `;` injects
  // fake tags. Replace them all with `_` so a check name or perfdata label
  // with these characters cannot inject metrics.
  str::utils::replace(sc, "\r", "_");
  str::utils::replace(sc, "\n", "_");
  str::utils::replace(sc, "\t", "_");
  str::utils::replace(sc, ";", "_");
  str::utils::replace(sc, std::string("\0", 1), "_");
  return sc;
}
namespace detail {
// Deadline-bounded synchronous IO for the carbon submission, modeled on
// SMTPClient's sync_io.
//
// Boost.Asio synchronous calls take no timeout, so a carbon endpoint that
// black-holes SYNs, stalls mid-TLS-handshake or advertises a zero TCP window
// could hold the submitting thread - channel submissions and the recurring
// metrics flush both land in send() - for the OS-level TCP timeout, or indefinitely on
// a stuck write. Each operation is therefore issued async and raced against a
// deadline on the one io_context.
//
// The deadline is a single budget for the whole submission (resolution
// included), not a fresh allowance per operation: an operator setting
// timeout=30 means "give up on this submission after 30 seconds", not "allow
// 30 seconds per write". Per-operation deadlines multiply out - a host name
// resolving to several dead addresses would get a fresh deadline per connect
// attempt.
class deadline_io {
 public:
  deadline_io(boost::asio::io_context &io, boost::asio::ip::tcp::socket &socket, unsigned int timeout_seconds)
      : io_(io), socket_(socket), timeout_seconds_(timeout_seconds), deadline_(std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds)) {}
  deadline_io(const deadline_io &) = delete;
  deadline_io &operator=(const deadline_io &) = delete;

  // Render a failure for an operator. A spent budget is ours to explain, not
  // the platform's: the platform text for timed_out describes a peer that did
  // not answer, which blames the server for a deadline this client set.
  std::string describe(const boost::system::error_code &ec) const {
    if (ec == boost::asio::error::timed_out) {
      return "timed out after " + str::xtos(timeout_seconds_) + "s (the budget for the whole submission)";
    }
    return ec.message();
  }

  // Resolve under the shared budget. The platform resolver's own timeouts are
  // long (and not ours to set), so a black-holed name server would otherwise
  // stall the submission well past the configured timeout before any socket
  // operation got a chance to be deadlined.
  boost::asio::ip::tcp::resolver::results_type resolve(const std::string &host, const std::string &port) {
    boost::asio::ip::tcp::resolver resolver(io_);
    boost::asio::ip::tcp::resolver::results_type endpoints;
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto &&done) {
          resolver.async_resolve(host, port,
                                 [&endpoints, done = std::move(done)](const boost::system::error_code &e, boost::asio::ip::tcp::resolver::results_type r) {
                                   endpoints = std::move(r);
                                   done(e);
                                 });
        },
        ec);
    if (ec) throw std::runtime_error("DNS resolve failed: " + describe(ec));
    return endpoints;
  }

  // Connect to the first endpoint that succeeds; every attempt draws on the
  // same budget.
  void connect(const boost::asio::ip::tcp::resolver::results_type &endpoints) {
    boost::system::error_code ec = boost::asio::error::host_not_found;
    for (auto it = endpoints.begin(); ec && it != endpoints.end(); ++it) {
      // Non-throwing close: an incidental failure here (closing a socket that
      // was never opened, on the first pass) must not mask the connect result.
      boost::system::error_code close_ec;
      socket_.close(close_ec);
      const auto ep = it->endpoint();
      run_with_deadline([&](auto &&done) { socket_.async_connect(ep, [done = std::move(done)](const boost::system::error_code &e) { done(e); }); }, ec);
      // A spent budget ends the walk, not just this attempt: the deadline
      // covers the whole submission, so every remaining endpoint would time
      // out the same way before its connect could complete.
      if (ec == boost::asio::error::timed_out) break;
    }
    if (ec) throw std::runtime_error("connect failed: " + describe(ec));
  }

  template <typename Stream>
  void write(Stream &stream, const std::string &payload) {
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto &&done) {
          boost::asio::async_write(stream, boost::asio::buffer(payload),
                                   [done = std::move(done)](const boost::system::error_code &e, std::size_t) { done(e); });
        },
        ec);
    if (ec) throw std::runtime_error("write failed: " + describe(ec));
  }

#ifdef USE_SSL
  // asio's synchronous handshake() takes no timeout, so a peer that stalls
  // part-way through one would hang the submission thread indefinitely - the
  // very thing this class exists to prevent everywhere else.
  void handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &stream) {
    boost::system::error_code ec;
    run_with_deadline(
        [&](auto &&done) {
          stream.async_handshake(boost::asio::ssl::stream_base::client, [done = std::move(done)](const boost::system::error_code &e) { done(e); });
        },
        ec);
    if (ec) throw std::runtime_error("TLS handshake failed: " + describe(ec));
  }

  // Best-effort close_notify so the TLS proxy flushes what we sent before it
  // sees the connection drop. asio's shutdown also waits for the peer's own
  // close_notify, which a proxy that simply never sends one would stretch to
  // the full remaining budget on every *successful* submission - so this
  // waits a short grace at most (and never past the budget). A failure here
  // is not a failed submission: the data was already written.
  void shutdown(boost::asio::ssl::stream<boost::asio::ip::tcp::socket> &stream) {
    boost::system::error_code ignored;
    run_until((std::min)(deadline_, std::chrono::steady_clock::now() + std::chrono::seconds(2)),
              [&](auto &&done) { stream.async_shutdown([done = std::move(done)](const boost::system::error_code &e) { done(e); }); }, ignored);
  }
#endif

 private:
  // Run a single async op until it completes or `deadline` expires. Sets
  // `out_ec` to the operation's result, or timed_out on expiry.
  //
  // The op's completion handler can outlive this frame. On timeout the timer
  // handler stop()s the io_context, and the cancelled op's handler - already
  // queued, or queued by the cancel - is left unexecuted; it runs on the NEXT
  // restart()/run(), which happens when a caller issues another operation
  // after a timeout (connect() walking its endpoint list). A handler that
  // captured this frame's locals by reference would then write through
  // dangling references into whatever occupies the stack now. So everything
  // the handler touches lives in `op`, co-owned by the handler itself: a
  // stale handler completes against its own orphaned state, which nobody
  // reads. Mirrors the same fix in SMTPClient's sync_io.
  //
  // The timer handler needs no such care: run() drains it before returning in
  // every case (it is either what called stop(), or it completes as aborted
  // before run() runs out of work), so its frame is always alive.
  template <typename Init>
  void run_until(const std::chrono::steady_clock::time_point deadline, Init &&init, boost::system::error_code &out_ec) {
    struct op_state {
      explicit op_state(boost::asio::io_context &io) : timer(io), ec(boost::asio::error::would_block), timed_out(false) {}
      boost::asio::steady_timer timer;
      boost::system::error_code ec;
      bool timed_out;
    };
    const auto op = std::make_shared<op_state>(io_);
    // A deadline already in the past fires immediately, which is what we want
    // once the budget is spent.
    op->timer.expires_at(deadline);
    op->timer.async_wait([this, op](const boost::system::error_code &e) {
      if (e == boost::asio::error::operation_aborted) return;
      op->timed_out = true;
      // Cancel any outstanding I/O so run() returns. stop() covers the
      // operations cancelling the socket does not reach - a resolve in
      // particular runs off on its own thread and would otherwise keep run()
      // going until the platform resolver gave up on its own schedule.
      boost::system::error_code ignore;
      socket_.cancel(ignore);
      io_.stop();
    });
    init([op](const boost::system::error_code &e) {
      op->ec = e;
      // cancel() can throw (the non-throwing cancel(ec) overload is removed
      // under BOOST_ASIO_NO_DEPRECATED). Swallow it so an incidental failure
      // can't escape this handler and misreport a completed operation.
      try {
        op->timer.cancel();
      } catch (...) {
      }
    });
    io_.restart();
    io_.run();
    out_ec = op->timed_out ? boost::asio::error::timed_out : op->ec;
  }

  template <typename Init>
  void run_with_deadline(Init &&init, boost::system::error_code &out_ec) {
    run_until(deadline_, std::forward<Init>(init), out_ec);
  }

  boost::asio::io_context &io_;
  boost::asio::ip::tcp::socket &socket_;
  unsigned int timeout_seconds_;
  std::chrono::steady_clock::time_point deadline_;
};
}  // namespace detail

struct graphite_client_handler : public client::handler_interface {
  bool query(client::destination_container _sender, client::destination_container _target, const PB::Commands::QueryRequestMessage &_request_message,
             PB::Commands::QueryResponseMessage &_response_message) {
    return false;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) {
    const PB::Common::Header &request_header = request_message.header();
    connection_data con(sender, target);

    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);
    std::string ppath = con.ppath;
    std::string spath = con.spath;
    str::utils::replace(ppath, "${hostname}", con.sender_hostname);
    str::utils::replace(spath, "${hostname}", con.sender_hostname);

    std::list<g_data> list;

    for (const ::PB::Commands::QueryResponseMessage_Response &p : request_message.payload()) {
      std::string tmp_path = ppath;
      str::utils::replace(tmp_path, "${check_alias}", p.alias());

      if (con.send_perf) {
        for (const ::PB::Commands::QueryResponseMessage::Response::Line &l : p.lines()) {
          for (const PB::Common::PerformanceData &perf : l.perf()) {
            g_data d;
            d.path = tmp_path;
            str::utils::replace(d.path, "${perf_alias}", perf.alias());
            d.value = nscapi::protobuf::functions::extract_perf_value_as_string(perf);
            d.path = fix_graphite_string(d.path);
            list.push_back(d);
          }
        }
      }
      if (con.send_status) {
        g_data d;
        d.path = spath;
        str::utils::replace(d.path, "${check_alias}", p.alias());
        d.value = str::xtos(nscapi::protobuf::functions::gbp_to_nagios_status(p.result()));
        // Scrub the whole status path like the perf path above: the alias (and
        // the ${hostname} substituted earlier) may come from a remote submitter
        // (NSCA, forwarded results), so an embedded newline or ';' must not be
        // able to inject extra metric lines or carbon tags.
        d.path = fix_graphite_string(d.path);
        list.push_back(d);
      }
    }
    if (list.empty()) {
      nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), std::string("No performance data to send"));
      return true;
    }
    boost::tuple<int, std::string> ret = send(con, list);
    if (ret.get<0>())
      nscapi::protobuf::functions::set_response_good(*response_message.add_payload(), ret.get<1>());
    else
      nscapi::protobuf::functions::set_response_bad(*response_message.add_payload(), ret.get<1>());

    return true;
  }

  bool exec(client::destination_container _sender, client::destination_container _target, const PB::Commands::ExecuteRequestMessage &_request_message,
            PB::Commands::ExecuteResponseMessage &_response_message) {
    return false;
  }

  void push_metrics(std::list<graphite_client::g_data> &list, const PB::Metrics::MetricsBundle &b, std::string path, std::string mpath) {
    std::string mypath;
    if (!path.empty()) mypath = path + ".";
    mypath += b.key();
    for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
      push_metrics(list, b2, mypath, mpath);
    }
    for (const PB::Metrics::Metric &v : b.value()) {
      graphite_client::g_data d;
      d.path = mpath;
      str::utils::replace(d.path, "${metric}", mypath + "." + v.key());
      d.path = fix_graphite_string(d.path);
      if (v.has_gauge_value()) {
        d.value = str::xtos(v.gauge_value().value());
        list.push_back(d);
      }
    }
  }

  bool metrics(client::destination_container sender, client::destination_container target, const PB::Metrics::MetricsMessage &request_message) {
    std::list<graphite_client::g_data> list;
    connection_data con(sender, target);
    std::string mpath = con.mpath;
    str::utils::replace(mpath, "${hostname}", con.sender_hostname);

    for (const PB::Metrics::MetricsMessage::Response &r : request_message.payload()) {
      for (const PB::Metrics::MetricsBundle &b : r.bundles()) {
        push_metrics(list, b, "", mpath);
      }
    }
    send(con, list);
    return true;
  }

  // Build the carbon line for one datum. The value is scrubbed like the path:
  // the Graphite line protocol is "<path> <value> <ts>\n", so a value with a
  // space, newline or ';' could otherwise inject an extra metric/tag line.
  // fix_graphite_string leaves a normal numeric value untouched.
  static std::string make_line(const g_data &d, const std::string &ts) { return d.path + " " + fix_graphite_string(d.value) + " " + ts + "\n"; }

  boost::tuple<bool, std::string> send(connection_data con, const std::list<g_data> &data) {
    try {
      boost::asio::io_context io_service;

      const boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
      const boost::posix_time::time_duration diff = boost::posix_time::microsec_clock::universal_time() - time_t_epoch;
      const std::string ts = boost::lexical_cast<std::string>(diff.total_seconds());

      // One buffer for the whole batch: the carbon protocol is just
      // newline-separated lines, so a single deadlined write replaces a
      // per-line loop (and the per-line socket.send() ignored short writes,
      // which async_write completes).
      std::string payload;
      for (const g_data &d : data) payload += make_line(d, ts);

#ifdef USE_SSL
      if (con.ssl.enabled) {
        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        std::list<std::string> errors;
        con.ssl.configure_ssl_context(ctx, errors);
        if (!errors.empty()) {
          std::string emsg;
          for (const std::string &e : errors) emsg += (emsg.empty() ? "" : "; ") + e;
          return boost::make_tuple(false, "TLS setup failed: " + emsg);
        }
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream(io_service, ctx);
        detail::deadline_io io(io_service, stream.next_layer(), con.timeout);

        // SNI: a TLS proxy fronting carbon (nginx/stunnel/carbon-relay-ng) often
        // hosts several certs and needs the server name to pick the right one;
        // without it the server may answer with its default cert and the hostname
        // check below fails. Mirrors the HTTP client.
        if (!con.get_address().empty()) {
          SSL_set_tlsext_host_name(stream.native_handle(), con.get_address().c_str());
        }

        // When peer verification is enabled, pin the certificate to the host we
        // resolved so a CA-signed cert from another host cannot impersonate the
        // target (MITM guard) - mirrors the shared socket client.
        if ((con.ssl.get_verify_mode() & boost::asio::ssl::context_base::verify_peer) != 0) {
          stream.set_verify_callback(boost::asio::ssl::host_name_verification(con.get_address()));
        }

        io.connect(io.resolve(con.get_address(), con.get_port()));
        io.handshake(stream);
        io.write(stream, payload);
        io.shutdown(stream);
        return boost::make_tuple(true, "Data presumably sent successfully (TLS)");
      }
#else
      if (con.ssl.enabled) {
        return boost::make_tuple(false, "TLS requested ('ssl = true') but NSClient++ was built without OpenSSL support");
      }
#endif

      boost::asio::ip::tcp::socket socket(io_service);
      detail::deadline_io io(io_service, socket, con.timeout);
      io.connect(io.resolve(con.get_address(), con.get_port()));
      io.write(socket, payload);
      return boost::make_tuple(true, "Data presumably sent successfully");
    } catch (const std::runtime_error &e) {
      return boost::make_tuple(false, "Socket error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      return boost::make_tuple(false, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      return boost::make_tuple(false, "Unknown error -- REPORT THIS!");
    }
  }
};
}  // namespace graphite_client