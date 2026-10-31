// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/thread/mutex.hpp>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <deque>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/failures.hpp>
#include <string>
#include <vector>

namespace nsclient {
namespace core {

// The failed-check log: a bounded, in-memory record of the last N check results
// that did not come back OK, whatever transport asked for them.
//
// Every check passes through plugin_manager::execute_query on its way back to
// the caller, and that is where the core records the non-OK payloads here -
// including the ones the core itself synthesises (an unknown command, a denied
// one, a module whose handler failed). Consumers read the log through
// NSAPIFailuresQuery (service/core_api.cpp): the REST API and the web UI, the
// `nscp test` console, or any module.
//
// What is kept is the response payload itself (PB::Commands::QueryResponse-
// Message::Response), so the perf data stays structured and nothing is
// re-parsed later - the same choice the facts and metrics repositories make.
// The payload is copied and capped on the way in (lines, line length, perf
// entries, `data` dropped), so one check with an enormous output cannot own
// the whole budget. Arguments are opt-in: a check's options can carry a
// credential (`check_sql password=...`, a forwarded `check_nrpe` token), so
// they are only recorded when the operator asks, and even then anything that
// looks like one is redacted.
//
// Thread safety: record/list/clear/configure can be called from any thread.
// record() is on the hot path of every check, so it does its copying outside
// the lock and holds it only for the push.
class failed_check_store {
 public:
  typedef PB::Failures::FailedCheck entry;
  typedef std::vector<entry> entry_list;

  // Which statuses count as "failed", as a bitmask over PB::Common::ResultCode.
  static constexpr unsigned status_bit(const int code) { return code >= 0 && code < 32 ? 1u << code : 0u; }
  static constexpr unsigned default_statuses =
      (1u << PB::Common::ResultCode::WARNING) | (1u << PB::Common::ResultCode::CRITICAL) | (1u << PB::Common::ResultCode::UNKNOWN);

  // Bounds. The entry cap is the operator's ([/settings/core/failed checks]
  // max entries); the rest cap one entry so the budget is predictable.
  static constexpr std::size_t default_max_entries = 50;
  static constexpr std::size_t max_max_entries = 10000;
  static constexpr std::size_t max_lines = 100;
  static constexpr std::size_t max_line_length = 4096;
  static constexpr std::size_t max_perf_per_line = 100;
  static constexpr std::size_t max_arguments = 100;
  static constexpr std::size_t max_argument_length = 1024;
  static constexpr std::size_t max_subject_length = 256;

  struct options {
    bool enabled = true;
    std::size_t max_entries = default_max_entries;
    bool record_arguments = false;
    unsigned statuses = default_statuses;
  };

  failed_check_store() : next_index_(1), dropped_(0) {}

  // Apply a configuration. A lowered cap trims the oldest entries now;
  // disabling the store empties it, so nothing recorded while it was on stays
  // readable after the operator turned it off. Out-of-range values are
  // clamped: 0 entries would be a silently useless store, so it becomes 1.
  void configure(const options &value) {
    options clamped = value;
    if (clamped.max_entries < 1) clamped.max_entries = 1;
    if (clamped.max_entries > max_max_entries) clamped.max_entries = max_max_entries;
    clamped.statuses &= default_statuses;
    boost::unique_lock<boost::mutex> lock(mutex_);
    options_ = clamped;
    if (!options_.enabled) {
      entries_.clear();
      return;
    }
    trim_locked();
  }

  options get_options() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return options_;
  }

  // Whether a result with this code is recorded under the current options.
  bool records(const PB::Common::ResultCode code) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return options_.enabled && (options_.statuses & status_bit(code)) != 0;
  }

  // Record one payload. `arguments` is the request's argument list for this
  // command (null when the caller has none to offer); it is only kept when
  // `record arguments` is on, and redacted then. Returns true when an entry
  // was stored: the store is enabled, and the result is one it records.
  bool record(const PB::Commands::QueryResponseMessage::Response &response, const google::protobuf::RepeatedPtrField<std::string> *arguments,
              const std::string &subject, const int depth, const std::int64_t now) {
    options current;
    {
      boost::unique_lock<boost::mutex> lock(mutex_);
      if (!options_.enabled || (options_.statuses & status_bit(response.result())) == 0) return false;
      current = options_;
    }

    entry item;
    item.set_timestamp(now);
    item.set_subject(clip(subject, max_subject_length));
    item.set_depth(depth < 0 ? 0 : depth);
    PB::Commands::QueryResponseMessage::Response *copy = item.mutable_response();
    copy->CopyFrom(response);
    sanitize(copy, current.record_arguments ? arguments : nullptr);

    boost::unique_lock<boost::mutex> lock(mutex_);
    // Re-checked under the lock: configure() may have switched the store off
    // while the copy was being made, and an entry landing after the clear
    // would outlive the operator's decision.
    if (!options_.enabled) return false;
    item.set_index(next_index_++);
    entries_.push_back(item);
    trim_locked();
    return true;
  }
  bool record(const PB::Commands::QueryResponseMessage::Response &response, const google::protobuf::RepeatedPtrField<std::string> *arguments,
              const std::string &subject, const int depth) {
    return record(response, arguments, subject, depth, now());
  }

  // Record every payload of a response the store is configured to record,
  // pairing each with the request payload of the same command so its
  // arguments can be kept when that is on. A module may answer in a different
  // order than it was asked (the core dispatches one chunk per module), so the
  // pairing is by command name, each request payload used once. Returns how
  // many entries were stored.
  std::size_t record_message(const PB::Commands::QueryRequestMessage &request, const PB::Commands::QueryResponseMessage &response, const std::string &subject,
                             const int depth, const std::int64_t now) {
    std::vector<bool> used(static_cast<std::size_t>(request.payload_size()), false);
    std::size_t stored = 0;
    for (const PB::Commands::QueryResponseMessage::Response &payload : response.payload()) {
      const google::protobuf::RepeatedPtrField<std::string> *arguments = nullptr;
      for (int i = 0; i < request.payload_size(); ++i) {
        if (used[static_cast<std::size_t>(i)]) continue;
        if (!same_command(request.payload(i).command(), payload.command())) continue;
        used[static_cast<std::size_t>(i)] = true;
        arguments = &request.payload(i).arguments();
        break;
      }
      if (record(payload, arguments, subject, depth, now)) ++stored;
    }
    return stored;
  }
  std::size_t record_message(const PB::Commands::QueryRequestMessage &request, const PB::Commands::QueryResponseMessage &response, const std::string &subject,
                             const int depth) {
    return record_message(request, response, subject, depth, now());
  }

  // The entries with an index greater than `since`, newest first, at most
  // `limit` of them (0 for all).
  entry_list list(const unsigned long long since = 0, const std::size_t limit = 0) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    entry_list out;
    for (std::deque<entry>::const_reverse_iterator it = entries_.rbegin(); it != entries_.rend(); ++it) {
      if (it->index() <= since) break;
      if (limit > 0 && out.size() >= limit) break;
      out.push_back(*it);
    }
    return out;
  }

  // Drop everything. Returns how many entries were removed. The index keeps
  // counting, so a poller's `since` stays valid across a clear.
  std::size_t clear() {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const std::size_t removed = entries_.size();
    entries_.clear();
    return removed;
  }

  std::size_t size() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return entries_.size();
  }

  // The highest index handed out so far; 0 before the first entry.
  unsigned long long last_index() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return next_index_ - 1;
  }

  // How many entries the cap has evicted since the process started. A clear
  // does not count: the operator asked for that.
  unsigned long long dropped() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return dropped_;
  }

  // Parse the `statuses` setting: a comma-separated list of warning, critical
  // and unknown (case-insensitive, surrounding space ignored; `warn` and
  // `crit` and the Nagios numbers 1-3 are accepted too). Returns false - with
  // `error` saying which token - on anything else, including `ok`: an OK
  // result is not a failed check, whatever the configuration says.
  static bool parse_statuses(const std::string &text, unsigned &out, std::string &error) {
    unsigned mask = 0;
    std::string::size_type start = 0;
    while (start <= text.size()) {
      std::string::size_type end = text.find(',', start);
      if (end == std::string::npos) end = text.size();
      std::string token = text.substr(start, end - start);
      boost::algorithm::trim(token);
      boost::algorithm::to_lower(token);
      start = end + 1;
      if (token.empty()) continue;
      if (token == "warning" || token == "warn" || token == "1")
        mask |= status_bit(PB::Common::ResultCode::WARNING);
      else if (token == "critical" || token == "crit" || token == "2")
        mask |= status_bit(PB::Common::ResultCode::CRITICAL);
      else if (token == "unknown" || token == "3")
        mask |= status_bit(PB::Common::ResultCode::UNKNOWN);
      else {
        error = "'" + token + "' is not a status that can be recorded (expected a list of warning, critical, unknown)";
        return false;
      }
    }
    if (mask == 0) {
      error = "at least one of warning, critical, unknown is required";
      return false;
    }
    out = mask;
    return true;
  }

  // The canonical spelling of a status mask, as parse_statuses reads it.
  static std::string format_statuses(const unsigned statuses) {
    std::string out;
    if (statuses & status_bit(PB::Common::ResultCode::WARNING)) out += "warning";
    if (statuses & status_bit(PB::Common::ResultCode::CRITICAL)) out += (out.empty() ? "" : ",") + std::string("critical");
    if (statuses & status_bit(PB::Common::ResultCode::UNKNOWN)) out += (out.empty() ? "" : ",") + std::string("unknown");
    return out;
  }

  // Whether an argument key names a credential. The same rule the client
  // side uses for its own logging (client::is_sensitive_key): a key that
  // mentions a password or a token, with leading option dashes ignored.
  // A message is the check's own text and is kept as it was, with one
  // exception: a `password=...` or `token=...` pair inside it loses its value.
  // A check that cannot parse its options echoes the offending argument back
  // ("unrecognised option 'password=hunter2'"), which would put the secret in
  // the log even with `record arguments` off. The key is the run of
  // non-blank, non-quote characters before the `=`; the value runs to the
  // next blank or quote.
  static std::string redact_message(const std::string &message) {
    std::string out = message;
    std::string::size_type pos = 0;
    while ((pos = out.find('=', pos)) != std::string::npos) {
      std::string::size_type start = pos;
      while (start > 0 && !is_boundary(out[start - 1])) --start;
      std::string::size_type end = pos + 1;
      while (end < out.size() && !is_boundary(out[end])) ++end;
      if (end > pos + 1 && is_sensitive_argument(out.substr(start, pos - start))) {
        out.replace(pos + 1, end - pos - 1, "***");
        end = pos + 4;
      }
      pos = end;
    }
    return out;
  }
  static bool is_boundary(const char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\'' || c == '"' || c == ',' || c == ';';
  }
  static bool is_sensitive_argument(const std::string &key) {
    std::string lower = key;
    boost::algorithm::to_lower(lower);
    const std::string::size_type first = lower.find_first_not_of('-');
    if (first == std::string::npos) return false;
    lower = lower.substr(first);
    return lower.find("password") != std::string::npos || lower.find("token") != std::string::npos;
  }

  // Redact credential-looking arguments in place. Two shapes are handled:
  // `password=secret` (REST, and the undashed half of the CLI pair) becomes
  // `password=***`, and a bare `--password` or `password` token has the token
  // that follows it replaced, which is how the CLI passes `-a password=x`.
  static void redact_arguments(google::protobuf::RepeatedPtrField<std::string> *arguments) {
    if (arguments == nullptr) return;
    bool redact_next = false;
    for (std::string &argument : *arguments) {
      if (redact_next) {
        argument = "***";
        redact_next = false;
        continue;
      }
      const std::string::size_type eq = argument.find('=');
      if (eq == std::string::npos) {
        redact_next = is_sensitive_argument(argument);
        continue;
      }
      if (is_sensitive_argument(argument.substr(0, eq))) argument = argument.substr(0, eq + 1) + "***";
    }
  }

  static std::int64_t now() { return static_cast<std::int64_t>(std::time(nullptr)); }

 private:
  // Apply the per-entry caps to a copied payload and attach (or drop) the
  // arguments. `arguments` is null when they are not to be recorded.
  static void sanitize(PB::Commands::QueryResponseMessage::Response *copy, const google::protobuf::RepeatedPtrField<std::string> *arguments) {
    // `data` is an opaque blob some modules use for their own protocol; it
    // has no place in a human-facing log and can be arbitrarily large.
    copy->clear_data();
    if (copy->lines_size() > static_cast<int>(max_lines)) {
      copy->mutable_lines()->DeleteSubrange(static_cast<int>(max_lines), copy->lines_size() - static_cast<int>(max_lines));
    }
    for (PB::Commands::QueryResponseMessage::Response::Line &line : *copy->mutable_lines()) {
      if (line.message().size() > max_line_length) line.set_message(clip(line.message(), max_line_length));
      line.set_message(redact_message(line.message()));
      if (line.perf_size() > static_cast<int>(max_perf_per_line)) {
        line.mutable_perf()->DeleteSubrange(static_cast<int>(max_perf_per_line), line.perf_size() - static_cast<int>(max_perf_per_line));
      }
    }
    copy->clear_arguments();
    if (arguments == nullptr) return;
    for (const std::string &argument : *arguments) {
      if (copy->arguments_size() >= static_cast<int>(max_arguments)) break;
      copy->add_arguments(argument.size() > max_argument_length ? clip(argument, max_argument_length) : argument);
    }
    redact_arguments(copy->mutable_arguments());
  }

  // Truncate to `length` bytes with a marker, so a reader can tell a capped
  // message from one that happened to end there.
  static std::string clip(const std::string &text, const std::size_t length) {
    if (text.size() <= length) return text;
    if (length <= 3) return text.substr(0, length);
    return text.substr(0, length - 3) + "...";
  }

  // Command names are case-insensitive throughout the core (the registry
  // lower-cases them), and a module echoes back the name it was given.
  static bool same_command(const std::string &lhs, const std::string &rhs) {
    if (lhs.size() != rhs.size()) return false;
    for (std::string::size_type i = 0; i < lhs.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i]))) return false;
    }
    return true;
  }

  // Callers must hold the lock.
  void trim_locked() {
    while (entries_.size() > options_.max_entries) {
      entries_.pop_front();
      ++dropped_;
    }
  }

  mutable boost::mutex mutex_;
  std::deque<entry> entries_;
  options options_;
  unsigned long long next_index_;
  unsigned long long dropped_;
};

typedef std::shared_ptr<failed_check_store> failed_check_store_instance;

}  // namespace core
}  // namespace nsclient
