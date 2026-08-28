// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <scheduler/simple_scheduler.hpp>

#include "schedules_handler.hpp"

typedef schedules::schedule_handler::object_instance schedule_instance;
class Scheduler : public schedules::task_handler, public nscapi::impl::simple_plugin {
 private:
  schedules::scheduler scheduler_;
  schedules::schedule_handler schedules_;
  // Window over which "run on startup" schedules are spread, see the
  // "startup window" setting.
  boost::posix_time::time_duration startup_window_;
  // True once startModule has run, i.e. once every other plugin is loaded and
  // startup runs are safe to fire. Reloads happen after that point and are
  // never followed by another startModule (the core calls it once per plugin
  // lifetime), so loadModuleEx has to fire the startup runs itself when set.
  bool started_;

 public:
  Scheduler() : startup_window_(boost::posix_time::seconds(0)), started_(false) { scheduler_.set_handler(this); }
  virtual ~Scheduler() { scheduler_.set_handler(nullptr); }
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool startModule();
  void prepareShutdown();
  bool unloadModule();

  // Metrics
  void fetchMetrics(PB::Metrics::MetricsMessage_Response* response);

  void add_schedule(const std::string& alias, const std::string& command);
  void set_startup_window(const std::string& value);
  bool handle_schedule(schedules::target_object task);

  // Runs schedules on demand instead of waiting for their interval, see
  // docs/samples/Scheduler_run_schedules_desc.md.
  void run_schedules(const PB::Commands::QueryRequestMessage::Request& request, PB::Commands::QueryResponseMessage::Response* response,
                     const PB::Commands::QueryRequestMessage& request_message);

  void on_error(const char* file, int line, std::string error);
  void on_trace(const char* file, int line, std::string error);

 private:
  // The caller identity forwarded to the checks a schedule runs. Empty for the
  // scheduler's own timer driven runs (which are then attributed to Scheduler
  // itself, as before); set to the original caller when run_schedules is
  // invoked from the outside so the permission check sees the human, not us.
  struct forwarded_identity {
    std::string caller_plugin_id;
    std::string principal;
  };
  static forwarded_identity extract_identity(const PB::Commands::QueryRequestMessage& request_message);

  // Runs one schedule: execute its command, filter on the report mode and
  // submit the result on its channel. `error` describes what went wrong when
  // it returns false. Shared by the timer path (handle_schedule) and the
  // on-demand path (run_schedules).
  bool run_schedule(const schedules::target_object& item, const forwarded_identity& id, std::string& error);
};