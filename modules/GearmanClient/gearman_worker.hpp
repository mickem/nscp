// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread.hpp>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "gearman_connection.hpp"
#include "gearman_crypt.hpp"
#include "gearman_job.hpp"

/**
 * The Mod-Gearman worker loop.
 *
 * One thread per worker, each owning its own connection: connect, announce
 * the queues, then ask for a job, run it, answer, repeat. gearmand hands a
 * job to whichever registered worker asks first, so two workers on the same
 * queue share the load with no coordination between them and nothing here
 * has to know about the others.
 *
 * Everything the loop needs from the rest of the agent arrives through two
 * interfaces - `query_executor` runs a check, `worker_logger` writes a log
 * line - so the whole loop can be driven against a scripted fake gearmand in
 * a unit test without a core, a socket to a real server, or a check that
 * touches the machine.
 */
namespace gearman {

/** The outcome of running one check. */
struct query_result {
  /** A Nagios status: 0 OK, 1 warning, 2 critical, 3 unknown. */
  int return_code = 3;
  /** One line, `message|perfdata`, with real newlines; the worker escapes them. */
  std::string output;
  /**
   * The check did not finish inside the job's timeout. The worker, not the
   * executor, decides what a timeout reports, so the other two fields are
   * ignored when this is set.
   */
  bool timed_out = false;
};

class query_executor {
 public:
  virtual ~query_executor() = default;
  /**
   * Run `command` and return its result, taking no longer than
   * `timeout_seconds`. An implementation that cannot stop the check must
   * still return on time with `timed_out` set and let the overrun finish on
   * its own.
   */
  virtual query_result execute(const std::string &command, const std::list<std::string> &arguments, unsigned int timeout_seconds) = 0;
};

class worker_logger {
 public:
  virtual ~worker_logger() = default;
  virtual void error(const std::string &message) = 0;
  virtual void warning(const std::string &message) = 0;
  virtual void info(const std::string &message) = 0;
  virtual void debug(const std::string &message) = 0;
};

struct worker_config {
  /** Tried in order; a failed connection moves on to the next one. */
  std::vector<server_address> servers;
  /** Encryption switch and shared key. */
  envelope crypto;
  /** Fully qualified queue names: `hostgroup_windows`, `servicegroup_db`, `host`, `service`. */
  std::vector<std::string> queues;

  /**
   * Agent mode: the names this agent answers for, lower-cased - the OS
   * host name plus whatever `host names` adds. A job for anything else is
   * refused rather than executed, because a queue carries the checks of
   * every host in its group and nothing in the protocol says which worker a
   * job was meant for.
   */
  std::set<std::string> host_names;
  /** False in proxy mode, where the command line names its own target. */
  bool bind_to_host = true;

  unsigned int workers = 2;
  /** Nagios status reported when a check overruns the job's timeout. */
  int timeout_return = 2;
  /** Refuse a job whose `core_time` is older than this many seconds; 0 disables. */
  long long max_age = 0;
  /** The core has already expanded `$ARGn$`, so a job is nothing but arguments. */
  bool allow_arguments = true;
  bool allow_nasty_characters = false;

  unsigned int connect_timeout = 30;
  /** How long to wait for gearmand between packets, and after `PRE_SLEEP`. */
  unsigned int idle_timeout = 30;
  /** Bound for a job that declares no timeout; both NEB modules always set one. */
  unsigned int default_job_timeout = 60;

  /** `nscp-<host>`; each worker appends its own number for `gearman_top`. */
  std::string client_id = "nscp";
  /** The result's `source` line, e.g. `NSClient++ 0.19.0 on win-srv01`. */
  std::string source = "NSClient++";
};

/**
 * What the pool has done so far. Read from any thread; the values are only
 * ever incremented by the worker that owns the event, so they are plain
 * relaxed counters and not a consistent snapshot of each other.
 */
struct worker_counters {
  /** Jobs decoded off a queue, whether or not the check itself ran. */
  std::atomic<long long> jobs{0};
  /** Connection failures, undecodable payloads and results that could not be submitted. */
  std::atomic<long long> errors{0};
  /** Workers currently holding a live socket. */
  std::atomic<long long> connected{0};
  /** When the last job was grabbed, as seconds since the epoch; 0 for never. */
  std::atomic<long long> last_job_time{0};
};

/** One thread, one connection, one `GRAB_JOB` loop. */
class worker {
 public:
  worker(std::string id, const worker_config &config, std::shared_ptr<query_executor> executor, std::shared_ptr<worker_logger> logger,
         std::shared_ptr<worker_counters> counters);

  /** Runs until `stop()` is called. Never throws. */
  void run();
  void stop();

 private:
  /** Nagios statuses, so the loop does not have to depend on the plugin API. */
  enum nagios_status { nagios_ok = 0, nagios_warning = 1, nagios_critical = 2, nagios_unknown = 3 };

  bool stopping() const { return stop_.load(); }
  /** Sleep in slices so a shutdown is not held up by a backoff. */
  void sleep_for(unsigned int milliseconds);

  void connect();
  void disconnect();
  /** One `GRAB_JOB` round: ask, then act on whatever comes back. */
  void pump();
  void handle_job(const packet &assignment);

  /** True when the job's host is one this agent answers for. */
  bool answers_for(const std::string &host_name) const;
  /** True when `core_time` is further in the past than `max age`. */
  bool too_old(const check_job &job) const;

  /** Build, submit and complete the answer to one job. */
  void answer(const check_job &job, int return_code, const std::string &output, const std::string &start_time, const std::string &handle);
  /**
   * Put the result on its queue, retrying once on a fresh connection - but
   * only while the first attempt is known not to have reached gearmand.
   */
  void submit(const std::string &queue, const std::string &payload);

  const std::string id_;
  const worker_config config_;
  /**
   * `config_.host_names` as one line, for the message a refused job carries.
   * Built once: the set does not change for the life of a worker, and a core
   * pointed at the wrong agent refuses every check it is sent.
   */
  const std::string host_names_text_;
  const std::shared_ptr<query_executor> executor_;
  const std::shared_ptr<worker_logger> logger_;
  /**
   * Shared, not a reference to the pool's member: a worker that outruns the
   * pool's bounded join keeps running after the pool is gone, and it must
   * still have somewhere valid to count.
   */
  const std::shared_ptr<worker_counters> counters_;

  std::atomic<bool> stop_{false};
  std::unique_ptr<connection> connection_;
  /** Whether this worker is currently counted in `worker_counters::connected`. */
  bool registered_ = false;
  /** Index into `config_.servers`; advanced on every reconnect. */
  std::size_t server_index_ = 0;
  /** The server the current (or most recent) connection went to. */
  server_address current_server_;
  /** Consecutive failed connects, for the backoff. */
  unsigned int failures_ = 0;
};

/**
 * The worker threads, started and stopped as a unit.
 *
 * A settings reload calls `loadModuleEx` again on the live module while these
 * threads are running, so the pool must be stopped before a new one is
 * started or every reload doubles the number of workers on the queue.
 */
class worker_pool {
 public:
  worker_pool() = default;
  ~worker_pool();

  worker_pool(const worker_pool &) = delete;
  worker_pool &operator=(const worker_pool &) = delete;

  void start(const worker_config &config, const std::shared_ptr<query_executor> &executor, const std::shared_ptr<worker_logger> &logger);
  /**
   * Signal every worker and join with a bounded wait. Returns false when a
   * thread did not come back in time; it is then parked rather than joined,
   * the way CheckHelpers parks an overrunning check_timeout worker, so an
   * unloading module is never blocked by a check that will not end.
   */
  bool stop(unsigned int timeout_seconds = 30);

  bool is_running() const { return !workers_.empty(); }
  const worker_counters &counters() const { return *counters_; }

 private:
  std::vector<std::shared_ptr<worker>> workers_;
  std::vector<std::shared_ptr<boost::thread>> threads_;
  /** Threads that outlived their bounded join; waited on again at destruction. */
  std::list<std::shared_ptr<boost::thread>> parked_;
  std::shared_ptr<worker_counters> counters_ = std::make_shared<worker_counters>();
};

}  // namespace gearman
