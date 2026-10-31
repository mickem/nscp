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

failed_check_store::options with_arguments() {
  failed_check_store::options o;
  o.record_arguments = true;
  return o;
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

TEST(failed_check_store, arguments_are_dropped_unless_recording_is_on) {
  PB::Commands::QueryRequestMessage request;
  add_request(request, "check_sql", {"query=select 1", "password=hunter2"});
  PB::Commands::QueryResponseMessage response;
  *response.add_payload() = make_response("check_sql", PB::Common::ResultCode::CRITICAL, "connection refused");
  // A module echoing the arguments in its payload must not leak them either.
  response.mutable_payload(0)->add_arguments("password=hunter2");

  failed_check_store store;
  EXPECT_EQ(1u, store.record_message(request, response, "NRPEServer", 0, 1));
  EXPECT_TRUE(arguments_of(store.list()[0]).empty());

  store.configure(with_arguments());
  EXPECT_EQ(1u, store.record_message(request, response, "NRPEServer", 0, 2));
  const std::vector<std::string> arguments = arguments_of(store.list()[0]);
  ASSERT_EQ(2u, arguments.size());
  EXPECT_EQ("query=select 1", arguments[0]);
  EXPECT_EQ("password=***", arguments[1]);
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
