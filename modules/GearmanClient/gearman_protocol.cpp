// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_protocol.hpp"

#include <algorithm>
#include <cstring>

namespace gearman {

namespace {

const char magic_request[4] = {'\0', 'R', 'E', 'Q'};
const char magic_response[4] = {'\0', 'R', 'E', 'S'};

std::uint32_t read_be32(const char *data) {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(data[0])) << 24) | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[1])) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(data[2])) << 8) | static_cast<std::uint32_t>(static_cast<unsigned char>(data[3]));
}

void append_be32(std::string &target, std::uint32_t value) {
  target.push_back(static_cast<char>((value >> 24) & 0xff));
  target.push_back(static_cast<char>((value >> 16) & 0xff));
  target.push_back(static_cast<char>((value >> 8) & 0xff));
  target.push_back(static_cast<char>(value & 0xff));
}

/**
 * True when the packet's first argument is a job handle. gearmand bounds a
 * handle at `max_job_handle_size`, so a longer one means the stream has lost
 * sync (or the peer is not gearmand) and the packet is refused.
 */
bool starts_with_job_handle(packet_type type) {
  switch (type) {
    case packet_type::job_created:
    case packet_type::job_assign:
    case packet_type::job_assign_uniq:
    case packet_type::work_status:
    case packet_type::work_complete:
    case packet_type::work_fail:
    case packet_type::work_data:
    case packet_type::work_warning:
    case packet_type::work_exception:
    case packet_type::get_status:
    case packet_type::status_res:
      return true;
    default:
      return false;
  }
}

const std::string empty_argument;

}  // namespace

const std::string &packet::arg(const std::size_t index) const { return index < args.size() ? args[index] : empty_argument; }

std::string to_string(const packet_type type) {
  switch (type) {
    case packet_type::can_do:
      return "CAN_DO";
    case packet_type::cant_do:
      return "CANT_DO";
    case packet_type::reset_abilities:
      return "RESET_ABILITIES";
    case packet_type::pre_sleep:
      return "PRE_SLEEP";
    case packet_type::noop:
      return "NOOP";
    case packet_type::submit_job:
      return "SUBMIT_JOB";
    case packet_type::job_created:
      return "JOB_CREATED";
    case packet_type::grab_job:
      return "GRAB_JOB";
    case packet_type::no_job:
      return "NO_JOB";
    case packet_type::job_assign:
      return "JOB_ASSIGN";
    case packet_type::work_status:
      return "WORK_STATUS";
    case packet_type::work_complete:
      return "WORK_COMPLETE";
    case packet_type::work_fail:
      return "WORK_FAIL";
    case packet_type::get_status:
      return "GET_STATUS";
    case packet_type::echo_req:
      return "ECHO_REQ";
    case packet_type::echo_res:
      return "ECHO_RES";
    case packet_type::submit_job_bg:
      return "SUBMIT_JOB_BG";
    case packet_type::error:
      return "ERROR";
    case packet_type::status_res:
      return "STATUS_RES";
    case packet_type::submit_job_high:
      return "SUBMIT_JOB_HIGH";
    case packet_type::set_client_id:
      return "SET_CLIENT_ID";
    case packet_type::can_do_timeout:
      return "CAN_DO_TIMEOUT";
    case packet_type::all_yours:
      return "ALL_YOURS";
    case packet_type::work_exception:
      return "WORK_EXCEPTION";
    case packet_type::option_req:
      return "OPTION_REQ";
    case packet_type::option_res:
      return "OPTION_RES";
    case packet_type::work_data:
      return "WORK_DATA";
    case packet_type::work_warning:
      return "WORK_WARNING";
    case packet_type::grab_job_uniq:
      return "GRAB_JOB_UNIQ";
    case packet_type::job_assign_uniq:
      return "JOB_ASSIGN_UNIQ";
    case packet_type::submit_job_high_bg:
      return "SUBMIT_JOB_HIGH_BG";
    case packet_type::submit_job_low:
      return "SUBMIT_JOB_LOW";
    case packet_type::submit_job_low_bg:
      return "SUBMIT_JOB_LOW_BG";
  }
  return "UNKNOWN(" + std::to_string(static_cast<std::uint32_t>(type)) + ")";
}

int argument_count(const packet_type type) {
  switch (type) {
    case packet_type::reset_abilities:
    case packet_type::pre_sleep:
    case packet_type::noop:
    case packet_type::grab_job:
    case packet_type::no_job:
    case packet_type::all_yours:
    case packet_type::grab_job_uniq:
      return 0;
    case packet_type::can_do:
    case packet_type::cant_do:
    case packet_type::job_created:
    case packet_type::work_fail:
    case packet_type::get_status:
    case packet_type::echo_req:
    case packet_type::echo_res:
    case packet_type::set_client_id:
    case packet_type::option_req:
    case packet_type::option_res:
      return 1;
    case packet_type::work_complete:
    case packet_type::error:
    case packet_type::can_do_timeout:
    case packet_type::work_exception:
    case packet_type::work_data:
    case packet_type::work_warning:
      return 2;
    case packet_type::submit_job:
    case packet_type::job_assign:
    case packet_type::work_status:
    case packet_type::submit_job_bg:
    case packet_type::submit_job_high:
    case packet_type::submit_job_high_bg:
    case packet_type::submit_job_low:
    case packet_type::submit_job_low_bg:
      return 3;
    case packet_type::job_assign_uniq:
      return 4;
    case packet_type::status_res:
      return 5;
  }
  return -1;
}

std::string encode_packet(const packet_type type, const std::vector<std::string> &args, const packet_magic magic) {
  const int expected = argument_count(type);
  if (expected < 0) throw protocol_error("cannot encode unknown gearman packet type " + to_string(type));
  if (args.size() != static_cast<std::size_t>(expected)) {
    throw protocol_error("gearman packet " + to_string(type) + " takes " + std::to_string(expected) + " argument(s), got " + std::to_string(args.size()));
  }

  std::size_t body_size = args.empty() ? 0 : args.size() - 1;
  for (const std::string &arg : args) body_size += arg.size();
  if (body_size > max_packet_size) throw protocol_error("gearman packet " + to_string(type) + " is larger than " + std::to_string(max_packet_size) + " bytes");

  std::string out;
  out.reserve(header_size + body_size);
  out.append(magic == packet_magic::request ? magic_request : magic_response, sizeof(magic_request));
  append_be32(out, static_cast<std::uint32_t>(type));
  append_be32(out, static_cast<std::uint32_t>(body_size));
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (i > 0) out.push_back('\0');
    out.append(args[i]);
  }
  return out;
}

decode_result decode_packet(const char *data, const std::size_t size, packet &out, std::size_t &consumed, std::string &error) {
  consumed = 0;
  if (size < header_size) return decode_result::incomplete;

  packet_magic magic;
  if (std::equal(magic_request, magic_request + sizeof(magic_request), data)) {
    magic = packet_magic::request;
  } else if (std::equal(magic_response, magic_response + sizeof(magic_response), data)) {
    magic = packet_magic::response;
  } else {
    error = "not a gearman packet: bad magic";
    return decode_result::invalid;
  }

  const packet_type type = static_cast<packet_type>(read_be32(data + 4));
  const std::uint32_t body_size = read_be32(data + 8);
  if (body_size > max_packet_size) {
    error = "gearman packet " + to_string(type) + " claims " + std::to_string(body_size) + " bytes, more than the " + std::to_string(max_packet_size) +
            " byte limit";
    return decode_result::invalid;
  }
  if (size - header_size < body_size) return decode_result::incomplete;

  const char *body = data + header_size;
  std::vector<std::string> args;
  const int count = argument_count(type);
  if (count < 0) {
    // An unknown type still has a length, so the stream stays in sync: hand
    // the body over untouched and let the caller log and skip the packet.
    if (body_size > 0) args.emplace_back(body, body_size);
  } else if (count > 0) {
    std::size_t start = 0;
    for (int i = 0; i + 1 < count; ++i) {
      const char *separator = static_cast<const char *>(std::memchr(body + start, '\0', body_size - start));
      if (separator == nullptr) {
        error = "gearman packet " + to_string(type) + " is missing argument " + std::to_string(i + 1);
        return decode_result::invalid;
      }
      const std::size_t offset = static_cast<std::size_t>(separator - body);
      args.emplace_back(body + start, offset - start);
      start = offset + 1;
    }
    args.emplace_back(body + start, body_size - start);
  }

  if (starts_with_job_handle(type) && !args.empty() && args[0].size() > max_job_handle_size) {
    error = "gearman packet " + to_string(type) + " carries a " + std::to_string(args[0].size()) + " byte job handle, more than the " +
            std::to_string(max_job_handle_size) + " byte limit";
    return decode_result::invalid;
  }

  out.magic = magic;
  out.type = type;
  out.args.swap(args);
  consumed = header_size + body_size;
  return decode_result::ok;
}

decode_result decode_packet(const std::string &buffer, packet &out, std::size_t &consumed, std::string &error) {
  return decode_packet(buffer.data(), buffer.size(), out, consumed, error);
}

}  // namespace gearman
