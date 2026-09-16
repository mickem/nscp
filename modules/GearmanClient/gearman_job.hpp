// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <map>
#include <stdexcept>
#include <string>

/**
 * The Mod-Gearman job and result text formats.
 *
 * Both are `key=value` lines inside the encrypted envelope. The core writes a
 * job with the plugin command line already macro-expanded; the worker writes
 * back a result the core's result thread files as if it had run the plugin
 * itself. Only the first `=` on a line separates the key from the value, since
 * a command line is full of them (`command_line=check_ok message=hello`).
 *
 * Fields the two flavours disagree on are optional: the nagios-mod-gearman NEB
 * module writes `start_time` and `next_check` into a job, ConSol's does not,
 * and neither the cores nor this module care about the order. Unknown keys are
 * ignored, so a newer core can add fields without breaking the worker.
 *
 * `format_job` and `format_result` reproduce the field order of the captured
 * payloads under `modules/GearmanClient/fixtures/` byte for byte, which is
 * what `gearman_job_test.cpp` asserts.
 */
namespace gearman {

/** Thrown when a payload decrypts but is not a job or a result. */
class job_error : public std::runtime_error {
 public:
  explicit job_error(const std::string &what) : std::runtime_error(what) {}
};

typedef std::map<std::string, std::string> field_map;

/** Split `key=value` lines. Blank lines and lines without a key are skipped. */
field_map parse_key_value_text(const std::string &text);

/** The first value for `key`, or `fallback` when the key is absent. */
std::string field(const field_map &fields, const std::string &key, const std::string &fallback = std::string());

/** The queue a worker submits a result to when the job names none. */
extern const char *const default_result_queue;

/** A check the core scheduled and handed to the queue. */
struct check_job {
  /** `host` or `service`. */
  std::string type;
  std::string host_name;
  /** Empty on a host check. */
  std::string service_description;
  /** Already macro-expanded by the core; for this module it is an NSClient++ query. */
  std::string command_line;
  /** Where the result goes; `check_results` when the job leaves it out. */
  std::string result_queue;
  /** The queue the core submitted to, e.g. `hostgroup_windows`. */
  std::string target_queue;
  /** Seconds with microseconds, kept verbatim so a result can quote it back. */
  std::string core_time;
  /** Written by the nagios-mod-gearman NEB module only. */
  std::string start_time;
  std::string next_check;
  /** Seconds the core allows the check; 0 when the job does not say. */
  long long timeout = 0;

  bool is_service() const { return type == "service"; }
};

/**
 * Cheap sanity check on a decrypted payload: every job and result starts with
 * `type=`. A payload that does not is a wrong key or a foreign queue, and
 * saying so beats a parse error listing missing fields.
 */
bool looks_like_job_text(const std::string &text);

/** Parse job text. Throws `job_error` when it is not a job or names no host. */
check_job parse_job(const std::string &text);

/** Job text in the NEB modules' field order, ending in the three newlines they write. */
std::string format_job(const check_job &job);

/** A check result on its way back to the core. */
struct check_result {
  /** `active` for a job this worker ran, `passive` for a submitted result. */
  std::string type;
  std::string host_name;
  /** Empty for a host result. */
  std::string service_description;
  int return_code = 0;
  /** Plugin output with its perf data; newlines are escaped on the wire. */
  std::string output;
  /** Seconds with microseconds; see `now_timestamp`. Empty fields are left out. */
  std::string start_time;
  std::string finish_time;
  std::string latency;
  /** How the result was produced, e.g. `NSClient++ 0.19.0 on win-srv01`. */
  std::string source;
  /** The job's `core_time`, quoted back so the core can measure its own latency. */
  std::string core_start_time;
  /** 1 when the check ran to completion, 0 when it did not; negative omits the line. */
  int exited_ok = -1;

  bool is_service() const { return !service_description.empty(); }
};

/** Result text in send_gearman's field order, ending in the blank line it writes. */
std::string format_result(const check_result &result);

/** Parse result text. Throws `job_error` when it is not a result. */
check_result parse_result(const std::string &text);

/** A newline inside plugin output travels as the two characters `\n`. */
std::string escape_output(const std::string &output);
std::string unescape_output(const std::string &wire);

/** `<seconds>.<microseconds>`, the `%.6f` the cores and their tools write. */
std::string format_timestamp(std::time_t seconds, long microseconds);
std::string now_timestamp();

/**
 * Read a timestamp as seconds since the epoch. Returns false for a field that
 * is absent or not a number, so a job with an unreadable `core_time` is simply
 * not aged out rather than discarded.
 */
bool parse_timestamp(const std::string &value, double &seconds);

}  // namespace gearman
