// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * The gearman binary protocol, as far as a Mod-Gearman worker needs it.
 *
 * Pure encoding and decoding: nothing here touches a socket, so the whole
 * packet layer is testable without a server. A packet is a twelve byte header
 * (a four byte magic, then the type and the body size as big-endian 32 bit
 * integers) followed by the body, which is the packet's arguments joined with
 * NUL bytes. The last argument runs to the end of the body and may itself
 * contain NULs - a job workload is binary as far as gearmand is concerned - so
 * a decoder splits on the first count-1 separators only.
 *
 * Reference: https://github.com/gearman/gearmand/blob/master/PROTOCOL
 */
namespace gearman {

/** Thrown when a caller hands the encoder something the protocol cannot carry. */
class protocol_error : public std::runtime_error {
 public:
  explicit protocol_error(const std::string &what) : std::runtime_error(what) {}
};

/** Packet type codes from the gearmand PROTOCOL document. */
enum class packet_type : std::uint32_t {
  can_do = 1,
  cant_do = 2,
  reset_abilities = 3,
  pre_sleep = 4,
  noop = 6,
  submit_job = 7,
  job_created = 8,
  grab_job = 9,
  no_job = 10,
  job_assign = 11,
  work_status = 12,
  work_complete = 13,
  work_fail = 14,
  get_status = 15,
  echo_req = 16,
  echo_res = 17,
  submit_job_bg = 18,
  error = 19,
  status_res = 20,
  submit_job_high = 21,
  set_client_id = 22,
  can_do_timeout = 23,
  all_yours = 24,
  work_exception = 25,
  option_req = 26,
  option_res = 27,
  work_data = 28,
  work_warning = 29,
  grab_job_uniq = 30,
  job_assign_uniq = 31,
  submit_job_high_bg = 32,
  submit_job_low = 33,
  submit_job_low_bg = 34,
};

/** `\0REQ` from a client or worker, `\0RES` from the server. */
enum class packet_magic { request, response };

/** Magic, type and size. */
const std::size_t header_size = 12;
const unsigned int default_port = 4730;

/**
 * gearmand's own bound on a job handle (`GEARMAN_JOB_HANDLE_SIZE`). Real
 * handles look like `H:gearmand:17`, so anything longer is a corrupt stream
 * or a peer that is not gearmand, and is rejected rather than copied.
 */
const std::size_t max_job_handle_size = 64;

/**
 * Refuse a body larger than this. The size field is 32 bits and arrives from
 * the network before a single byte of the body does, so without a bound a
 * stray header makes the reader wait for (and reserve) four gigabytes. A
 * check result is a few hundred bytes; a megabyte is already absurd.
 */
const std::size_t max_packet_size = 16 * 1024 * 1024;

struct packet {
  packet_magic magic = packet_magic::request;
  packet_type type = packet_type::noop;
  /** Arguments as raw bytes; the last one may contain NUL bytes. */
  std::vector<std::string> args;

  /** The argument at `index`, or an empty string when the packet is shorter. */
  const std::string &arg(std::size_t index) const;
};

/** `CAN_DO`, `JOB_ASSIGN`, ... or `UNKNOWN(42)` for a code we do not know. */
std::string to_string(packet_type type);

/**
 * How many NUL separated arguments the packet carries, or -1 when the type is
 * unknown (in which case the body is handed back as one opaque argument).
 */
int argument_count(packet_type type);

/** Encode one packet. Throws `protocol_error` when the argument count is wrong. */
std::string encode_packet(packet_type type, const std::vector<std::string> &args = std::vector<std::string>(), packet_magic magic = packet_magic::request);

enum class decode_result {
  /** A whole packet was decoded; `consumed` bytes may be dropped from the buffer. */
  ok,
  /** The buffer holds less than one packet; read more and try again. */
  incomplete,
  /** The stream is not gearman (or not sane); `error` says why. Reconnect. */
  invalid
};

/** Decode the packet at the front of the buffer. */
decode_result decode_packet(const char *data, std::size_t size, packet &out, std::size_t &consumed, std::string &error);
decode_result decode_packet(const std::string &buffer, packet &out, std::size_t &consumed, std::string &error);

}  // namespace gearman
