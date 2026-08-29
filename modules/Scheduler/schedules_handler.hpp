// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/optional.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/object.hpp>
#include <parsers/cron/cron_parser.hpp>
#include <scheduler/simple_scheduler.hpp>
#include <str/format.hpp>
#include <str/utils.hpp>
#include <string>

namespace sh = nscapi::settings_helper;

namespace schedules {
struct schedule_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  schedule_object(std::string alias, std::string path) : parent(alias, path), randomness(0.0), run_on_startup(false), report(0), id(0) {}
  schedule_object(const schedule_object& other)
      : parent(other),
        source_id(other.source_id),
        target_id(other.target_id),
        duration(other.duration),
        randomness(other.randomness),
        schedule(other.schedule),
        run_on_startup(other.run_on_startup),
        channel(other.channel),
        report(other.report),
        command(other.command),
        arguments(other.arguments),
        // Not copying this left it indeterminate, which to_string then printed
        // as a random schedule id in the "Adding scheduled item" log line.
        id(other.id) {}

  // Schedule keys
  std::string source_id;
  std::string target_id;
  boost::optional<boost::posix_time::time_duration> duration;
  double randomness;
  boost::optional<std::string> schedule;
  bool run_on_startup;
  std::string channel;
  unsigned int report;
  std::string command;
  std::list<std::string> arguments;

  // Others keys (Managed by application)
  int id;

  void set_randomness(std::string str) { randomness = str::stox<double>(boost::replace_first_copy(str, "%", "")) / 100.0; }
  void set_report(std::string str) { report = nscapi::report::parse(str); }
  void set_duration(std::string str) { duration = boost::posix_time::seconds(str::format::stox_as_time_sec<long>(str, "s")); }
  void set_schedule(std::string str) { schedule = str; }
  void set_command(std::string str) {
    if (!str.empty()) {
      arguments.clear();
      str::utils::parse_command(str, command, arguments);
    }
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << get_alias() << "[" << id << "] = "
       << "{tpl: " << parent::to_string() << ", command: " << command << ", channel: " << channel << ", source_id: " << source_id
       << ", target_id: " << target_id;
    if (duration) {
      ss << ", duration: " << (*duration).total_seconds() << "s, " << (randomness * 100) << "% randomness";
    }
    if (schedule) ss << ", schedule: " << *schedule;
    if (run_on_startup) ss << ", run on startup";
    ss << "}";
    return ss.str();
  }

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    set_command(get_value());
    bool is_def = is_default();

    nscapi::settings_helper::settings_registry settings(proxy);
    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();
    if (oneliner) return;

    root_path.add_path()("SCHEDULE DEFINITION", "Schedule definition for: " + get_alias());

    root_path
        .add_key()

        .add_string("command", sh::string_fun_key([this](auto value) { this->set_command(value); }), "SCHEDULE COMMAND", "Command to execute", is_def)

        .add_string("target", sh::string_key(&target_id), "TARGET", "The target to send the message to (will be resolved by the consumer)", true)
        .add_string("source", sh::string_key(&source_id), "SOURCE",
                    "The name of the source system, will automatically use the remote system if a remote system is called. Almost most sending systems will "
                    "replace this with current systems hostname if not present. So use this only if you need specific source systems for specific schedules "
                    "and not calling remote systems.",
                    true)

        ;
    if (is_def) {
      root_path
          .add_key()

          .add_string("channel", sh::string_key(&channel, "NSCA"), "SCHEDULE CHANNEL", "Channel to send results on, set to drop to ignore results")

          .add_string("interval", sh::string_fun_key([this](auto value) { this->set_duration(value); }), "SCHEDULE INTERVAL",
                      "Time in seconds between each check")

          .add_string("randomness", sh::string_fun_key([this](auto value) { this->set_randomness(value); }), "RANDOMNESS",
                      "% of the interval which should be random to prevent overloading server resources")

          .add_string("schedule", sh::string_fun_key([this](auto value) { this->set_schedule(value); }), "SCHEDULE",
                      "Cron-like statement for when a task is run. Currently limited to only one number i.e. 1 * * * * or * * 1 * * but not 1 1 * * *")

          .add_bool("run on startup", sh::bool_key(&run_on_startup, false), "RUN ON STARTUP",
                    "Run the command once as soon as NSClient++ has started (and after each reload) instead of waiting for the first interval or schedule to "
                    "elapse. Use this to avoid stale results after a reboot when the interval is long. The regular schedule is unaffected and continues from "
                    "the startup run.")

          .add_string("report", sh::string_fun_key([this](auto value) { this->set_report(value); }, "all"), "REPORT MODE",
                      "What to report to the server (any of the following: all, critical, warning, unknown, ok)")

          ;
    } else {
      root_path.add_key()
          .add_string("channel", sh::string_key(&channel), "SCHEDULE CHANNEL", "Channel to send results on")

          .add_string("interval", sh::string_fun_key([this](auto value) { this->set_duration(value); }), "SCHEDULE INTERVAL",
                      "Time in seconds between each check", true)

          .add_string("randomness", sh::string_fun_key([this](auto value) { this->set_randomness(value); }), "RANDOMNESS",
                      "% of the interval which should be random to prevent overloading server resources")

          .add_string("schedule", sh::string_fun_key([this](auto value) { this->set_schedule(value); }), "SCHEDULE",
                      "Cron-like statement for when a task is run. Currently limited to only one number i.e. 1 * * * * or * * 1 * * but not 1 1 * * *")

          .add_bool("run on startup", sh::bool_key(&run_on_startup), "RUN ON STARTUP",
                    "Run the command once as soon as NSClient++ has started (and after each reload) instead of waiting for the first interval or schedule to "
                    "elapse. Inherited from the default schedule unless set here.",
                    true)

          .add_string("report", sh::string_fun_key([this](auto value) { this->set_report(value); }), "REPORT MODE",
                      "What to report to the server (any of the following: all, critical, warning, unknown, ok)", true)

          ;
    }

    settings.register_all();
    settings.notify();
  }
};

typedef boost::optional<schedule_object> optional_target_object;
typedef std::shared_ptr<schedule_object> target_object;

typedef nscapi::settings_objects::object_handler<schedule_object> schedule_handler;

struct task_handler {
  virtual bool handle_schedule(target_object task) = 0;
  virtual void on_error(const char* file, int line, std::string error) = 0;
  virtual void on_trace(const char* file, int line, std::string error) = 0;
};

struct scheduler : public simple_scheduler::handler {
  typedef boost::unordered_map<int, target_object> metadata_map;
  metadata_map metadata;
  simple_scheduler::scheduler tasks;
  // Explicitly initialised: a default-constructed std::atomic holds an
  // indeterminate value before C++20, and this struct has no constructor
  // that would otherwise set it before the first load().
  std::atomic<task_handler*> handler_{nullptr};

  target_object get(int id);
  // Snapshot of every schedule which was successfully added (i.e. what the
  // scheduler will actually run - schedules rejected at load time for a bad
  // interval are not in here). Returns a copy so callers can iterate without
  // holding the task mutex.
  std::list<target_object> get_all();

  void start();
  void stop();

  simple_scheduler::scheduler& get_scheduler() { return tasks; }

  void set_handler(task_handler* handler) { handler_ = handler; }
  void prepare_shutdown() { tasks.prepare_shutdown(); }
  void unset_handler() { handler_ = nullptr; }
  void clear();

  void set_threads(int count) { tasks.set_threads(count); }
  void set_timezone(const std::string& tz) { tasks.set_timezone(tz); }

  void add_task(const target_object target);
  // Queue the first run of every "run on startup" schedule. add_task leaves
  // those dormant, so this is what actually brings them to life - it must be
  // called once the agent is up (see Scheduler::startModule). The runs are
  // spread evenly over `window` to avoid hitting the monitoring server with
  // every passive check at once; a zero window runs them all immediately.
  void run_startup_tasks(boost::posix_time::time_duration window);

  bool handle_schedule(simple_scheduler::task item) {
    task_handler* tmp = handler_.load();
    if (tmp) {
      if (!tmp->handle_schedule(get(item.id))) tasks.remove_task(item.id);
    }
    return true;
  }
  void on_error(const char* file, int line, std::string error) {
    task_handler* tmp = handler_.load();
    if (tmp) tmp->on_error(file, line, error);
  }
  void on_trace(const char* file, int line, std::string error) {
    task_handler* tmp = handler_.load();
    if (tmp) tmp->on_trace(file, line, error);
  }
};

}  // namespace schedules