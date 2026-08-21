// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for include/client/command_line_parser.cpp - the machinery every
// outbound client module (NRPEClient, NSCAClient, ...) shares.
//
// client::configuration is what turns an incoming query/exec/submit request
// into a call on the module's handler: it picks the target, expands command
// aliases, builds the payload from the request's own arguments and decides
// whether a command is a query, an exec, a submit or a plain forward. All of
// that is driven here through a recording handler, so the assertions are
// about the dispatch decisions rather than any particular protocol.

#include <gtest/gtest.h>

#include <boost/program_options.hpp>
#include <client/command_line_parser.hpp>
#include <memory>
#include <string>
#include <vector>

namespace po = boost::program_options;

namespace {

// Handler that records what it was asked to do and answers with whatever the
// test set up. The real ones talk to a remote host over NRPE/NSCA/HTTP.
class recording_handler : public client::handler_interface {
 public:
  bool query_result = true;
  bool submit_result = true;
  bool exec_result = true;
  bool metrics_result = true;

  int query_calls = 0;
  int submit_calls = 0;
  int exec_calls = 0;
  int metrics_calls = 0;

  client::destination_container last_target;
  client::destination_container last_sender;
  PB::Commands::QueryRequestMessage last_query;
  PB::Commands::SubmitRequestMessage last_submit;
  PB::Commands::ExecuteRequestMessage last_exec;

  bool query(client::destination_container sender, client::destination_container target, const PB::Commands::QueryRequestMessage &request_message,
             PB::Commands::QueryResponseMessage &response_message) override {
    query_calls++;
    last_sender = sender;
    last_target = target;
    last_query = request_message;
    if (!query_result) return false;
    PB::Commands::QueryResponseMessage::Response *payload = response_message.add_payload();
    payload->set_command("handled");
    payload->set_result(PB::Common::ResultCode::OK);
    payload->add_lines()->set_message("from the handler");
    return true;
  }

  bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
              PB::Commands::SubmitResponseMessage &response_message) override {
    submit_calls++;
    last_sender = sender;
    last_target = target;
    last_submit = request_message;
    if (!submit_result) return false;
    PB::Commands::SubmitResponseMessage::Response *payload = response_message.add_payload();
    payload->set_command("handled");
    payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
    return true;
  }

  bool exec(client::destination_container sender, client::destination_container target, const PB::Commands::ExecuteRequestMessage &request_message,
            PB::Commands::ExecuteResponseMessage &response_message) override {
    exec_calls++;
    last_sender = sender;
    last_target = target;
    last_exec = request_message;
    if (!exec_result) return false;
    PB::Commands::ExecuteResponseMessage::Response *payload = response_message.add_payload();
    payload->set_command("handled");
    payload->set_result(PB::Common::ResultCode::OK);
    payload->set_message("from the handler");
    return true;
  }

  bool metrics(client::destination_container sender, client::destination_container target, const PB::Metrics::MetricsMessage &request_message) override {
    metrics_calls++;
    last_sender = sender;
    last_target = target;
    return metrics_result;
  }
};

// The reader is where a module adds its own options (--password, --ssl, ...).
// Nothing here needs module-specific options, so it adds none.
struct null_reader : client::options_reader_interface {
  void process(po::options_description &, client::destination_container &, client::destination_container &) override {}
  object_instance create(std::string alias, std::string path) override {
    return std::make_shared<nscapi::settings_objects::object_instance_interface>(alias, path);
  }
  object_instance clone(object_instance parent, std::string alias, std::string path) override {
    return std::make_shared<nscapi::settings_objects::object_instance_interface>(parent, alias, path);
  }
};

// A configuration wired to the two fakes above, plus the handles a test needs
// to poke at them.
struct fixture {
  std::shared_ptr<recording_handler> handler = std::make_shared<recording_handler>();
  std::shared_ptr<null_reader> reader = std::make_shared<null_reader>();
  client::configuration config{"test client", handler, reader};

  fixture() { config.set_path("/settings/test/targets"); }

  // Install a target object the way reading settings would have.
  void add_target(const std::string &alias, const std::map<std::string, std::string> &options) {
    auto obj = std::make_shared<nscapi::settings_objects::object_instance_interface>(alias, "/settings/test/targets");
    for (const auto &o : options) obj->set_property_string(o.first, o.second);
    config.targets.objects[alias] = obj;
  }

  // Build a one-payload query request for `command` with `arguments`.
  static PB::Commands::QueryRequestMessage query_request(const std::string &command, const std::vector<std::string> &arguments = {},
                                                         const std::string &recipient = "") {
    PB::Commands::QueryRequestMessage request;
    if (!recipient.empty()) request.mutable_header()->set_recipient_id(recipient);
    PB::Commands::QueryRequestMessage::Request *payload = request.add_payload();
    payload->set_command(command);
    for (const std::string &a : arguments) payload->add_arguments(a);
    return request;
  }
};

// First line of the first payload - where both the handler's answer and any
// error message end up.
std::string first_message(const PB::Commands::QueryResponseMessage &response) {
  if (response.payload_size() == 0) return "<no payload>";
  const PB::Commands::QueryResponseMessage::Response &payload = response.payload(0);
  if (payload.lines_size() == 0) return "<no lines>";
  return payload.lines(0).message();
}

}  // namespace

// ---------------------------------------------------------------------------
// destination_container - the bag of connection settings every handler reads.
// Its setters translate a handful of well-known keys into structured fields
// and let everything else through as free-form data.
// ---------------------------------------------------------------------------

TEST(destination_container, well_known_keys_become_structured_fields) {
  client::destination_container d;
  d.set_string_data("host", "server.example.com");
  d.set_string_data("port", "5666");
  d.set_string_data("timeout", "42");
  d.set_string_data("retry", "7");

  EXPECT_EQ(d.get_host(), "server.example.com");
  EXPECT_EQ(d.address.port, 5666u);
  EXPECT_EQ(d.timeout, 42);
  EXPECT_EQ(d.retry, 7);
  EXPECT_FALSE(d.has_data("host")) << "translated, not stored as free-form data";
}

TEST(destination_container, unknown_keys_are_kept_as_data) {
  client::destination_container d;
  d.set_string_data("password", "secret");

  EXPECT_TRUE(d.has_data("password"));
  EXPECT_EQ(d.get_string_data("password"), "secret");
  EXPECT_EQ(d.get_string_data("missing", "fallback"), "fallback");
}

TEST(destination_container, address_sets_protocol_host_and_port_at_once) {
  client::destination_container d;
  d.set_string_data("address", "nrpe://server.example.com:5667");

  EXPECT_TRUE(d.has_protocol());
  EXPECT_EQ(d.get_protocol(), "nrpe");
  EXPECT_EQ(d.get_host(), "server.example.com");
  EXPECT_EQ(d.address.port, 5667u);
}

TEST(destination_container, garbage_numbers_fall_back_to_the_previous_value) {
  // to_int swallows the parse error: a bad timeout in a config file must not
  // take the whole client down.
  client::destination_container d;
  d.set_string_data("timeout", "not-a-number");
  EXPECT_EQ(d.timeout, 10) << "the default is kept";

  d.set_string_data("timeout", "");
  EXPECT_EQ(d.timeout, 10) << "and so is an empty value";
}

TEST(destination_container, booleans_only_accept_the_documented_spellings) {
  EXPECT_TRUE(client::destination_container::to_bool("true"));
  EXPECT_TRUE(client::destination_container::to_bool("True"));
  EXPECT_TRUE(client::destination_container::to_bool("1"));
  EXPECT_FALSE(client::destination_container::to_bool("yes")) << "not a documented spelling";
  EXPECT_FALSE(client::destination_container::to_bool("false"));
  EXPECT_TRUE(client::destination_container::to_bool("", true)) << "empty falls back to the default";
}

TEST(destination_container, a_host_from_the_request_header_is_applied) {
  PB::Common::Host host;
  host.set_id("primary");
  host.set_address("nrpe://from-header:1234");
  PB::Common::KeyValue *kvp = host.add_metadata();
  kvp->set_key("password");
  kvp->set_value("from-header");

  client::destination_container d;
  d.apply_host(host);

  EXPECT_EQ(d.get_host(), "from-header");
  EXPECT_EQ(d.address.port, 1234u);
  EXPECT_EQ(d.get_string_data("password"), "from-header");
}

// ---------------------------------------------------------------------------
// Command aliases. `nscp settings --set /settings/NRPE/client/targets ...`
// style configuration registers an alias plus a fixed argument list.
// ---------------------------------------------------------------------------

TEST(client_configuration, add_command_splits_the_command_from_its_arguments) {
  fixture f;

  const std::string key = f.config.add_command("check_alias", "check_cpu warn=10 crit=20");

  EXPECT_EQ(key, "check_alias");
  ASSERT_EQ(f.config.commands.count("check_alias"), 1u);
  EXPECT_EQ(f.config.commands["check_alias"].command, "check_cpu");
  EXPECT_EQ(f.config.commands["check_alias"].arguments.size(), 2u);
}

TEST(client_configuration, add_command_lower_cases_the_alias) {
  // Command lookup is done on the lower-cased name, so the key has to match.
  fixture f;

  const std::string key = f.config.add_command("CHECK_Alias", "check_cpu");

  EXPECT_EQ(key, "check_alias");
  EXPECT_EQ(f.config.commands.count("check_alias"), 1u);
}

TEST(client_configuration, add_command_keeps_quoted_arguments_together) {
  fixture f;

  f.config.add_command("check_alias", "check_cpu \"filter=core = 'total'\"");

  ASSERT_EQ(f.config.commands["check_alias"].arguments.size(), 1u);
  EXPECT_EQ(f.config.commands["check_alias"].arguments.front(), "filter=core = 'total'");
}

// ---------------------------------------------------------------------------
// Target selection.
// ---------------------------------------------------------------------------

TEST(client_configuration, get_target_uses_the_named_target) {
  fixture f;
  f.add_target("primary", {{"address", "nrpe://primary.example.com:5666"}, {"password", "p"}});

  const client::destination_container d = f.config.get_target("primary");

  EXPECT_EQ(d.get_host(), "primary.example.com");
}

TEST(client_configuration, get_target_falls_back_to_default) {
  // An unknown target is not an error: the default target's settings apply.
  fixture f;
  f.add_target("default", {{"address", "nrpe://fallback.example.com:5666"}});

  const client::destination_container d = f.config.get_target("nobody-configured-this");

  EXPECT_EQ(d.get_host(), "fallback.example.com");
}

TEST(client_configuration, get_target_without_any_configuration_is_empty) {
  fixture f;

  const client::destination_container d = f.config.get_target("anything");

  EXPECT_TRUE(d.get_host().empty());
}

TEST(client_configuration, get_sender_comes_from_the_configured_sender) {
  fixture f;
  f.config.set_sender("nsca://sender.example.com:5667");

  const client::destination_container s = f.config.get_sender();

  EXPECT_EQ(s.get_host(), "sender.example.com");
  EXPECT_EQ(s.address.port, 5667u);
}

// ---------------------------------------------------------------------------
// do_query: which handler call a command shape maps to.
// ---------------------------------------------------------------------------

TEST(client_query, a_check_command_is_sent_to_the_handler) {
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu", {"--command", "check_cpu"}), response);

  EXPECT_EQ(f.handler->query_calls, 1);
  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(first_message(response), "from the handler");
}

TEST(client_query, the_command_and_arguments_reach_the_handler) {
  // The payload the handler receives is rebuilt from the request's own
  // arguments (--command / --argument), not forwarded verbatim.
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_remote", {"--command", "check_cpu", "--argument", "warn=10"}), response);

  ASSERT_EQ(f.handler->last_query.payload_size(), 1);
  EXPECT_EQ(f.handler->last_query.payload(0).command(), "check_cpu");
  ASSERT_EQ(f.handler->last_query.payload(0).arguments_size(), 1);
  EXPECT_EQ(f.handler->last_query.payload(0).arguments(0), "warn=10");
}

TEST(client_query, a_failing_handler_is_reported_as_failed) {
  fixture f;
  f.handler->query_result = false;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu"), response);

  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(first_message(response), "check_cpu failed");
}

TEST(client_query, an_unrecognised_command_shape_is_reported_as_not_found) {
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("wibble_wobble"), response);

  EXPECT_EQ(f.handler->query_calls, 0);
  EXPECT_EQ(first_message(response), "wibble_wobble not found");
}

TEST(client_query, a_short_command_name_is_reported_as_not_found) {
  // Regression: the prefix/suffix tests used to index from command.size() - 8,
  // which underflows for anything shorter than the suffix. Every command under
  // eight characters came back as "Exception processing command line:
  // basic_string::substr" instead of an answer.
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("cpu"), response);

  EXPECT_EQ(first_message(response), "cpu not found");
}

TEST(client_query, a_short_check_command_still_reaches_the_handler) {
  // The other half of the same defect: "check_" is six characters, so a query
  // named check_x is long enough for the prefix test but too short for the
  // "_forward" suffix test that runs first.
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_x"), response);

  EXPECT_EQ(f.handler->query_calls, 1);
  EXPECT_EQ(first_message(response), "from the handler");
}

TEST(client_query, a_query_suffix_is_recognised_as_well_as_the_check_prefix) {
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("cpu_query"), response);

  EXPECT_EQ(f.handler->query_calls, 1);
}

TEST(client_query, a_forward_command_hands_the_request_over_untouched) {
  // Forwarding is the pass-through mode: the request goes to the remote
  // system as-is rather than being rebuilt from parsed options.
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("forward_check", {"whatever=1"}), response);

  ASSERT_EQ(f.handler->query_calls, 1);
  ASSERT_EQ(f.handler->last_query.payload_size(), 1);
  EXPECT_EQ(f.handler->last_query.payload(0).command(), "forward_check");
  ASSERT_EQ(f.handler->last_query.payload(0).arguments_size(), 1);
  EXPECT_EQ(f.handler->last_query.payload(0).arguments(0), "whatever=1");
}

TEST(client_query, a_forward_suffix_forwards_too) {
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_forward", {"whatever=1"}), response);

  ASSERT_EQ(f.handler->query_calls, 1);
  EXPECT_EQ(f.handler->last_query.payload(0).command(), "check_forward");
}

TEST(client_query, help_pb_on_a_forward_command_is_answered_locally) {
  // A forwarding command cannot describe its arguments, so it answers the
  // introspection request itself instead of asking the remote system.
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("forward_check", {"help-pb"}), response);

  EXPECT_EQ(f.handler->query_calls, 0) << "answered without going to the handler";
  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_FALSE(response.payload(0).data().empty());
}

TEST(client_query, an_alias_is_expanded_to_the_command_it_names) {
  fixture f;
  f.config.add_command("my_alias", "forward_check");
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("my_alias"), response);

  EXPECT_EQ(f.handler->query_calls, 1) << "the alias resolved to a forwarding command";
}

TEST(client_query, each_recipient_in_a_list_gets_its_own_call) {
  fixture f;
  f.add_target("first", {{"address", "nrpe://first.example.com:5666"}});
  f.add_target("second", {{"address", "nrpe://second.example.com:5666"}});
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu", {}, "first,second"), response);

  EXPECT_EQ(f.handler->query_calls, 2);
  EXPECT_EQ(response.payload_size(), 2);
}

TEST(client_query, the_target_settings_reach_the_handler) {
  fixture f;
  f.add_target("primary", {{"address", "nrpe://primary.example.com:5666"}, {"password", "secret"}});
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu", {}, "primary"), response);

  EXPECT_EQ(f.handler->last_target.get_host(), "primary.example.com");
  EXPECT_EQ(f.handler->last_target.get_string_data("password"), "secret");
}

TEST(client_query, command_line_options_override_the_target) {
  // -H on the command line beats whatever the configured target says.
  fixture f;
  f.add_target("default", {{"address", "nrpe://configured.example.com:5666"}});
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu", {"--host", "override.example.com", "--timeout", "99"}), response);

  EXPECT_EQ(f.handler->last_target.get_host(), "override.example.com");
  EXPECT_EQ(f.handler->last_target.timeout, 99);
}

TEST(client_query, an_unknown_option_is_reported_rather_than_sent) {
  fixture f;
  PB::Commands::QueryResponseMessage response;

  f.config.do_query(fixture::query_request("check_cpu", {"--no-such-option"}), response);

  EXPECT_EQ(f.handler->query_calls, 0);
  ASSERT_EQ(response.payload_size(), 1);
  EXPECT_NE(response.payload(0).result(), PB::Common::ResultCode::OK);
}

// ---------------------------------------------------------------------------
// do_exec / do_submit / do_metrics.
// ---------------------------------------------------------------------------

TEST(client_exec, an_exec_command_is_sent_to_the_handler) {
  fixture f;
  PB::Commands::ExecuteRequestMessage request;
  PB::Commands::ExecuteRequestMessage::Request *payload = request.add_payload();
  payload->set_command("exec_something");
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_TRUE(f.config.do_exec(request, response, ""));

  EXPECT_EQ(f.handler->exec_calls, 1);
}

TEST(client_exec, an_unknown_exec_command_does_not_reach_the_handler) {
  fixture f;
  PB::Commands::ExecuteRequestMessage request;
  request.add_payload()->set_command("wibble_wobble");
  PB::Commands::ExecuteResponseMessage response;

  f.config.do_exec(request, response, "");

  EXPECT_EQ(f.handler->exec_calls, 0);
}

TEST(client_exec, a_short_exec_command_does_not_blow_up) {
  // Same underflow as the query path: "_submit" is seven characters, so an
  // exec command shorter than that used to answer with the substr exception
  // rather than saying it did not recognise the command.
  fixture f;
  PB::Commands::ExecuteRequestMessage request;
  request.add_payload()->set_command("run");
  PB::Commands::ExecuteResponseMessage response;

  EXPECT_FALSE(f.config.do_exec(request, response, ""));

  EXPECT_EQ(f.handler->exec_calls, 0);
  ASSERT_GE(response.payload_size(), 1);
  EXPECT_EQ(response.payload(0).message(), "Module does not know of any command called: run");
}

TEST(client_submit, a_submit_command_is_sent_to_the_handler) {
  fixture f;
  PB::Commands::SubmitRequestMessage request;
  request.mutable_header()->set_command("submit_result");
  PB::Commands::QueryResponseMessage::Response *payload = request.add_payload();
  payload->set_command("check_cpu");
  payload->set_result(PB::Common::ResultCode::OK);
  PB::Commands::SubmitResponseMessage response;

  f.config.do_submit(request, response);

  EXPECT_EQ(f.handler->submit_calls, 1);
}

TEST(client_metrics, metrics_are_sent_to_every_configured_target) {
  fixture f;
  f.add_target("default", {{"address", "nsca://metrics.example.com:5667"}});
  PB::Metrics::MetricsMessage request;
  request.add_payload();

  f.config.do_metrics(request);

  EXPECT_EQ(f.handler->metrics_calls, 1);
  EXPECT_EQ(f.handler->last_target.get_host(), "metrics.example.com");
}
