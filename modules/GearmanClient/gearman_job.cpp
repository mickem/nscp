// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_job.hpp"

#include <chrono>
#include <cstdlib>

namespace gearman {

const char *const default_result_queue = "check_results";

std::string format_source(const std::string &version, const std::string &host_name) { return "NSClient++ " + version + " on " + host_name; }

namespace {

const char *const job_prefix = "type=";

/**
 * Timestamps are built and read digit by digit rather than through `%f` and
 * `strtod`: the cores always write a `.` and a C library in a comma-decimal
 * locale would print (and stop reading at) something else.
 */
std::string format_fraction(long microseconds) {
  std::string digits = std::to_string(microseconds);
  if (digits.size() < 6) digits.insert(0, 6 - digits.size(), '0');
  return digits;
}

long long to_number(const std::string &value, const long long fallback) {
  if (value.empty()) return fallback;
  char *end = nullptr;
  const long long parsed = std::strtoll(value.c_str(), &end, 10);
  if (end == value.c_str() || (end != nullptr && *end != '\0')) return fallback;
  return parsed;
}

void append_line(std::string &text, const std::string &key, const std::string &value) {
  text.append(key);
  text.push_back('=');
  text.append(value);
  text.push_back('\n');
}

void append_optional_line(std::string &text, const std::string &key, const std::string &value) {
  if (!value.empty()) append_line(text, key, value);
}

}  // namespace

field_map parse_key_value_text(const std::string &text) {
  field_map fields;
  std::string::size_type start = 0;
  while (start <= text.size()) {
    std::string::size_type end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string::size_type separator = text.find('=', start);
    if (separator != std::string::npos && separator > start && separator < end) {
      fields[text.substr(start, separator - start)] = text.substr(separator + 1, end - separator - 1);
    }
    if (end == text.size()) break;
    start = end + 1;
  }
  return fields;
}

std::string field(const field_map &fields, const std::string &key, const std::string &fallback) {
  const field_map::const_iterator it = fields.find(key);
  return it == fields.end() ? fallback : it->second;
}

bool looks_like_job_text(const std::string &text) { return text.compare(0, std::char_traits<char>::length(job_prefix), job_prefix) == 0; }

check_job parse_job(const std::string &text) {
  if (!looks_like_job_text(text)) throw job_error("payload does not start with type=, so it is not a Mod-Gearman job");
  const field_map fields = parse_key_value_text(text);

  check_job job;
  job.type = field(fields, "type");
  if (job.type != "host" && job.type != "service") throw job_error("not a check job: type=" + job.type);
  job.host_name = field(fields, "host_name");
  if (job.host_name.empty()) throw job_error("check job names no host");
  job.service_description = field(fields, "service_description");
  job.command_line = field(fields, "command_line");
  if (job.command_line.empty()) throw job_error("check job carries no command line");
  job.result_queue = field(fields, "result_queue", default_result_queue);
  job.target_queue = field(fields, "target_queue");
  job.core_time = field(fields, "core_time");
  job.start_time = field(fields, "start_time");
  job.next_check = field(fields, "next_check");
  job.timeout = to_number(field(fields, "timeout"), 0);
  return job;
}

std::string format_job(const check_job &job) {
  std::string text;
  append_line(text, "type", job.type);
  append_line(text, "result_queue", job.result_queue.empty() ? default_result_queue : job.result_queue);
  append_line(text, "target_queue", job.target_queue);
  append_line(text, "host_name", job.host_name);
  if (job.is_service()) append_line(text, "service_description", job.service_description);
  append_optional_line(text, "start_time", job.start_time);
  append_optional_line(text, "next_check", job.next_check);
  append_line(text, "core_time", job.core_time);
  append_line(text, "timeout", std::to_string(job.timeout));
  append_line(text, "command_line", job.command_line);
  // The NEB modules end a job with `command_line=%s\n\n\n`.
  text.append("\n\n");
  return text;
}

std::string format_result(const check_result &result) {
  std::string text;
  append_line(text, "type", result.type);
  append_line(text, "host_name", result.host_name);
  append_optional_line(text, "start_time", result.start_time);
  append_optional_line(text, "finish_time", result.finish_time);
  append_optional_line(text, "latency", result.latency);
  append_line(text, "return_code", std::to_string(result.return_code));
  append_optional_line(text, "source", result.source);
  if (result.exited_ok >= 0) append_line(text, "exited_ok", std::to_string(result.exited_ok));
  append_optional_line(text, "core_start_time", result.core_start_time);
  append_optional_line(text, "service_description", result.service_description);
  append_line(text, "output", escape_output(result.output));
  // send_gearman ends a result with a blank line.
  text.push_back('\n');
  return text;
}

check_result parse_result(const std::string &text) {
  if (!looks_like_job_text(text)) throw job_error("payload does not start with type=, so it is not a Mod-Gearman result");
  const field_map fields = parse_key_value_text(text);

  check_result result;
  result.type = field(fields, "type");
  if (result.type == "host" || result.type == "service") throw job_error("not a check result: type=" + result.type);
  result.host_name = field(fields, "host_name");
  if (result.host_name.empty()) throw job_error("check result names no host");
  result.service_description = field(fields, "service_description");
  result.output = unescape_output(field(fields, "output"));
  result.start_time = field(fields, "start_time");
  result.finish_time = field(fields, "finish_time");
  result.latency = field(fields, "latency");
  result.source = field(fields, "source");
  result.core_start_time = field(fields, "core_start_time");
  result.return_code = static_cast<int>(to_number(field(fields, "return_code"), 0));
  result.exited_ok = static_cast<int>(to_number(field(fields, "exited_ok"), -1));
  return result;
}

std::string escape_output(const std::string &output) {
  std::string wire;
  wire.reserve(output.size());
  for (const char c : output) {
    if (c == '\n') {
      wire.append("\\n");
    } else {
      wire.push_back(c);
    }
  }
  return wire;
}

std::string unescape_output(const std::string &wire) {
  std::string output;
  output.reserve(wire.size());
  for (std::string::size_type i = 0; i < wire.size(); ++i) {
    if (wire[i] == '\\' && i + 1 < wire.size() && wire[i + 1] == 'n') {
      output.push_back('\n');
      ++i;
    } else {
      output.push_back(wire[i]);
    }
  }
  return output;
}

std::string format_timestamp(std::time_t seconds, long microseconds) {
  if (microseconds >= 1000000) {
    seconds += microseconds / 1000000;
    microseconds %= 1000000;
  }
  if (microseconds < 0) microseconds = 0;
  return std::to_string(static_cast<long long>(seconds)) + "." + format_fraction(microseconds);
}

std::string now_timestamp() {
  const std::chrono::system_clock::duration since_epoch = std::chrono::system_clock::now().time_since_epoch();
  const std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
  const std::chrono::microseconds microseconds = std::chrono::duration_cast<std::chrono::microseconds>(since_epoch - seconds);
  return format_timestamp(static_cast<std::time_t>(seconds.count()), static_cast<long>(microseconds.count()));
}

bool parse_timestamp(const std::string &value, double &seconds) {
  if (value.empty()) return false;
  const std::string::size_type dot = value.find('.');
  const std::string whole = value.substr(0, dot);
  if (whole.empty()) return false;
  for (const char c : whole) {
    if (c < '0' || c > '9') return false;
  }
  char *end = nullptr;
  const long long parsed = std::strtoll(whole.c_str(), &end, 10);
  if (end == whole.c_str()) return false;

  double fraction = 0.0;
  if (dot != std::string::npos) {
    double scale = 0.1;
    for (std::string::size_type i = dot + 1; i < value.size(); ++i) {
      if (value[i] < '0' || value[i] > '9') return false;
      fraction += (value[i] - '0') * scale;
      scale /= 10.0;
    }
  }
  seconds = static_cast<double>(parsed) + fraction;
  return true;
}

}  // namespace gearman
