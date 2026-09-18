// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_protocol.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace gearman;

namespace {

std::string header(const char *magic, const std::uint32_t type, const std::uint32_t size) {
  std::string out(magic, 4);
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((type >> shift) & 0xff));
  for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((size >> shift) & 0xff));
  return out;
}

const char request_magic[4] = {'\0', 'R', 'E', 'Q'};
const char response_magic[4] = {'\0', 'R', 'E', 'S'};

/** Encode, decode, and assert the arguments survived the round trip. */
void round_trip(const packet_type type, const std::vector<std::string> &args, const packet_magic magic = packet_magic::request) {
  const std::string wire = encode_packet(type, args, magic);
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  ASSERT_EQ(decode_result::ok, decode_packet(wire, decoded, consumed, error)) << to_string(type) << ": " << error;
  EXPECT_EQ(wire.size(), consumed);
  EXPECT_EQ(magic, decoded.magic);
  EXPECT_EQ(type, decoded.type);
  ASSERT_EQ(args.size(), decoded.args.size()) << to_string(type);
  for (std::size_t i = 0; i < args.size(); ++i) EXPECT_EQ(args[i], decoded.args[i]) << to_string(type) << " argument " << i;
}

}  // namespace

// ============================================================================
// Header layout
// ============================================================================

TEST(gearman_protocol, header_is_magic_type_and_size) {
  const std::string wire = encode_packet(packet_type::can_do, {"hostgroup_windows"});
  ASSERT_EQ(header_size + 17, wire.size());
  EXPECT_EQ(std::string(request_magic, 4), wire.substr(0, 4));
  EXPECT_EQ(std::string("\0\0\0\1", 4), wire.substr(4, 4));
  EXPECT_EQ(std::string("\0\0\0\21", 4), wire.substr(8, 4));
  EXPECT_EQ("hostgroup_windows", wire.substr(header_size));
}

TEST(gearman_protocol, a_response_carries_the_res_magic) {
  const std::string wire = encode_packet(packet_type::no_job, {}, packet_magic::response);
  EXPECT_EQ(std::string(response_magic, 4), wire.substr(0, 4));
  EXPECT_EQ(header_size, wire.size());
}

// ============================================================================
// Every packet the worker loop uses
// ============================================================================

TEST(gearman_protocol, round_trips_the_worker_packets) {
  round_trip(packet_type::can_do, {"hostgroup_windows"});
  round_trip(packet_type::cant_do, {"hostgroup_windows"});
  round_trip(packet_type::reset_abilities, {});
  round_trip(packet_type::pre_sleep, {});
  round_trip(packet_type::noop, {}, packet_magic::response);
  round_trip(packet_type::job_created, {"H:gearmand:17"}, packet_magic::response);
  round_trip(packet_type::grab_job, {});
  round_trip(packet_type::no_job, {}, packet_magic::response);
  round_trip(packet_type::job_assign, {"H:gearmand:17", "hostgroup_windows", "dHlwZT1zZXJ2aWNlCg=="}, packet_magic::response);
  round_trip(packet_type::work_complete, {"H:gearmand:17", ""});
  round_trip(packet_type::work_fail, {"H:gearmand:17"});
  round_trip(packet_type::submit_job_bg, {"check_results", "win-srv01-CPU load", "dHlwZT1wYXNzaXZlCg=="});
  round_trip(packet_type::error, {"27", "Invalid function"}, packet_magic::response);
  round_trip(packet_type::set_client_id, {"nscp-win-srv01-1"});
}

TEST(gearman_protocol, an_empty_last_argument_round_trips) { round_trip(packet_type::work_complete, {"H:gearmand:17", ""}); }

TEST(gearman_protocol, the_last_argument_may_contain_nul_bytes) {
  // A workload is binary as far as gearmand is concerned, so only the first
  // count-1 NULs separate arguments.
  const std::string workload("type=service\0binary\0tail", 24);
  round_trip(packet_type::job_assign, {"H:gearmand:17", "hostgroup_windows", workload}, packet_magic::response);
}

TEST(gearman_protocol, encoding_the_wrong_argument_count_throws) {
  EXPECT_THROW(encode_packet(packet_type::can_do, {}), protocol_error);
  EXPECT_THROW(encode_packet(packet_type::can_do, {"a", "b"}), protocol_error);
  EXPECT_THROW(encode_packet(packet_type::grab_job, {"a"}), protocol_error);
  EXPECT_THROW(encode_packet(static_cast<packet_type>(9999), {}), protocol_error);
}

// ============================================================================
// Decoding a stream
// ============================================================================

TEST(gearman_protocol, decodes_one_packet_at_a_time) {
  const std::string stream = encode_packet(packet_type::no_job, {}, packet_magic::response) +
                             encode_packet(packet_type::job_assign, {"H:1", "host", "payload"}, packet_magic::response);
  packet decoded;
  std::size_t consumed = 0;
  std::string error;

  ASSERT_EQ(decode_result::ok, decode_packet(stream, decoded, consumed, error));
  EXPECT_EQ(packet_type::no_job, decoded.type);
  EXPECT_EQ(header_size, consumed);

  const std::string rest = stream.substr(consumed);
  ASSERT_EQ(decode_result::ok, decode_packet(rest, decoded, consumed, error));
  EXPECT_EQ(packet_type::job_assign, decoded.type);
  EXPECT_EQ("payload", decoded.arg(2));
  EXPECT_EQ(rest.size(), consumed);
}

TEST(gearman_protocol, a_partial_packet_is_incomplete_not_invalid) {
  const std::string wire = encode_packet(packet_type::job_assign, {"H:1", "host", "payload"}, packet_magic::response);
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  for (std::size_t length = 0; length < wire.size(); ++length) {
    EXPECT_EQ(decode_result::incomplete, decode_packet(wire.data(), length, decoded, consumed, error)) << "at " << length << " bytes";
    EXPECT_EQ(0u, consumed);
  }
  EXPECT_EQ(decode_result::ok, decode_packet(wire, decoded, consumed, error));
}

TEST(gearman_protocol, a_bad_magic_is_invalid) {
  std::string wire = encode_packet(packet_type::no_job, {}, packet_magic::response);
  wire[1] = 'X';
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  EXPECT_EQ(decode_result::invalid, decode_packet(wire, decoded, consumed, error));
  EXPECT_NE(std::string::npos, error.find("magic"));
}

TEST(gearman_protocol, a_missing_argument_separator_is_invalid) {
  packet decoded;
  std::size_t consumed = 0;
  std::string error;

  // JOB_ASSIGN needs two separators. Both present with nothing after the
  // second is a job with an empty workload, which is well formed.
  const std::string empty_workload = std::string("H:1") + '\0' + "hostgroup_windows" + '\0';
  const std::string ok_wire = header(response_magic, 11, static_cast<std::uint32_t>(empty_workload.size())) + empty_workload;
  ASSERT_EQ(decode_result::ok, decode_packet(ok_wire, decoded, consumed, error)) << error;
  EXPECT_EQ("", decoded.arg(2));

  // One separator short is not.
  const std::string body = std::string("H:1") + '\0' + "hostgroup_windows";
  const std::string wire = header(response_magic, 11, static_cast<std::uint32_t>(body.size())) + body;
  EXPECT_EQ(decode_result::invalid, decode_packet(wire, decoded, consumed, error));
  EXPECT_NE(std::string::npos, error.find("argument"));
}

TEST(gearman_protocol, an_absurd_size_is_rejected_rather_than_waited_for) {
  // Without the bound this asks the reader to wait for four gigabytes.
  const std::string wire = header(response_magic, 11, 0xffffffffu);
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  EXPECT_EQ(decode_result::invalid, decode_packet(wire, decoded, consumed, error));
  EXPECT_NE(std::string::npos, error.find("limit"));
}

TEST(gearman_protocol, an_oversized_job_handle_is_rejected) {
  const std::string handle(max_job_handle_size + 1, 'H');
  const std::string body = handle + '\0' + "hostgroup_windows" + '\0' + "payload";
  const std::string wire = header(response_magic, 11, static_cast<std::uint32_t>(body.size())) + body;
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  EXPECT_EQ(decode_result::invalid, decode_packet(wire, decoded, consumed, error));
  EXPECT_NE(std::string::npos, error.find("job handle"));

  const std::string at_limit(max_job_handle_size, 'H');
  const std::string ok_body = at_limit + '\0' + "hostgroup_windows" + '\0' + "payload";
  const std::string ok_wire = header(response_magic, 11, static_cast<std::uint32_t>(ok_body.size())) + ok_body;
  EXPECT_EQ(decode_result::ok, decode_packet(ok_wire, decoded, consumed, error));
}

TEST(gearman_protocol, an_unknown_type_keeps_the_stream_in_sync) {
  // The length is still known, so the caller can log the packet and read on
  // instead of tearing the connection down.
  const std::string body = "whatever";
  const std::string wire =
      header(response_magic, 250, static_cast<std::uint32_t>(body.size())) + body + encode_packet(packet_type::no_job, {}, packet_magic::response);
  packet decoded;
  std::size_t consumed = 0;
  std::string error;
  ASSERT_EQ(decode_result::ok, decode_packet(wire, decoded, consumed, error));
  EXPECT_EQ("UNKNOWN(250)", to_string(decoded.type));
  EXPECT_EQ(body, decoded.arg(0));
  EXPECT_EQ(header_size + body.size(), consumed);

  ASSERT_EQ(decode_result::ok, decode_packet(wire.substr(consumed), decoded, consumed, error));
  EXPECT_EQ(packet_type::no_job, decoded.type);
}

TEST(gearman_protocol, missing_arguments_read_as_empty) {
  packet empty;
  EXPECT_EQ("", empty.arg(0));
  EXPECT_EQ("", empty.arg(7));
}

TEST(gearman_protocol, names_every_type_it_can_encode) {
  EXPECT_EQ("CAN_DO", to_string(packet_type::can_do));
  EXPECT_EQ("JOB_ASSIGN", to_string(packet_type::job_assign));
  EXPECT_EQ("SUBMIT_JOB_BG", to_string(packet_type::submit_job_bg));
  EXPECT_EQ("UNKNOWN(9999)", to_string(static_cast<packet_type>(9999)));
  EXPECT_EQ(-1, argument_count(static_cast<packet_type>(9999)));
  EXPECT_EQ(3, argument_count(packet_type::job_assign));
}
