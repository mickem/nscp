// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <client/command_line_parser.hpp>
#include <list>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nscapi/protobuf/nagios.hpp>
#include <sstream>
#include <str/utf8.hpp>
#include <string>

#include "gearman_connection.hpp"
#include "gearman_crypt.hpp"
#include "gearman_job.hpp"
#include "gearman_protocol.hpp"

/**
 * The passive submit channel.
 *
 * Mod-Gearman's result queue is not only for workers: the core's result thread
 * files whatever arrives on `check_results`, and a result that says
 * `type=passive` is filed as a passive check. `send_gearman` is nothing more
 * than that, so once the module already has the envelope, the result text and
 * a gearman connection, a submit channel is one handler.
 *
 * What it buys an installation is one daemon fewer: the Scheduler, the REST
 * API and `nscp client` can push passive results into the same gearmand the
 * checks come from, instead of running NSCA alongside it.
 *
 * Unlike the worker, a submission owns its connection for the length of one
 * call. Submissions arrive on channel threads and on command threads, several
 * at a time, and gearmand is happy to take a connection per result; sharing
 * one would need a lock around a socket whose failure mode is a hung thread.
 */
namespace gearman_client {

struct connection_data {
  std::string host;
  std::string port;
  std::string queue;
  gearman::envelope crypto;
  bool insecure = false;
  int timeout = 30;
  /** The host the results are filed under; the core has to know it by this name. */
  std::string sender_hostname;

  connection_data(client::destination_container arguments, client::destination_container sender) {
    host = arguments.address.host;
    port = arguments.address.get_port_string(std::to_string(gearman::default_port));
    queue = arguments.get_string_data("queue");
    if (queue.empty()) queue = gearman::default_result_queue;
    // A target that says nothing about encryption gets it, like the worker:
    // the payload is only as protected as the least careful end of it.
    crypto.encryption = arguments.get_bool_data("encryption", true);
    // See gearman_handler.hpp for why the shared key lives under `password`.
    crypto.key = arguments.get_string_data("password");
    insecure = arguments.get_bool_data("insecure", false);
    timeout = arguments.timeout > 0 ? arguments.timeout : 30;
    // get_host(), not get_string_data("host"): destination_container routes
    // the well-known "host" key into a typed field rather than the data map.
    sender_hostname = sender.get_host();
  }

  std::string get_endpoint_string() const { return host + ":" + port; }

  std::string to_string() const {
    std::stringstream ss;
    ss << "host: " << get_endpoint_string();
    ss << ", queue: " << queue;
    ss << ", timeout: " << timeout;
    ss << ", encryption: " << (crypto.encryption ? "aes-256" : "none");
    // Never the key itself: this line is written at trace level on every
    // submission, and the key is what separates a result the agent filed from
    // one anybody who can reach gearmand made up.
    ss << ", key: " << (crypto.key.empty() ? "<unset>" : "<set>");
    ss << ", hostname: " << sender_hostname;
    return ss.str();
  }
};

struct gearman_client_handler final : public client::handler_interface {
  bool query(client::destination_container, client::destination_container, const PB::Commands::QueryRequestMessage &,
             PB::Commands::QueryResponseMessage &) override {
    return false;
  }

  bool exec(client::destination_container, client::destination_container, const PB::Commands::ExecuteRequestMessage &,
            PB::Commands::ExecuteResponseMessage &) override {
    return false;
  }

  bool metrics(client::destination_container, client::destination_container, const PB::Metrics::MetricsMessage &) override { return false; }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override {
    nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());
    connection_data con(target, sender);

    // The redacted connection_data, never target.to_string(): that one dumps
    // the destination's whole data map, where the key sits in the clear under
    // `password`, and would defeat the redaction above.
    NSC_TRACE_ENABLED() { NSC_TRACE_MSG("Target configuration: " + con.to_string()); }

    PB::Commands::SubmitResponseMessage::Response *payload = response_message.add_payload();
    if (con.host.empty()) {
      nscapi::protobuf::functions::set_response_bad(
          *payload, "No gearmand to submit to. Name one with address=<host>:<port>, or configure a target under /settings/gearman/client/targets.");
      return true;
    }
    if (con.crypto.encryption && con.crypto.key.empty()) {
      nscapi::protobuf::functions::set_response_bad(*payload,
                                                    "No key for " + con.get_endpoint_string() +
                                                        ". The key is the only thing separating a result this agent filed from one anybody who can reach "
                                                        "gearmand made up, so an encrypted submission without one is refused. Set the target's key to the "
                                                        "same value as the core's module.conf.");
      return true;
    }
    if (!con.crypto.encryption && !con.insecure) {
      nscapi::protobuf::functions::set_response_bad(*payload,
                                                    "Encryption is off for " + con.get_endpoint_string() +
                                                        " but 'insecure' is not set. An unencrypted result is readable and forgeable by anyone who can "
                                                        "reach gearmand; set insecure=true to say that is intended.");
      return true;
    }
    if (con.sender_hostname.empty()) {
      // The core matches a result to an object by host_name and discards one
      // it cannot place, without saying so anywhere the agent can see. Failing
      // here is the only chance to tell the operator.
      nscapi::protobuf::functions::set_response_bad(*payload,
                                                    "No host name to file the result under. The core matches a result to a host by name and silently "
                                                    "discards one it does not know; set /settings/gearman/client/hostname (or pass source-host=) to the "
                                                    "host_name the core knows this machine by.");
      return true;
    }

    // One `source` for the whole submission: what an operator reads in the
    // core to tell which agent filed a passive result, in the same shape the
    // worker writes for an active one.
    const std::string source = gearman::format_source(utf8::cvt<std::string>(GET_CORE()->getApplicationVersionString()), con.sender_hostname);

    std::list<gearman::check_result> results;
    for (const PB::Commands::QueryResponseMessage::Response &item : request_message.payload()) {
      gearman::check_result result;
      // What makes the core file this as a passive check rather than believe
      // it scheduled the check itself.
      result.type = "passive";
      result.host_name = con.sender_hostname;
      std::string alias = item.alias();
      if (alias.empty()) alias = item.command();
      // The same convention as NSCA and NRDP: `host_check` is the host's own
      // result, and a result with no service_description is a host result.
      if (alias != "host_check") result.service_description = alias;
      result.return_code = nscapi::protobuf::functions::gbp_to_nagios_status(item.result());
      result.output = nscapi::protobuf::functions::query_data_to_nagios_string(item, gearman::max_output_length);
      // A passive result describes a check that has already been run, and the
      // agent knows nothing more precise about when: both cores read the pair
      // as the check's window and show the latency from it.
      result.start_time = gearman::now_timestamp();
      result.finish_time = result.start_time;
      result.source = source;
      // Without this the core throws the output away and reports that the
      // plugin did not exit properly instead.
      result.exited_ok = 1;
      results.push_back(result);
    }
    if (results.empty()) {
      nscapi::protobuf::functions::set_response_good(*payload, "Nothing to submit");
      return true;
    }

    send(payload, con, results);
    return true;
  }

  void send(PB::Commands::SubmitResponseMessage::Response *payload, const connection_data &con, const std::list<gearman::check_result> &results) {
    try {
      gearman::connection socket;
      socket.connect(gearman::server_address(con.host, con.port), static_cast<unsigned int>(con.timeout));
      for (const gearman::check_result &result : results) {
        // Waiting for the acknowledgement is what a passive channel owes its
        // caller: a background job is fire and forget as far as gearmand is
        // concerned, so without it a submission would report success for a
        // result that never left the socket buffer - and unlike an active
        // check, nobody is waiting for this one on the other side either.
        socket.submit_background(con.queue, gearman::encode_payload(gearman::format_result(result), con.crypto), static_cast<unsigned int>(con.timeout));
      }
      socket.close();
      nscapi::protobuf::functions::set_response_good(*payload, "Submission successful");
    } catch (const gearman::connection_error &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Gearman error: " + utf8::utf8_from_native(e.what()));
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Error: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      nscapi::protobuf::functions::set_response_bad(*payload, "Unknown error -- REPORT THIS!");
    }
  }
};
}  // namespace gearman_client
