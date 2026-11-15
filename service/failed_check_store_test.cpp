// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "failed_check_store.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using failed_check_store = nsclient::core::failed_check_store;

namespace {

PB::Commands::QueryResponseMessage::Response make_response(const std::string &command, const PB::Common::ResultCode result, const std::string &message,
                                                           const std::string &alias = "") {
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command(command);
  if (!alias.empty()) response.set_alias(alias);
  response.set_result(result);
  PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();
  line->set_message(message);
  PB::Common::PerformanceData *perf = line->add_perf();
  perf->set_alias("load");
  perf->mutable_float_value()->set_value(42.5);
  perf->mutable_float_value()->set_unit("%");
  return response;
}

PB::Commands::QueryRequestMessage::Request *add_request(PB::Commands::QueryRequestMessage &request, const std::string &command,
                                                        const std::vector<std::string> &arguments) {
  PB::Commands::QueryRequestMessage::Request *payload = request.add_payload();
  payload->set_command(command);
  for (const std::string &argument : arguments) payload->add_arguments(argument);
  return payload;
}

std::vector<std::string> arguments_of(const PB::Failures::FailedCheck &entry) {
  return std::vector<std::string>(entry.response().arguments().begin(), entry.response().arguments().end());
}

failed_check_store::options with_arguments(const failed_check_store::argument_mode mode = failed_check_store::argument_mode::all) {
  failed_check_store::options o;
  o.record_arguments = mode;
  return o;
}

std::vector<std::string> safe_filtered(const std::vector<std::string> &in) {
  google::protobuf::RepeatedPtrField<std::string> arguments;
  for (const std::string &a : in) arguments.Add()->assign(a);
  failed_check_store::filter_safe_arguments(&arguments);
  return std::vector<std::string>(arguments.begin(), arguments.end());
}

}  // namespace

TEST(failed_check_store, records_non_ok_results_only_by_default) {
  failed_check_store store;
  EXPECT_FALSE(store.record(make_response("check_ok", PB::Common::ResultCode::OK, "fine"), nullptr, "NRPEServer", 0, 100));
  EXPECT_TRUE(store.record(make_response("check_warning", PB::Common::ResultCode::WARNING, "meh"), nullptr, "NRPEServer", 0, 101));
  EXPECT_TRUE(store.record(make_response("check_critical", PB::Common::ResultCode::CRITICAL, "bad"), nullptr, "NRPEServer", 0, 102));
  EXPECT_TRUE(store.record(make_response("check_unknown", PB::Common::ResultCode::UNKNOWN, "eh"), nullptr, "NRPEServer", 0, 103));
  EXPECT_EQ(3u, store.size());
  EXPECT_EQ(3ull, store.last_index());
}

TEST(failed_check_store, lists_newest_first_with_index_timestamp_and_payload) {
  failed_check_store store;
  store.record(make_response("check_a", PB::Common::ResultCode::WARNING, "first"), nullptr, "Scheduler", 0, 1000);
  store.record(make_response("check_b", PB::Common::ResultCode::CRITICAL, "second", "disk"), nullptr, "WEBServer:admin", 1, 1001);

  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(2ull, entries[0].index());
  EXPECT_EQ(1001, entries[0].timestamp());
  EXPECT_EQ("WEBServer:admin", entries[0].subject());
  EXPECT_EQ(1, entries[0].depth());
  EXPECT_EQ("check_b", entries[0].response().command());
  EXPECT_EQ("disk", entries[0].response().alias());
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, entries[0].response().result());
  ASSERT_EQ(1, entries[0].response().lines_size());
  EXPECT_EQ("second", entries[0].response().lines(0).message());
  // Perf data travels structured, not flattened to a string.
  ASSERT_EQ(1, entries[0].response().lines(0).perf_size());
  EXPECT_EQ("load", entries[0].response().lines(0).perf(0).alias());
  EXPECT_DOUBLE_EQ(42.5, entries[0].response().lines(0).perf(0).float_value().value());
  EXPECT_EQ("%", entries[0].response().lines(0).perf(0).float_value().unit());

  EXPECT_EQ(1ull, entries[1].index());
  EXPECT_EQ("check_a", entries[1].response().command());
}

TEST(failed_check_store, since_and_limit_select_a_window) {
  failed_check_store store;
  for (int i = 0; i < 10; ++i) store.record(make_response("check_" + std::to_string(i), PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, i);

  failed_check_store::entry_list entries = store.list(7);
  ASSERT_EQ(3u, entries.size());
  EXPECT_EQ(10ull, entries[0].index());
  EXPECT_EQ(8ull, entries[2].index());

  entries = store.list(0, 2);
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(10ull, entries[0].index());
  EXPECT_EQ(9ull, entries[1].index());

  // Nothing new since the last index seen.
  EXPECT_TRUE(store.list(10).empty());
}

TEST(failed_check_store, cap_evicts_the_oldest_and_counts_the_drops) {
  failed_check_store store;
  failed_check_store::options o;
  o.max_entries = 3;
  store.configure(o);
  for (int i = 0; i < 5; ++i) store.record(make_response("check_" + std::to_string(i), PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, i);

  EXPECT_EQ(3u, store.size());
  EXPECT_EQ(2ull, store.dropped());
  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(3u, entries.size());
  EXPECT_EQ("check_4", entries[0].response().command());
  EXPECT_EQ("check_2", entries[2].response().command());
  // The index keeps counting past evicted entries.
  EXPECT_EQ(5ull, store.last_index());
}

TEST(failed_check_store, lowering_the_cap_trims_and_zero_is_clamped_to_one) {
  failed_check_store store;
  for (int i = 0; i < 5; ++i) store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, i);
  failed_check_store::options o;
  o.max_entries = 2;
  store.configure(o);
  EXPECT_EQ(2u, store.size());
  EXPECT_EQ(3ull, store.dropped());

  o.max_entries = 0;
  store.configure(o);
  EXPECT_EQ(1u, store.size());
  EXPECT_EQ(1u, store.get_options().max_entries);

  o.max_entries = failed_check_store::max_max_entries + 1;
  store.configure(o);
  EXPECT_EQ(failed_check_store::max_max_entries, store.get_options().max_entries);
}

TEST(failed_check_store, disabling_clears_and_stops_recording) {
  failed_check_store store;
  store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 1);
  failed_check_store::options o;
  o.enabled = false;
  store.configure(o);
  EXPECT_EQ(0u, store.size());
  EXPECT_FALSE(store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 2));
  EXPECT_FALSE(store.records(PB::Common::ResultCode::CRITICAL));
  // Not a drop: the operator asked for it.
  EXPECT_EQ(0ull, store.dropped());

  o.enabled = true;
  store.configure(o);
  EXPECT_TRUE(store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 3));
  // The index did not restart.
  EXPECT_EQ(2ull, store.list()[0].index());
}

TEST(failed_check_store, status_filter_narrows_what_is_recorded) {
  failed_check_store store;
  failed_check_store::options o;
  o.statuses = failed_check_store::status_bit(PB::Common::ResultCode::CRITICAL);
  store.configure(o);
  EXPECT_FALSE(store.record(make_response("w", PB::Common::ResultCode::WARNING, "x"), nullptr, "s", 0, 1));
  EXPECT_TRUE(store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 2));
  EXPECT_FALSE(store.record(make_response("u", PB::Common::ResultCode::UNKNOWN, "x"), nullptr, "s", 0, 3));
  EXPECT_TRUE(store.records(PB::Common::ResultCode::CRITICAL));
  EXPECT_FALSE(store.records(PB::Common::ResultCode::WARNING));

  // OK can never be configured in: the mask is clamped to the three failures.
  o.statuses = failed_check_store::status_bit(PB::Common::ResultCode::OK) | failed_check_store::status_bit(PB::Common::ResultCode::WARNING);
  store.configure(o);
  EXPECT_FALSE(store.record(make_response("ok", PB::Common::ResultCode::OK, "x"), nullptr, "s", 0, 4));
  EXPECT_TRUE(store.record(make_response("w", PB::Common::ResultCode::WARNING, "x"), nullptr, "s", 0, 5));
}

TEST(failed_check_store, parse_statuses_accepts_words_aliases_and_numbers) {
  unsigned mask = 0;
  std::string error;
  ASSERT_TRUE(failed_check_store::parse_statuses("warning,critical,unknown", mask, error)) << error;
  EXPECT_EQ(failed_check_store::default_statuses, mask);
  EXPECT_EQ("warning,critical,unknown", failed_check_store::format_statuses(mask));

  ASSERT_TRUE(failed_check_store::parse_statuses(" Crit , 3 ", mask, error)) << error;
  EXPECT_EQ(failed_check_store::status_bit(PB::Common::ResultCode::CRITICAL) | failed_check_store::status_bit(PB::Common::ResultCode::UNKNOWN), mask);
  EXPECT_EQ("critical,unknown", failed_check_store::format_statuses(mask));

  ASSERT_TRUE(failed_check_store::parse_statuses("warn", mask, error)) << error;
  EXPECT_EQ("warning", failed_check_store::format_statuses(mask));
}

TEST(failed_check_store, parse_statuses_rejects_ok_junk_and_empty) {
  unsigned mask = 0;
  std::string error;
  EXPECT_FALSE(failed_check_store::parse_statuses("ok,critical", mask, error));
  EXPECT_NE(std::string::npos, error.find("'ok'"));
  EXPECT_FALSE(failed_check_store::parse_statuses("critical,bogus", mask, error));
  EXPECT_NE(std::string::npos, error.find("'bogus'"));
  EXPECT_FALSE(failed_check_store::parse_statuses("", mask, error));
  EXPECT_FALSE(failed_check_store::parse_statuses(" , ", mask, error));
}

TEST(failed_check_store, arguments_are_dropped_when_the_mode_is_none) {
  PB::Commands::QueryRequestMessage request;
  add_request(request, "check_sql", {"query=select 1", "password=hunter2"});
  PB::Commands::QueryResponseMessage response;
  *response.add_payload() = make_response("check_sql", PB::Common::ResultCode::CRITICAL, "connection refused");
  // A module echoing the arguments in its payload must not leak them either.
  response.mutable_payload(0)->add_arguments("password=hunter2");

  failed_check_store store;
  store.configure(with_arguments(failed_check_store::argument_mode::none));
  EXPECT_EQ(1u, store.record_message(request, response, "NRPEServer", 0, 1));
  EXPECT_TRUE(arguments_of(store.list()[0]).empty());

  store.configure(with_arguments());
  EXPECT_EQ(1u, store.record_message(request, response, "NRPEServer", 0, 2));
  const std::vector<std::string> arguments = arguments_of(store.list()[0]);
  ASSERT_EQ(2u, arguments.size());
  EXPECT_EQ("query=select 1", arguments[0]);
  EXPECT_EQ("password=***", arguments[1]);
}

TEST(failed_check_store, safe_is_the_default_mode) {
  EXPECT_EQ(failed_check_store::argument_mode::safe, failed_check_store::options().record_arguments);
  EXPECT_EQ(failed_check_store::argument_mode::safe, failed_check_store().get_options().record_arguments);
}

TEST(failed_check_store, safe_keeps_only_allow_listed_rest_pairs) {
  PB::Commands::QueryRequestMessage request;
  add_request(request, "check_sql", {"query=select 1", "password=hunter2", "warning=time > 5", "critical=time > 10", "host=db1", "--top-syntax=${list}"});
  PB::Commands::QueryResponseMessage response;
  *response.add_payload() = make_response("check_sql", PB::Common::ResultCode::CRITICAL, "slow");

  failed_check_store store;
  store.configure(with_arguments(failed_check_store::argument_mode::safe));
  EXPECT_EQ(1u, store.record_message(request, response, "WEBServer:admin", 0, 1));
  EXPECT_EQ((std::vector<std::string>{"warning=time > 5", "critical=time > 10", "--top-syntax=${list}"}), arguments_of(store.list()[0]));
}

TEST(failed_check_store, safe_keeps_the_arguments_the_estate_faults_role_uses) {
  // The checks the `faults` role provokes (build/powershell/estate/roles/faults)
  // must reach the log with their arguments under the default mode.
  EXPECT_EQ((std::vector<std::string>{"time=1m", "warning=load > 70", "critical=load > 85"}),
            safe_filtered({"time=1m", "warning=load > 70", "critical=load > 85"}));
  EXPECT_EQ((std::vector<std::string>{"type=physical", "warning=used > 80%"}), safe_filtered({"type=physical", "warning=used > 80%"}));
  EXPECT_EQ((std::vector<std::string>{"drive=C:", "critical=used > 90%"}), safe_filtered({"drive=C:", "critical=used > 90%"}));
  EXPECT_EQ((std::vector<std::string>{"service=acme-sync", "critical=state != 'running'"}),
            safe_filtered({"service=acme-sync", "critical=state != 'running'"}));
}

TEST(failed_check_store, safe_handles_the_cli_shape) {
  // nscp client -a key=value passes --key then value.
  EXPECT_EQ((std::vector<std::string>{"--warning", "load > 70", "--drive", "C:"}),
            safe_filtered({"--warning", "load > 70", "--password", "hunter2", "--drive", "C:", "--host", "db1"}));
  // A value containing an `=` stays a value.
  EXPECT_EQ((std::vector<std::string>{"--filter", "name = 'x'", "--critical", "load>=80"}), safe_filtered({"--filter", "name = 'x'", "--critical", "load>=80"}));
  // The value of a dropped key is dropped whatever it looks like.
  EXPECT_EQ((std::vector<std::string>{"--time", "1m"}), safe_filtered({"--password", "warning=x", "--time", "1m"}));
  // Undashed, as --raw-argument passes it.
  EXPECT_EQ((std::vector<std::string>{"warning", "used > 80%"}), safe_filtered({"password", "hunter2", "warning", "used > 80%"}));
}

TEST(failed_check_store, safe_flags_do_not_swallow_the_next_argument) {
  // A flag with no value followed by a dashed option or a REST pair: the
  // pair is judged on its own key, never kept as the flag's value.
  EXPECT_EQ((std::vector<std::string>{"--show-all", "--time", "1m"}), safe_filtered({"--show-all", "--time", "1m"}));
  EXPECT_EQ((std::vector<std::string>{"show-all"}), safe_filtered({"show-all", "password=hunter2"}));
  EXPECT_EQ((std::vector<std::string>{"show-all"}), safe_filtered({"show-all", "secret=hunter2"}));
  EXPECT_EQ((std::vector<std::string>{"show-all", "warning=x"}), safe_filtered({"show-all", "warning=x"}));
}

TEST(failed_check_store, safe_drops_positionals_and_credential_keys) {
  EXPECT_EQ(std::vector<std::string>(), safe_filtered({"select * from users", "hunter2"}));
  EXPECT_EQ(std::vector<std::string>(), safe_filtered({"proxy-password=x", "--api-token", "abc", "token=x"}));
  EXPECT_EQ(std::vector<std::string>(), safe_filtered({"=x", "-", "--"}));
  EXPECT_TRUE(failed_check_store::is_safe_argument("--WARNING"));
  EXPECT_TRUE(failed_check_store::is_safe_argument("detail-syntax"));
  EXPECT_FALSE(failed_check_store::is_safe_argument("host"));
  EXPECT_FALSE(failed_check_store::is_safe_argument("command"));
  EXPECT_FALSE(failed_check_store::is_safe_argument(""));
  for (const std::string &key : failed_check_store::safe_argument_keys()) EXPECT_FALSE(failed_check_store::is_sensitive_argument(key)) << key;
}

TEST(failed_check_store, safe_applies_before_the_argument_cap) {
  google::protobuf::RepeatedPtrField<std::string> arguments;
  for (std::size_t i = 0; i < failed_check_store::max_arguments + 5; ++i) arguments.Add()->assign("noise=" + std::to_string(i));
  arguments.Add()->assign("warning=load > 70");
  failed_check_store store;
  store.configure(with_arguments(failed_check_store::argument_mode::safe));
  ASSERT_TRUE(store.record(make_response("check_cpu", PB::Common::ResultCode::WARNING, "x"), &arguments, "s", 0, 1));
  EXPECT_EQ((std::vector<std::string>{"warning=load > 70"}), arguments_of(store.list()[0]));
}

TEST(failed_check_store, parse_argument_mode_takes_modes_and_the_old_booleans) {
  failed_check_store::argument_mode mode = failed_check_store::argument_mode::safe;
  std::string error;
  ASSERT_TRUE(failed_check_store::parse_argument_mode(" ALL ", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::all, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("safe", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::safe, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("none", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::none, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("true", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::all, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("1", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::all, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("False", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::none, mode);
  ASSERT_TRUE(failed_check_store::parse_argument_mode("0", mode, error));
  EXPECT_EQ(failed_check_store::argument_mode::none, mode);
  mode = failed_check_store::argument_mode::safe;
  EXPECT_FALSE(failed_check_store::parse_argument_mode("some", mode, error));
  EXPECT_NE(std::string::npos, error.find("some"));
  EXPECT_EQ(failed_check_store::argument_mode::safe, mode);
  EXPECT_EQ("none", failed_check_store::format_argument_mode(failed_check_store::argument_mode::none));
  EXPECT_EQ("safe", failed_check_store::format_argument_mode(failed_check_store::argument_mode::safe));
  EXPECT_EQ("all", failed_check_store::format_argument_mode(failed_check_store::argument_mode::all));
}

TEST(failed_check_store, redaction_covers_dashed_keys_and_the_cli_pair_shape) {
  google::protobuf::RepeatedPtrField<std::string> arguments;
  // REST passes key=value; the CLI passes --key value and the undashed pair.
  for (const char *argument : {"host=db1", "--password", "hunter2", "password", "hunter2", "--api-token=abc", "Password=Secret", "--port", "1433", "verbose"}) {
    arguments.Add(argument);
  }
  failed_check_store::redact_arguments(&arguments);
  const std::vector<std::string> got(arguments.begin(), arguments.end());
  const std::vector<std::string> want = {"host=db1", "--password", "***", "password", "***", "--api-token=***", "Password=***", "--port", "1433", "verbose"};
  EXPECT_EQ(want, got);
}

TEST(failed_check_store, is_sensitive_argument) {
  EXPECT_TRUE(failed_check_store::is_sensitive_argument("password"));
  EXPECT_TRUE(failed_check_store::is_sensitive_argument("--PASSWORD"));
  EXPECT_TRUE(failed_check_store::is_sensitive_argument("api-token"));
  EXPECT_TRUE(failed_check_store::is_sensitive_argument("proxy-password"));
  EXPECT_FALSE(failed_check_store::is_sensitive_argument("host"));
  EXPECT_FALSE(failed_check_store::is_sensitive_argument("--"));
  EXPECT_FALSE(failed_check_store::is_sensitive_argument(""));
}

TEST(failed_check_store, record_message_pairs_arguments_by_command_across_reordered_payloads) {
  PB::Commands::QueryRequestMessage request;
  add_request(request, "check_a", {"token=aaa", "x=1"});
  add_request(request, "check_b", {"x=2"});
  add_request(request, "check_a", {"x=3"});
  PB::Commands::QueryResponseMessage response;
  // Answered in module-chunk order, not request order, and one of them OK.
  *response.add_payload() = make_response("CHECK_B", PB::Common::ResultCode::OK, "fine");
  *response.add_payload() = make_response("check_a", PB::Common::ResultCode::WARNING, "first a");
  *response.add_payload() = make_response("check_a", PB::Common::ResultCode::CRITICAL, "second a");
  *response.add_payload() = make_response("check_c", PB::Common::ResultCode::UNKNOWN, "no request payload at all");

  failed_check_store store;
  store.configure(with_arguments());
  EXPECT_EQ(3u, store.record_message(request, response, "s", 0, 1));
  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(3u, entries.size());
  EXPECT_EQ("check_c", entries[0].response().command());
  EXPECT_TRUE(arguments_of(entries[0]).empty());
  EXPECT_EQ((std::vector<std::string>{"x=3"}), arguments_of(entries[1]));
  EXPECT_EQ((std::vector<std::string>{"token=***", "x=1"}), arguments_of(entries[2]));
}

TEST(failed_check_store, caps_lines_line_length_perf_and_arguments_and_drops_data) {
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command("check_big");
  response.set_result(PB::Common::ResultCode::CRITICAL);
  response.set_data(std::string(100000, 'd'));
  for (std::size_t i = 0; i < failed_check_store::max_lines + 5; ++i) {
    PB::Commands::QueryResponseMessage::Response::Line *line = response.add_lines();
    line->set_message(std::string(failed_check_store::max_line_length + 100, 'm'));
    for (std::size_t p = 0; p < failed_check_store::max_perf_per_line + 5; ++p) line->add_perf()->set_alias("p" + std::to_string(p));
  }
  google::protobuf::RepeatedPtrField<std::string> arguments;
  for (std::size_t i = 0; i < failed_check_store::max_arguments + 5; ++i) arguments.Add("a=" + std::string(failed_check_store::max_argument_length, 'v'));

  failed_check_store store;
  store.configure(with_arguments());
  ASSERT_TRUE(store.record(response, &arguments, "s", 0, 1));
  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(1u, entries.size());
  const PB::Commands::QueryResponseMessage::Response &stored = entries[0].response();
  EXPECT_TRUE(stored.data().empty());
  EXPECT_EQ(static_cast<int>(failed_check_store::max_lines), stored.lines_size());
  EXPECT_EQ(failed_check_store::max_line_length, stored.lines(0).message().size());
  EXPECT_EQ("...", stored.lines(0).message().substr(failed_check_store::max_line_length - 3));
  EXPECT_EQ(static_cast<int>(failed_check_store::max_perf_per_line), stored.lines(0).perf_size());
  EXPECT_EQ(static_cast<int>(failed_check_store::max_arguments), stored.arguments_size());
  EXPECT_EQ(failed_check_store::max_argument_length, stored.arguments(0).size());
  // The original is untouched: the store works on its own copy.
  EXPECT_EQ(100000u, response.data().size());
  EXPECT_EQ(static_cast<int>(failed_check_store::max_lines + 5), response.lines_size());
}

TEST(failed_check_store, clear_removes_everything_but_keeps_the_index) {
  failed_check_store store;
  store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 1);
  store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 2);
  EXPECT_EQ(2u, store.clear());
  EXPECT_EQ(0u, store.size());
  EXPECT_EQ(0u, store.clear());
  EXPECT_EQ(2ull, store.last_index());
  store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, "s", 0, 3);
  EXPECT_EQ(3ull, store.list()[0].index());
}

TEST(failed_check_store, negative_depth_and_long_subject_are_normalised) {
  failed_check_store store;
  store.record(make_response("c", PB::Common::ResultCode::CRITICAL, "x"), nullptr, std::string(failed_check_store::max_subject_length + 10, 's'), -4, 1);
  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(1u, entries.size());
  const PB::Failures::FailedCheck &entry = entries[0];
  EXPECT_EQ(0, entry.depth());
  EXPECT_EQ(failed_check_store::max_subject_length, entry.subject().size());
}

// The message is the check's text and stays as it was - except that a
// password=value or token=value pair inside it loses its value, because a
// check that rejects its options echoes them back and would otherwise put
// the secret in the log even with argument recording off.
TEST(failed_check_store, redacts_secrets_echoed_in_messages) {
  EXPECT_EQ("Invalid command line: unrecognised option 'password=***'",
            failed_check_store::redact_message("Invalid command line: unrecognised option 'password=hunter2'"));
  EXPECT_EQ("user=bob --api-token=*** host=x", failed_check_store::redact_message("user=bob --api-token=s3cr3t host=x"));
  EXPECT_EQ("Password=***, retry", failed_check_store::redact_message("Password=abc, retry"));
  // No value, no change; unrelated keys untouched; bare words untouched.
  EXPECT_EQ("password= token", failed_check_store::redact_message("password= token"));
  EXPECT_EQ("free=10% used=90%", failed_check_store::redact_message("free=10% used=90%"));
  EXPECT_EQ("the password is wrong", failed_check_store::redact_message("the password is wrong"));

  failed_check_store store;
  PB::Commands::QueryResponseMessage::Response response;
  response.set_command("check_x");
  response.set_result(PB::Common::ResultCode::UNKNOWN);
  response.add_lines()->set_message("unrecognised option 'password=hunter2'");
  ASSERT_TRUE(store.record(response, nullptr, "test", 0, 100));
  const failed_check_store::entry_list entries = store.list();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ("unrecognised option 'password=***'", entries[0].response().lines(0).message());
}

// --- Metrics context -------------------------------------------------------

namespace {
// A snapshot of roughly `padding` bytes on the wire, stored the way the core
// stores one after a metrics round.
nsclient::core::metrics_snapshot_ptr make_snapshot(nsclient::core::metrics_snapshot_store &snapshots, const std::size_t padding, const std::int64_t now) {
  PB::Metrics::MetricsMessage message;
  PB::Metrics::MetricsBundle *bundle = message.add_payload()->add_bundles();
  bundle->set_key("system");
  PB::Metrics::Metric *metric = bundle->add_value();
  metric->set_key("blob");
  metric->mutable_string_value()->set_value(std::string(padding, 'x'));
  return snapshots.store(message, now);
}

failed_check_store::options capturing(const std::size_t budget = failed_check_store::default_context_budget,
                                      const std::size_t max_size = failed_check_store::default_max_context_size) {
  failed_check_store::options o;
  o.capture_metrics = true;
  o.context_budget = budget;
  o.max_context_size = max_size;
  return o;
}

PB::Commands::QueryResponseMessage::Response critical(const std::string &command = "check_cpu") {
  return make_response(command, PB::Common::ResultCode::CRITICAL, "bad");
}
}  // namespace

TEST(metrics_snapshot_store, hands_out_increasing_ids_and_keeps_the_latest) {
  nsclient::core::metrics_snapshot_store snapshots;
  EXPECT_FALSE(snapshots.latest());
  const nsclient::core::metrics_snapshot_ptr first = make_snapshot(snapshots, 10, 100);
  const nsclient::core::metrics_snapshot_ptr second = make_snapshot(snapshots, 10, 110);
  EXPECT_EQ(1u, first->id);
  EXPECT_EQ(2u, second->id);
  EXPECT_EQ(110, second->captured_at);
  EXPECT_GT(second->wire_size, 10u);
  EXPECT_EQ(second, snapshots.latest());
  // A reader that held the first one still has it.
  EXPECT_EQ("system", first->message.payload(0).bundles(0).key());
  snapshots.reset();
  EXPECT_FALSE(snapshots.latest());
  EXPECT_EQ(3u, make_snapshot(snapshots, 10, 120)->id);
}

TEST(failed_check_store, capture_is_off_by_default_and_marks_entries_disabled) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  EXPECT_FALSE(store.captures_metrics());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, make_snapshot(snapshots, 100, 95)));
  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::DISABLED, found.item.context().state());
  EXPECT_FALSE(found.context);
  EXPECT_EQ(0u, store.context_bytes_used());
}

TEST(failed_check_store, capture_without_a_snapshot_yet_is_none) {
  failed_check_store store;
  store.configure(capturing());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100));
  EXPECT_EQ(PB::Failures::MetricsContext::NONE, store.list()[0].context().state());
}

TEST(failed_check_store, entries_in_one_interval_share_one_snapshot) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  const nsclient::core::metrics_snapshot_ptr snapshot = make_snapshot(snapshots, 1000, 95);
  ASSERT_TRUE(store.record(critical("check_cpu"), nullptr, "test", 0, 100, snapshot));
  ASSERT_TRUE(store.record(critical("check_drive_size"), nullptr, "test", 0, 101, snapshot));
  // Counted once, however many entries refer to it.
  EXPECT_EQ(snapshot->wire_size, store.context_bytes_used());

  failed_check_store::entry_with_context first, second;
  ASSERT_TRUE(store.get(1, first));
  ASSERT_TRUE(store.get(2, second));
  EXPECT_EQ(first.context, second.context);
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, first.item.context().state());
  EXPECT_EQ(snapshot->id, first.item.context().snapshot_id());
  EXPECT_EQ(95, first.item.context().captured_at());
  EXPECT_EQ(snapshot->wire_size, first.item.context().size());
  // The listing carries the reference, never the snapshot itself.
  EXPECT_FALSE(store.list()[0].context().has_metrics());
}

TEST(failed_check_store, budget_releases_the_oldest_snapshot_and_keeps_the_entries) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  // Room for two snapshots of ~40k, not three.
  store.configure(capturing(84 * 1024, 64 * 1024));
  const nsclient::core::metrics_snapshot_ptr a = make_snapshot(snapshots, 40 * 1024, 90);
  const nsclient::core::metrics_snapshot_ptr b = make_snapshot(snapshots, 40 * 1024, 100);
  const nsclient::core::metrics_snapshot_ptr c = make_snapshot(snapshots, 40 * 1024, 110);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, a));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 92, a));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, b));
  EXPECT_EQ(a->wire_size + b->wire_size, store.context_bytes_used());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 111, c));
  EXPECT_EQ(b->wire_size + c->wire_size, store.context_bytes_used());

  EXPECT_EQ(4u, store.size());
  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::EVICTED, found.item.context().state());
  EXPECT_FALSE(found.context);
  // The reference survives the eviction, so a reader can still say which
  // snapshot it was.
  EXPECT_EQ(a->id, found.item.context().snapshot_id());
  ASSERT_TRUE(store.get(2, found));
  EXPECT_EQ(PB::Failures::MetricsContext::EVICTED, found.item.context().state());
  ASSERT_TRUE(store.get(3, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());
  EXPECT_EQ(b, found.context);
}

TEST(failed_check_store, a_snapshot_over_the_size_cap_is_not_kept) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing(failed_check_store::default_context_budget, 1024));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, make_snapshot(snapshots, 4096, 95)));
  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::TOO_LARGE, found.item.context().state());
  EXPECT_GT(found.item.context().size(), 4096u);
  EXPECT_FALSE(found.context);
  EXPECT_EQ(0u, store.context_bytes_used());
}

TEST(failed_check_store, the_size_cap_never_exceeds_the_budget) {
  failed_check_store store;
  store.configure(capturing(failed_check_store::min_context_budget, failed_check_store::min_context_budget * 4));
  EXPECT_EQ(failed_check_store::min_context_budget, store.get_options().max_context_size);
  // And the budget has a floor.
  store.configure(capturing(1, 1));
  EXPECT_EQ(failed_check_store::min_context_budget, store.get_options().context_budget);
}

TEST(failed_check_store, evicting_entries_releases_their_snapshot) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  failed_check_store::options o = capturing();
  o.max_entries = 2;
  store.configure(o);
  const nsclient::core::metrics_snapshot_ptr a = make_snapshot(snapshots, 1000, 90);
  const nsclient::core::metrics_snapshot_ptr b = make_snapshot(snapshots, 1000, 100);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, a));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, b));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 102, b));
  // Entry 1 aged out, and with it the only reference to `a`.
  EXPECT_EQ(b->wire_size, store.context_bytes_used());
  EXPECT_EQ(2u, store.size());
}

TEST(failed_check_store, turning_capture_off_releases_what_is_held) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, make_snapshot(snapshots, 1000, 95)));
  store.configure(failed_check_store::options());
  EXPECT_EQ(0u, store.context_bytes_used());
  EXPECT_EQ(1u, store.size());
  EXPECT_EQ(PB::Failures::MetricsContext::EVICTED, store.list()[0].context().state());
}

TEST(failed_check_store, clear_releases_every_snapshot) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, make_snapshot(snapshots, 1000, 95)));
  EXPECT_EQ(1u, store.clear());
  EXPECT_EQ(0u, store.context_bytes_used());
  failed_check_store::entry_with_context found;
  EXPECT_FALSE(store.get(1, found));
}

TEST(failed_check_store, record_message_attaches_the_context_to_every_payload) {
  nsclient::core::metrics_snapshot_store snapshots;
  failed_check_store store;
  store.configure(capturing());
  PB::Commands::QueryRequestMessage request;
  add_request(request, "check_cpu", {});
  add_request(request, "check_memory", {});
  PB::Commands::QueryResponseMessage response;
  *response.add_payload() = critical("check_cpu");
  *response.add_payload() = critical("check_memory");
  const nsclient::core::metrics_snapshot_ptr snapshot = make_snapshot(snapshots, 100, 95);
  EXPECT_EQ(2u, store.record_message(request, response, "test", 0, 100, snapshot));
  for (const PB::Failures::FailedCheck &entry : store.list()) {
    EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, entry.context().state());
    EXPECT_EQ(snapshot->id, entry.context().snapshot_id());
  }
}

// --- Diagnostics context ---------------------------------------------------

namespace {
// A diagnostics snapshot of roughly `padding` bytes on the wire.
nsclient::core::diagnostics_snapshot_ptr make_diagnostics(nsclient::core::diagnostics_snapshot_store &snapshots, const std::size_t padding,
                                                          const std::int64_t now) {
  nsclient::core::diagnostics_snapshot s;
  s.captured_at = now;
  PB::Diagnostics::Section *section = s.message.add_sections();
  section->set_name("processes.top_cpu");
  section->add_columns()->set_name("name");
  section->add_rows()->add_cells()->set_s(std::string(padding, 'p'));
  return snapshots.store(s);
}

failed_check_store::options capturing_both(const std::size_t budget = failed_check_store::default_context_budget,
                                           const std::size_t max_size = failed_check_store::default_max_context_size) {
  failed_check_store::options o = capturing(budget, max_size);
  o.capture_diagnostics = true;
  return o;
}
}  // namespace

TEST(failed_check_store, diagnostics_capture_is_off_by_default_and_marks_entries_disabled) {
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  EXPECT_FALSE(store.captures_diagnostics());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, nullptr, make_diagnostics(diagnostics, 100, 95)));
  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::DiagnosticsContext::DISABLED, found.item.diagnostics().state());
  EXPECT_FALSE(found.diagnostics);
  EXPECT_EQ(0u, store.context_bytes_used());
}

TEST(failed_check_store, diagnostics_capture_without_a_round_yet_is_none) {
  failed_check_store store;
  store.configure(capturing_both());
  EXPECT_TRUE(store.captures_diagnostics());
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100));
  EXPECT_EQ(PB::Failures::DiagnosticsContext::NONE, store.list()[0].diagnostics().state());
}

TEST(failed_check_store, both_snapshots_are_shared_and_counted_once_each) {
  nsclient::core::metrics_snapshot_store metrics;
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  store.configure(capturing_both());
  const nsclient::core::metrics_snapshot_ptr m = make_snapshot(metrics, 1000, 95);
  const nsclient::core::diagnostics_snapshot_ptr d = make_diagnostics(diagnostics, 500, 96);
  ASSERT_TRUE(store.record(critical("check_cpu"), nullptr, "test", 0, 100, m, d));
  ASSERT_TRUE(store.record(critical("check_memory"), nullptr, "test", 0, 101, m, d));
  EXPECT_EQ(m->wire_size + d->wire_size, store.context_bytes_used());

  failed_check_store::entry_with_context first, second;
  ASSERT_TRUE(store.get(1, first));
  ASSERT_TRUE(store.get(2, second));
  EXPECT_EQ(first.diagnostics, second.diagnostics);
  EXPECT_EQ(PB::Failures::DiagnosticsContext::CAPTURED, first.item.diagnostics().state());
  EXPECT_EQ(d->id, first.item.diagnostics().snapshot_id());
  EXPECT_EQ(96, first.item.diagnostics().captured_at());
  EXPECT_EQ(d->wire_size, first.item.diagnostics().size());
  // The listing carries the reference, never the tables.
  EXPECT_FALSE(store.list()[0].diagnostics().has_snapshot());
}

TEST(failed_check_store, the_budget_releases_the_oldest_snapshot_whichever_kind) {
  nsclient::core::metrics_snapshot_store metrics;
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  // Room for two snapshots of ~40k, not three.
  store.configure(capturing_both(84 * 1024, 64 * 1024));
  // The diagnostics snapshot is held first, so it is the oldest.
  const nsclient::core::diagnostics_snapshot_ptr d = make_diagnostics(diagnostics, 40 * 1024, 90);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, nullptr, d));
  const nsclient::core::metrics_snapshot_ptr a = make_snapshot(metrics, 40 * 1024, 95);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 96, a));
  EXPECT_EQ(d->wire_size + a->wire_size, store.context_bytes_used());
  const nsclient::core::metrics_snapshot_ptr b = make_snapshot(metrics, 40 * 1024, 100);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, b));
  EXPECT_EQ(a->wire_size + b->wire_size, store.context_bytes_used());

  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::DiagnosticsContext::EVICTED, found.item.diagnostics().state());
  EXPECT_FALSE(found.diagnostics);
  EXPECT_EQ(d->id, found.item.diagnostics().snapshot_id());
  ASSERT_TRUE(store.get(2, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());

  // And the other way round: a metrics snapshot older than a diagnostics one
  // goes first.
  const nsclient::core::diagnostics_snapshot_ptr e = make_diagnostics(diagnostics, 40 * 1024, 110);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 111, nullptr, e));
  EXPECT_EQ(b->wire_size + e->wire_size, store.context_bytes_used());
  ASSERT_TRUE(store.get(2, found));
  EXPECT_EQ(PB::Failures::MetricsContext::EVICTED, found.item.context().state());
}

TEST(failed_check_store, the_budget_spares_the_newest_failures_snapshots_while_older_ones_are_left) {
  nsclient::core::metrics_snapshot_store metrics;
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  // Room for three snapshots of ~40k, not four.
  store.configure(capturing_both(124 * 1024, 64 * 1024));
  const nsclient::core::metrics_snapshot_ptr m1 = make_snapshot(metrics, 40 * 1024, 90);
  const nsclient::core::diagnostics_snapshot_ptr d1 = make_diagnostics(diagnostics, 40 * 1024, 90);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, m1, d1));
  // The second failure shares the metrics snapshot - the oldest one held -
  // and brings two new diagnostics snapshots' worth with the third.
  const nsclient::core::diagnostics_snapshot_ptr d2 = make_diagnostics(diagnostics, 40 * 1024, 95);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 96, m1, d2));
  const nsclient::core::diagnostics_snapshot_ptr d3 = make_diagnostics(diagnostics, 40 * 1024, 100);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, m1, d3));

  // m1 is the oldest held snapshot, but the newest failure refers to it: the
  // oldest one it does not refer to went instead.
  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(3, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());
  EXPECT_EQ(m1, found.context);
  EXPECT_EQ(PB::Failures::DiagnosticsContext::CAPTURED, found.item.diagnostics().state());
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());
  EXPECT_EQ(PB::Failures::DiagnosticsContext::EVICTED, found.item.diagnostics().state());
  EXPECT_EQ(m1->wire_size + d2->wire_size + d3->wire_size, store.context_bytes_used());
}

TEST(failed_check_store, a_budget_too_small_for_one_pair_keeps_the_newest_failures_metrics) {
  nsclient::core::metrics_snapshot_store metrics;
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  // Room for one snapshot of ~40k, not for a metrics and a diagnostics one.
  store.configure(capturing_both(64 * 1024, 64 * 1024));
  const nsclient::core::metrics_snapshot_ptr m1 = make_snapshot(metrics, 40 * 1024, 90);
  const nsclient::core::diagnostics_snapshot_ptr d1 = make_diagnostics(diagnostics, 40 * 1024, 90);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, m1, d1));

  failed_check_store::entry_with_context found;
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());
  EXPECT_EQ(m1, found.context);
  EXPECT_EQ(PB::Failures::DiagnosticsContext::EVICTED, found.item.diagnostics().state());
  EXPECT_FALSE(found.diagnostics);
  EXPECT_EQ(m1->wire_size, store.context_bytes_used());

  // The next failure's metrics replace the previous one's.
  const nsclient::core::metrics_snapshot_ptr m2 = make_snapshot(metrics, 40 * 1024, 100);
  const nsclient::core::diagnostics_snapshot_ptr d2 = make_diagnostics(diagnostics, 40 * 1024, 100);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, m2, d2));
  ASSERT_TRUE(store.get(1, found));
  EXPECT_EQ(PB::Failures::MetricsContext::EVICTED, found.item.context().state());
  ASSERT_TRUE(store.get(2, found));
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, found.item.context().state());
  EXPECT_EQ(PB::Failures::DiagnosticsContext::EVICTED, found.item.diagnostics().state());
  EXPECT_EQ(m2->wire_size, store.context_bytes_used());
}

TEST(failed_check_store, a_diagnostics_snapshot_over_the_size_cap_is_not_kept) {
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  store.configure(capturing_both(failed_check_store::default_context_budget, 1024));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, nullptr, make_diagnostics(diagnostics, 4096, 95)));
  const PB::Failures::FailedCheck entry = store.list()[0];
  EXPECT_EQ(PB::Failures::DiagnosticsContext::TOO_LARGE, entry.diagnostics().state());
  EXPECT_EQ(0u, store.context_bytes_used());
}

TEST(failed_check_store, turning_diagnostics_off_releases_only_diagnostics) {
  nsclient::core::metrics_snapshot_store metrics;
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  store.configure(capturing_both());
  const nsclient::core::metrics_snapshot_ptr m = make_snapshot(metrics, 1000, 95);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 100, m, make_diagnostics(diagnostics, 1000, 95)));
  store.configure(capturing());
  EXPECT_FALSE(store.captures_diagnostics());
  EXPECT_EQ(m->wire_size, store.context_bytes_used());
  const PB::Failures::FailedCheck entry = store.list()[0];
  EXPECT_EQ(PB::Failures::DiagnosticsContext::EVICTED, entry.diagnostics().state());
  EXPECT_EQ(PB::Failures::MetricsContext::CAPTURED, entry.context().state());
}

TEST(failed_check_store, ageing_out_and_clearing_release_diagnostics_too) {
  nsclient::core::diagnostics_snapshot_store diagnostics;
  failed_check_store store;
  failed_check_store::options o = capturing_both();
  o.max_entries = 1;
  store.configure(o);
  const nsclient::core::diagnostics_snapshot_ptr a = make_diagnostics(diagnostics, 1000, 90);
  const nsclient::core::diagnostics_snapshot_ptr b = make_diagnostics(diagnostics, 2000, 100);
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 91, nullptr, a));
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 101, nullptr, b));
  EXPECT_EQ(b->wire_size, store.context_bytes_used());
  store.clear();
  EXPECT_EQ(0u, store.context_bytes_used());
  // A snapshot held again after a clear is accounted afresh.
  ASSERT_TRUE(store.record(critical(), nullptr, "test", 0, 102, nullptr, b));
  EXPECT_EQ(b->wire_size, store.context_bytes_used());
}