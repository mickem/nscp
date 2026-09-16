// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_job.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "gearman_crypt.hpp"

#ifndef GEARMAN_FIXTURE_DIR
#error "GEARMAN_FIXTURE_DIR must point at modules/GearmanClient/fixtures"
#endif

using namespace gearman;

namespace {

std::string read_fixture(const std::string &name) {
  const std::string path = std::string(GEARMAN_FIXTURE_DIR) + "/" + name;
  std::ifstream file(path.c_str(), std::ios::binary);
  EXPECT_TRUE(file.is_open()) << "missing fixture " << path;
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/** The text a real core or send_gearman put inside the captured envelope. */
std::string fixture_text(const std::string &name) { return decrypt_payload(read_fixture(name), read_fixture("key.txt")); }

}  // namespace

// ============================================================================
// key=value lines
// ============================================================================

TEST(gearman_job, only_the_first_equals_separates_key_from_value) {
  const field_map fields = parse_key_value_text("command_line=check_ok message=hello\nhost_name=win-srv01\n");
  EXPECT_EQ("check_ok message=hello", field(fields, "command_line"));
  EXPECT_EQ("win-srv01", field(fields, "host_name"));
}

TEST(gearman_job, blank_and_keyless_lines_are_ignored) {
  const field_map fields = parse_key_value_text("\ntype=service\n\nnonsense\n=novalue\nhost_name=win\n\n\n");
  EXPECT_EQ(2u, fields.size());
  EXPECT_EQ("service", field(fields, "type"));
  EXPECT_EQ("win", field(fields, "host_name"));
}

TEST(gearman_job, an_empty_value_is_still_a_field) {
  const field_map fields = parse_key_value_text("target_queue=\n");
  ASSERT_EQ(1u, fields.size());
  EXPECT_EQ("", field(fields, "target_queue"));
  EXPECT_EQ("fallback", field(fields, "missing", "fallback"));
}

TEST(gearman_job, a_last_line_without_a_newline_is_read) { EXPECT_EQ("service", field(parse_key_value_text("type=service"), "type")); }

// ============================================================================
// Job text, against what the two cores actually sent
// ============================================================================

TEST(gearman_job, parses_the_service_job_consol_mod_gearman_sent) {
  const check_job job = parse_job(fixture_text("naemon-job-service.b64"));
  EXPECT_EQ("service", job.type);
  EXPECT_TRUE(job.is_service());
  EXPECT_EQ("nscp-test", job.host_name);
  EXPECT_EQ("helper", job.service_description);
  EXPECT_EQ("check_ok message=hello", job.command_line);
  EXPECT_EQ("check_results", job.result_queue);
  EXPECT_EQ("hostgroup_gearman-test", job.target_queue);
  EXPECT_EQ("1789564903.687813", job.core_time);
  EXPECT_EQ(3, job.timeout);
  EXPECT_EQ("", job.start_time);
  EXPECT_EQ("", job.next_check);
}

TEST(gearman_job, parses_the_host_job_consol_mod_gearman_sent) {
  const check_job job = parse_job(fixture_text("naemon-job-host.b64"));
  EXPECT_EQ("host", job.type);
  EXPECT_FALSE(job.is_service());
  EXPECT_EQ("nscp-test", job.host_name);
  EXPECT_EQ("", job.service_description);
  EXPECT_EQ("check_always_ok", job.command_line);
}

TEST(gearman_job, the_nagios_forks_extra_fields_are_read_not_tripped_over) {
  const check_job job = parse_job(fixture_text("nagios-job-service.b64"));
  EXPECT_EQ("service", job.type);
  EXPECT_EQ("helper", job.service_description);
  EXPECT_EQ("check_ok message=hello", job.command_line);
  EXPECT_EQ("1789564901.437880", job.core_time);
  EXPECT_EQ("1789564901.0", job.start_time);
  EXPECT_EQ("1789564901.0", job.next_check);
}

TEST(gearman_job, reformatting_a_captured_job_reproduces_it_byte_for_byte) {
  const char *const fixtures[] = {"naemon-job-host.b64", "naemon-job-service.b64", "nagios-job-host.b64", "nagios-job-service.b64"};
  for (const char *const name : fixtures) {
    const std::string text = fixture_text(name);
    EXPECT_EQ(text, format_job(parse_job(text))) << name;
  }
}

TEST(gearman_job, a_job_ends_in_the_three_newlines_the_neb_modules_write) {
  check_job job;
  job.type = "service";
  job.host_name = "win-srv01";
  job.service_description = "CPU load";
  job.command_line = "check_cpu warn=load>80";
  job.target_queue = "hostgroup_windows";
  job.core_time = "1757930400.123456";
  job.timeout = 60;
  EXPECT_EQ(
      "type=service\n"
      "result_queue=check_results\n"
      "target_queue=hostgroup_windows\n"
      "host_name=win-srv01\n"
      "service_description=CPU load\n"
      "core_time=1757930400.123456\n"
      "timeout=60\n"
      "command_line=check_cpu warn=load>80\n\n\n",
      format_job(job));
}

TEST(gearman_job, unknown_keys_are_ignored) {
  const check_job job = parse_job(
      "type=service\nhost_name=win-srv01\nservice_description=CPU\ncommand_line=check_cpu\n"
      "some_field_a_newer_core_added=17\n");
  EXPECT_EQ("check_cpu", job.command_line);
  EXPECT_EQ("check_results", job.result_queue);
  EXPECT_EQ(0, job.timeout);
}

TEST(gearman_job, a_payload_that_is_not_a_job_is_refused) {
  EXPECT_FALSE(looks_like_job_text("rubbish"));
  EXPECT_FALSE(looks_like_job_text(""));
  EXPECT_TRUE(looks_like_job_text("type=service\n"));

  EXPECT_THROW(parse_job("rubbish"), job_error);
  // A result is not a job, however well formed.
  EXPECT_THROW(parse_job(fixture_text("naemon-result-passive-service.b64")), job_error);
  EXPECT_THROW(parse_job("type=service\ncommand_line=check_cpu\n"), job_error);
  EXPECT_THROW(parse_job("type=service\nhost_name=win-srv01\n"), job_error);
}

TEST(gearman_job, an_unreadable_timeout_is_no_timeout_rather_than_a_refusal) {
  const check_job job = parse_job("type=host\nhost_name=win-srv01\ncommand_line=check_ok\ntimeout=soon\n");
  EXPECT_EQ(0, job.timeout);
}

// ============================================================================
// Result text, against what send_gearman actually sent
// ============================================================================

TEST(gearman_job, parses_a_passive_service_result) {
  const check_result result = parse_result(fixture_text("naemon-result-passive-service.b64"));
  EXPECT_EQ("passive", result.type);
  EXPECT_EQ("nscp-test", result.host_name);
  EXPECT_EQ("helper", result.service_description);
  EXPECT_TRUE(result.is_service());
  EXPECT_EQ(0, result.return_code);
  EXPECT_EQ("OK: hello|'time'=1ms;5;10", result.output);
  EXPECT_EQ("send_gearman", result.source);
  EXPECT_EQ("0.000000", result.latency);
  EXPECT_EQ(-1, result.exited_ok);
}

TEST(gearman_job, parses_a_passive_host_result) {
  const check_result result = parse_result(fixture_text("nagios-result-passive-host.b64"));
  EXPECT_EQ("passive", result.type);
  EXPECT_EQ("", result.service_description);
  EXPECT_FALSE(result.is_service());
  EXPECT_EQ(1, result.return_code);
  EXPECT_EQ("WARNING: host result", result.output);
  EXPECT_EQ("nagios-send-gearman", result.source);
}

TEST(gearman_job, a_newline_in_the_output_travels_escaped) {
  const check_result result = parse_result(fixture_text("naemon-result-active-service.b64"));
  EXPECT_EQ("active", result.type);
  EXPECT_EQ(2, result.return_code);
  EXPECT_EQ("CRITICAL: active result\nsecond line|'load'=12%;80;90", result.output);
  EXPECT_EQ("1757930400.000000", result.start_time);
  EXPECT_EQ("1757930401.000000", result.finish_time);
  EXPECT_EQ("0.500000", result.latency);
}

TEST(gearman_job, reformatting_a_captured_result_reproduces_it_byte_for_byte) {
  const char *const fixtures[] = {"naemon-result-active-service.b64", "naemon-result-passive-host.b64", "naemon-result-passive-service.b64",
                                  "nagios-result-active-service.b64", "nagios-result-passive-host.b64", "nagios-result-passive-service.b64"};
  for (const char *const name : fixtures) {
    const std::string text = fixture_text(name);
    EXPECT_EQ(text, format_result(parse_result(text))) << name;
  }
}

TEST(gearman_job, a_worker_result_carries_exited_ok_and_the_cores_own_time) {
  check_result result;
  result.type = "active";
  result.host_name = "win-srv01";
  result.service_description = "CPU load";
  result.return_code = 0;
  result.output = "OK: CPU load is ok.|'total 5m'=12%;80;90";
  result.start_time = "1757930400.234000";
  result.finish_time = "1757930400.412000";
  result.latency = "0.110000";
  result.source = "NSClient++ 0.19.0 on win-srv01";
  result.core_start_time = "1757930400.123456";
  result.exited_ok = 1;
  EXPECT_EQ(
      "type=active\n"
      "host_name=win-srv01\n"
      "start_time=1757930400.234000\n"
      "finish_time=1757930400.412000\n"
      "latency=0.110000\n"
      "return_code=0\n"
      "source=NSClient++ 0.19.0 on win-srv01\n"
      "exited_ok=1\n"
      "core_start_time=1757930400.123456\n"
      "service_description=CPU load\n"
      "output=OK: CPU load is ok.|'total 5m'=12%;80;90\n\n",
      format_result(result));
}

TEST(gearman_job, a_host_result_leaves_the_service_line_out) {
  check_result result;
  result.type = "passive";
  result.host_name = "win-srv01";
  result.return_code = 2;
  result.output = "CRITICAL: down";
  EXPECT_EQ(
      "type=passive\n"
      "host_name=win-srv01\n"
      "return_code=2\n"
      "output=CRITICAL: down\n\n",
      format_result(result));
}

TEST(gearman_job, a_result_that_is_not_a_result_is_refused) {
  EXPECT_THROW(parse_result("rubbish"), job_error);
  EXPECT_THROW(parse_result(fixture_text("naemon-job-service.b64")), job_error);
  EXPECT_THROW(parse_result("type=passive\noutput=no host here\n"), job_error);
}

// ============================================================================
// Output escaping
// ============================================================================

TEST(gearman_job, escaping_round_trips) {
  const std::string output = "line one\nline two\nline three|'perf'=1";
  EXPECT_EQ("line one\\nline two\\nline three|'perf'=1", escape_output(output));
  EXPECT_EQ(output, unescape_output(escape_output(output)));
}

TEST(gearman_job, a_backslash_that_is_not_an_escape_survives) {
  EXPECT_EQ("C:\\temp\\file", unescape_output("C:\\temp\\file"));
  // A literal backslash-n in the output is indistinguishable from an escaped
  // newline on this wire; the cores have the same ambiguity.
  EXPECT_EQ("a\nb", unescape_output("a\\nb"));
  EXPECT_EQ("trailing\\", unescape_output("trailing\\"));
}

// ============================================================================
// Timestamps
// ============================================================================

TEST(gearman_job, a_timestamp_is_seconds_and_six_digits_of_microseconds) {
  EXPECT_EQ("1757930400.123456", format_timestamp(1757930400, 123456));
  EXPECT_EQ("1757930400.000000", format_timestamp(1757930400, 0));
  EXPECT_EQ("1757930400.000007", format_timestamp(1757930400, 7));
  EXPECT_EQ("1757930401.000000", format_timestamp(1757930400, 1000000));
}

TEST(gearman_job, now_is_a_timestamp_the_cores_can_read) {
  double seconds = 0;
  ASSERT_TRUE(parse_timestamp(now_timestamp(), seconds));
  // Any clock this side of 2020 with a plausible upper bound.
  EXPECT_GT(seconds, 1577836800.0);
}

TEST(gearman_job, timestamps_parse_back_to_seconds) {
  double seconds = 0;
  ASSERT_TRUE(parse_timestamp("1757930400.123456", seconds));
  EXPECT_NEAR(1757930400.123456, seconds, 0.000001);
  ASSERT_TRUE(parse_timestamp("1789564901.0", seconds));
  EXPECT_NEAR(1789564901.0, seconds, 0.000001);
  ASSERT_TRUE(parse_timestamp("42", seconds));
  EXPECT_NEAR(42.0, seconds, 0.000001);
}

TEST(gearman_job, an_unreadable_timestamp_does_not_pretend_to_be_a_number) {
  // A job whose core_time cannot be read must not be aged out by accident.
  double seconds = -1;
  EXPECT_FALSE(parse_timestamp("", seconds));
  EXPECT_FALSE(parse_timestamp("soon", seconds));
  EXPECT_FALSE(parse_timestamp("17.abc", seconds));
  EXPECT_FALSE(parse_timestamp("-17.0", seconds));
  EXPECT_FALSE(parse_timestamp(".5", seconds));
  EXPECT_EQ(-1, seconds);
}
