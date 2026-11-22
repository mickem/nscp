// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <NSCAPI.h>

#include <atomic>
#include <map>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <str/utf8.hpp>
#include <str/utils_no_boost.hpp>
#include <utility>

namespace parsers {
namespace where {
template <class runtime_data, class config_object>
struct realtime_filter_helper {
  nscapi::core_wrapper *core;
  int plugin_id;
  // Number of process_startup() rounds which still left something pending,
  // bounding the retries (see max_startup_attempts).
  int startup_attempts;
  realtime_filter_helper(nscapi::core_wrapper *core, const int plugin_id) : core(core), plugin_id(plugin_id), startup_attempts(0) {}

  // How many process_startup() rounds may fail before the remaining `run on
  // startup` submissions are abandoned. The rounds are driven by the module's
  // real-time loop (sub-second to a few seconds apart), so this covers a slow
  // boot without retrying a genuinely broken destination forever.
  static constexpr int max_startup_attempts = 20;

  typedef typename runtime_data::filter_type filter_type;
  typedef typename runtime_data::transient_data_type transient_data_type;
  typedef boost::optional<boost::posix_time::time_duration> op_duration;
  struct container {
   private:
    std::string alias;
    std::string event_name;
    std::string target;
    std::string target_id;
    std::string source_id;

   public:
    std::string command;
    std::string timeout_msg;
    NSCAPI::nagiosReturn severity;
    boost::optional<boost::posix_time::time_duration> max_age;
    boost::optional<boost::posix_time::time_duration> silent_period;

    // should be private...
    filter_type filter;

   private:
    boost::posix_time::ptime next_ok_;
    boost::posix_time::ptime next_alert_;
    // The evaluation failure last reported, and when the same one may be
    // reported again (see should_report_failure).
    std::string last_failure_;
    boost::posix_time::ptime next_failure_report_;

   public:
    bool debug;
    bool escape_html;
    // Still needs its `run on startup` priming submitted, see
    // process_startup(). Only touched from the module's real-time thread.
    bool startup_pending;
    runtime_data data;
    std::atomic<long long> match_count;
    std::atomic<long long> error_count;
    // Set when build_filters fails during init. The container is still kept
    // in `items` so its error_count surfaces in get_counts() — without that
    // a filter that never started would simply be invisible in metrics. All
    // runtime paths skip broken items so no half-built filter ever runs.
    bool broken;

    container(const std::string &alias, std::string event_name, runtime_data data)
        : alias(alias),
          event_name(std::move(event_name)),
          command(alias),
          severity(0),
          debug(false),
          escape_html(false),
          startup_pending(false),
          data(data),
          match_count(0),
          error_count(0),
          broken(false) {}

    void set_target(const std::string &new_target, const std::string &new_target_id, const std::string &new_source_id) {
      target = new_target;
      target_id = new_target_id;
      source_id = new_source_id;
    }
    bool is_event() const { return target == "event"; }
    bool is_events() const { return target == "events"; }

    std::string get_event_name() const { return event_name; }
    std::string get_alias() const { return alias; }
    std::string get_target() const { return target; }
    std::string get_target_id() const { return target_id; }
    std::string get_source_id() const { return source_id; }

    bool build_filters(nscapi::settings_filters::filter_object config, std::string &error) {
      std::string message;
      // Same list rendering as a queried check, so the same knob: a real-time
      // filter reports to the same frontend and gains as much from one item
      // per line. Set before the first match is recorded; an unset value means
      // "inherit" here (as it does for every other filter_object string), so
      // the summary keeps its own ", " default.
      if (!config.list_separator.empty()) filter.summary.list_separator = str::utils::unescape(config.list_separator);
      // Same story for the number format (#1428): set before the first row is
      // rendered, and left alone entirely while nothing has been configured so
      // an untouched filter renders exactly what it always did.
      const std::string byte_unit_error = config.invalid_byte_unit();
      if (!byte_unit_error.empty()) {
        // A one-shot query would be rejected here; a real-time filter keeps
        // running (auto-scaling the value) so a formatting typo cannot take
        // down persistent monitoring, but the misconfiguration is logged.
        NSC_LOG_ERROR(byte_unit_error + " in real-time filter '" + alias + "'; falling back to auto-scaling");
      }
      const str::number_format number_format = config.number_format();
      if (!number_format.is_default()) {
        filter.context->set_number_format(number_format);
        filter.set_human_number_format(true);
      }
      if (!filter.build_syntax(config.debug, config.syntax_top, config.syntax_detail, config.perf_data, config.perf_config, config.syntax_ok,
                               config.syntax_empty)) {
        error = "Failed to build strings " + alias;
        return false;
      }
      if (!filter.build_engines(config.debug, config.filter_string(), config.filter_ok, config.filter_warn, config.filter_crit)) {
        error = "Failed to build filters: " + alias;
        return false;
      }

      if (!filter.validate(message)) {
        error = "Failed to validate filter for " + alias + ": " + message;
        return false;
      }
      return true;
    }

    bool is_silent(const boost::posix_time::ptime &now) const {
      if (!silent_period) {
        return false;
      }
      return now < next_alert_;
    }

    void touch(const boost::posix_time::ptime &now, bool alert) {
      if (max_age) next_ok_ = now + (max_age.value());
      if (alert && silent_period)
        next_alert_ = now + (silent_period.value());
      else if (silent_period)
        next_alert_ = now;
      data.touch(now);
    }

    bool has_timedout(const boost::posix_time::ptime &now) const { return max_age && next_ok_ <= now; }

    // A filter that fails on every record - a regular expression that every
    // line is too long for, a unit the keyword does not take - fails on every
    // batch, which for the CheckSystem filters is every second. The failure is
    // counted every time, but reported (logged, and submitted as UNKNOWN when
    // the batch has nothing else to say) only when it is new, or once per
    // interval while it lasts: the silent period when one is set, else
    // failure_report_interval. A batch with records that matched is always
    // submitted; only its log line follows this.
    static boost::posix_time::time_duration failure_report_interval() { return boost::posix_time::minutes(5); }
    // What makes two failures the same one: the text with every run of digits
    // folded, since a failure can carry a number that differs from record to
    // record - the regular-expression refusal names the size of the line it
    // refused, and oversized lines of differing length are still one failure.
    static std::string failure_key(const std::string &failure) {
      std::string key;
      bool in_digits = false;
      for (const char c : failure) {
        const bool digit = c >= '0' && c <= '9';
        if (digit && !in_digits) key += '#';
        if (!digit) key += c;
        in_digits = digit;
      }
      return key;
    }
    bool should_report_failure(const std::string &failure, const boost::posix_time::ptime &now) {
      const std::string key = failure_key(failure);
      if (key == last_failure_ && now < next_failure_report_) return false;
      last_failure_ = key;
      next_failure_report_ = now + (silent_period ? silent_period.value() : failure_report_interval());
      return true;
    }
    // A batch that evaluated cleanly: the next failure is new again.
    void clear_failure() { last_failure_.clear(); }

    bool find_minimum_timeout(boost::optional<boost::posix_time::ptime> &minNext) const {
      if (!max_age) return false;
      if (!minNext)  // No value yes, lest assign ours.
        minNext = next_ok_;
      if (next_ok_ > minNext.value())  // Our value is not interesting: lets ignore us
        return false;
      minNext = next_ok_;
      return true;
    }
  };

  typedef std::shared_ptr<container> container_type;

  std::list<container_type> items;

  bool add_item(const std::shared_ptr<config_object> object, const runtime_data &source_data, const std::string &event_name) {
    container_type item(new container(object->get_alias(), event_name, source_data));
    item->set_target(object->filter.target, object->filter.target_id, object->filter.source_id);
    item->timeout_msg = object->filter.timeout_msg;
    item->severity = object->filter.severity;
    item->max_age = object->filter.max_age;
    item->silent_period = object->filter.silent_period;
    item->debug = object->filter.debug;
    item->escape_html = object->filter.escape_html;
    item->startup_pending = object->filter.run_on_startup;
    if (!object->filter.command.empty()) item->command = object->filter.command;
    std::string message;

    if (!item->build_filters(object->filter, message)) {
      NSC_LOG_ERROR(message);
      // Keep the failed filter in `items` so its alias still appears in
      // get_counts() with a non-zero `.errors` value — this is how an
      // operator notices that a configured filter never came up. The
      // boolean guard makes every runtime path treat the entry as inert.
      item->broken = true;
      item->error_count.fetch_add(1, std::memory_order_relaxed);
      items.push_back(item);
      return false;
    }

    item->data.boot();
    items.push_back(item);
    return true;
  }

  void process_timeout(const container_type item) {
    std::string response;
    nscapi::core_helper ch(core, plugin_id);
    if (!ch.submit_simple_message(item->get_target(), item->get_source_id(), item->get_target_id(), item->command, NSCAPI::query_return_codes::returnOK,
                                  item->timeout_msg, "", response)) {
      item->error_count.fetch_add(1, std::memory_order_relaxed);
      NSC_LOG_ERROR("Failed to submit result: " + response);
    }
  }

  // Runs one batch through the item's filter and sends what it amounts to.
  // True when an alert went out for records that matched - which is what
  // starts the silent period. A failure on its own may still be submitted
  // (as UNKNOWN) but is no alert: a silent period it started would suppress
  // the matching line that arrives right after the one that failed.
  bool process_item(container_type item, transient_data_type data, const bool is_silent,
                    const boost::posix_time::ptime &now = boost::posix_time::second_clock::local_time()) {
    std::string response;
    try {
      if (item->is_events() || item->is_event()) {
        item->filter.fetch_hash(true);
      }
      item->filter.start_match();
      if (item->severity != -1) item->filter.summary.returnCode = item->severity;

      const modern_filter::match_result result = item->data.process_item(item->filter, data);
      // Evaluate deferred warn/crit records and post-iteration summary
      // expressions so the summary state reflects the final verdict before we
      // act on it. This mirrors the cli_helper::post_process flow used by the
      // active check path.
      item->filter.match_post();
      // A record whose evaluation failed (a regular expression refused by the
      // budget or the subject limit, a unit the keyword does not take) got no
      // verdict at all: modern_filters neither matched it nor dropped it as a
      // non-match. The rest of the batch - for a log file, every line written
      // since the last read - is still judged, so an oversized line written
      // beside a matching one cannot hide it.
      bool failed = item->filter.has_evaluation_errors();
      if (!result.matched_filter && !failed) {
        item->clear_failure();
        return false;
      }
      if (result.matched_filter) item->match_count.fetch_add(1, std::memory_order_relaxed);

      // A status is rendered before it is judged: the top syntax can fail too
      // (a template function), and that failure belongs in this verdict.
      const bool is_status = !item->is_event() && !item->is_events();
      std::string rendered;
      if (is_status && result.matched_filter && !is_silent) {
        rendered = item->filter.get_message();
        failed = item->filter.has_evaluation_errors();
      }
      bool report_failure = false;
      if (failed) {
        item->error_count.fetch_add(1, std::memory_order_relaxed);
        report_failure = item->should_report_failure(item->filter.get_evaluation_failure(), now);
        if (report_failure) NSC_LOG_ERROR("Realtime filter '" + item->get_alias() + "': " + item->filter.get_evaluation_failure());
      } else {
        item->clear_failure();
      }

      if (is_silent) {
        NSC_TRACE_MSG("Eventlog filter is silenced " + item->get_alias());
        // Only what matched counts as an alert: a failure alone sent nothing.
        return result.matched_filter;
      }

      nscapi::core_helper ch(core, plugin_id);
      // An event channel carries records, not a status: it gets the records
      // that matched, and the failure stays in the log and the error count.
      if (item->is_event()) {
        if (!result.matched_filter) return false;
        typedef std::list<std::map<std::string, std::string> > list_type;
        typedef std::map<std::string, std::string> hash_type;

        list_type keys = item->filter.records_;
        for (hash_type &bundle : keys) {
          if (!ch.emit_event(item->get_event_name(), item->get_alias(), bundle, response)) {
            item->error_count.fetch_add(1, std::memory_order_relaxed);
            NSC_LOG_ERROR("Failed to submit '" + response);
          }
        }
        return true;
      }
      if (item->is_events()) {
        if (!result.matched_filter) return false;
        const std::list<std::map<std::string, std::string> > keys = item->filter.records_;
        if (!ch.emit_event(item->get_event_name(), item->get_alias(), keys, response)) {
          item->error_count.fetch_add(1, std::memory_order_relaxed);
          NSC_LOG_ERROR("Failed to submit '" + response);
        }
        return true;
      }

      // A status: the verdict the matched records reached, as for an active
      // check - a WARNING or CRITICAL stands with the failure beside it, and
      // anything else (including a batch where nothing matched) is UNKNOWN.
      int code = item->filter.summary.returnCode;
      const bool alert = result.matched_filter && (code == NSCAPI::query_return_codes::returnWARN || code == NSCAPI::query_return_codes::returnCRIT);
      std::string message;
      if (failed && !alert) {
        // UNKNOWN - and not an alert, so no silent period. When records
        // matched it is submitted every time with what they rendered, the
        // failure beside it: throttling it would drop them. When nothing
        // matched the failure is all there is to say: said once, then once
        // per interval while it lasts.
        code = NSCAPI::query_return_codes::returnUNKNOWN;
        if (result.matched_filter) {
          message = item->filter.with_evaluation_failure(rendered.empty() ? "Nothing matched" : rendered);
        } else {
          if (!report_failure) return false;
          message = item->filter.get_evaluation_failure();
        }
      } else {
        message = rendered;
        if (message.empty()) message = "Nothing matched";
        if (failed) message = item->filter.with_evaluation_failure(message);
      }
      if (!ch.submit_simple_message(item->get_target(), item->get_source_id(), item->get_target_id(), item->command, code, message, "", response)) {
        item->error_count.fetch_add(1, std::memory_order_relaxed);
        NSC_LOG_ERROR("Failed to submit '" + message);
      }
      return result.matched_filter && !(failed && !alert);
    } catch (const nsclient::nsclient_exception &e) {
      item->error_count.fetch_add(1, std::memory_order_relaxed);
      NSC_DEBUG_MSG("Realtime filter '" + item->get_alias() + "' failed: " + e.reason());
    } catch (const std::exception &e) {
      item->error_count.fetch_add(1, std::memory_order_relaxed);
      NSC_DEBUG_MSG("Realtime filter '" + item->get_alias() + "' failed: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      item->error_count.fetch_add(1, std::memory_order_relaxed);
      NSC_DEBUG_MSG("Realtime filter '" + item->get_alias() + "' failed");
    }
    return false;
  }

  void touch_all() {
    boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
    for (container_type item : items) {
      if (item->broken) continue;
      item->touch(current_time, false);
    }
  }

  // Whether any filter still has a `run on startup` submission to deliver.
  // Lets the module's real-time loop shorten its wait only while priming is
  // actually outstanding.
  bool has_pending_startup() const {
    for (const container_type &item : items) {
      if (!item->broken && item->startup_pending) return true;
    }
    return false;
  }

  // Prime the destination for every filter which opted in via `run on
  // startup` (#584): submit the ok/empty message right away instead of
  // leaving the destination (e.g. the cache behind check_cache) without an
  // entry until the first data change or `maximum age` expiry. At boot no
  // data has been seen yet, which is exactly the state the `maximum age`
  // timeout reports, so the same message is submitted.
  //
  // Returns true once nothing is pending any more. A submission can fail
  // because the destination module has not registered its channel yet - the
  // real-time thread starts while later plugins are still loading (the same
  // ordering problem the Scheduler solves with startModule) - so a failed
  // attempt stays pending, to be retried by the caller's loop, and is only
  // logged as an error once the attempts are exhausted (a destination which
  // is still missing then is a real misconfiguration, which the `maximum
  // age` path would keep reporting too).
  bool process_startup() {
    const boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
    bool pending = false;
    nscapi::core_helper ch(core, plugin_id);
    for (const container_type &item : items) {
      if (item->broken || !item->startup_pending) continue;
      std::string response;
      if (ch.submit_simple_message(item->get_target(), item->get_source_id(), item->get_target_id(), item->command, NSCAPI::query_return_codes::returnOK,
                                   item->timeout_msg, "", response)) {
        item->startup_pending = false;
        item->touch(current_time, false);
      } else if (startup_attempts + 1 >= max_startup_attempts) {
        item->error_count.fetch_add(1, std::memory_order_relaxed);
        item->startup_pending = false;
        NSC_LOG_ERROR("Failed to submit startup message for '" + item->get_alias() + "', giving up: " + response);
      } else {
        pending = true;
        NSC_DEBUG_MSG("Startup message for '" + item->get_alias() + "' could not be delivered yet, will retry: " + response);
      }
    }
    if (pending) startup_attempts++;
    return !pending;
  }

  void process_items(transient_data_type data) {
    try {
      boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
      bool has_matched = false;
      bool has_changed = false;
      // Process all items matching this event
      if (items.size() == 0) {
        NSC_TRACE_MSG("No filters to check for: " + data->to_string());
      }
      for (container_type item : items) {
        if (item->broken) continue;
        if (item->data.has_changed(data)) {
          has_changed = true;
          if (process_item(item, data, item->is_silent(current_time), current_time)) {
            has_matched = true;
            item->touch(current_time, true);
          }
        }
      }
      if (!has_changed) {
        NSC_TRACE_MSG("No filters changes detected: " + data->to_string());
      } else if (!has_matched) {
        NSC_TRACE_MSG("No filters matched: " + data->to_string());
      }
      do_process_no_items(current_time);
    } catch (const nsclient::nsclient_exception &e) {
      bump_all_errors();
      NSC_DEBUG_MSG("Realtime processing faillure: " + e.reason());
    } catch (const std::exception &e) {
      bump_all_errors();
      NSC_DEBUG_MSG("Realtime processing faillure: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      bump_all_errors();
      NSC_DEBUG_MSG("Realtime processing faillure");
    }
  }

  void do_process_no_items(boost::posix_time::ptime current_time) {
    try {
      // Match any stale items and process timeouts
      for (container_type item : items) {
        if (item->broken) continue;
        if (item->has_timedout(current_time)) {
          process_timeout(item);
          item->touch(current_time, false);
        }
      }
    } catch (...) {
      bump_all_errors();
      NSC_DEBUG_MSG("Realtime processing faillure");
    }
  }

  // Failures observed outside any specific item (e.g. exception thrown
  // while iterating items / setting up transient data) cannot be attributed
  // to a single filter, so they get charged to every configured item. This
  // keeps `.errors` honest as "did this collection cycle complete cleanly"
  // rather than silently dropping the failure. Broken items are skipped:
  // their error count is already non-zero from the build failure, and
  // stacking cycle errors on top would mask whether new failures are
  // accumulating in the surviving filters.
  void bump_all_errors() {
    for (const container_type &item : items) {
      if (item->broken) continue;
      item->error_count.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void process_no_items() { do_process_no_items(boost::posix_time::second_clock::local_time()); }

  std::map<std::string, long long> get_counts() const {
    std::map<std::string, long long> ret;
    for (const container_type &item : items) {
      ret[item->get_alias() + ".fired"] = item->match_count.load(std::memory_order_relaxed);
      ret[item->get_alias() + ".errors"] = item->error_count.load(std::memory_order_relaxed);
    }
    return ret;
  }

  op_duration find_minimum_timeout() {
    op_duration ret;
    boost::posix_time::ptime current_time = boost::posix_time::second_clock::local_time();
    boost::optional<boost::posix_time::ptime> minNext;
    for (const container_type &item : items) {
      if (item->broken) continue;
      item->find_minimum_timeout(minNext);
    }

    if (!minNext) {
      NSC_TRACE_MSG("Next miss time is in: no timeout specified");
    } else {
      boost::posix_time::time_duration dur = minNext.value() - current_time;
      if (dur.total_seconds() <= 0) {
        NSC_LOG_ERROR("Invalid duration for eventlog check, assuming all values stale");
        touch_all();
        minNext = boost::none;
        for (const container_type &item : items) {
          if (item->broken) continue;
          item->find_minimum_timeout(minNext);
        }
        dur = minNext.value() - current_time;
        if (dur.total_seconds() <= 0) {
          NSC_LOG_ERROR("Something is fishy with your periods, returning 30 seconds...");
          dur = boost::posix_time::time_duration(0, 0, 30, 0);
        }
      }
      NSC_TRACE_MSG("Next miss time is in: " + str::xtos(dur.total_seconds()) + "s");
      ret = dur;
    }
    return ret;
  }
};
}  // namespace where
}  // namespace parsers
