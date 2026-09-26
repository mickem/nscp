// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "gearman_worker.hpp"

#include <config.h>

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <chrono>
#include <ctime>
#include <str/utils.hpp>
#include <thread>
#include <threads/guarded_thread.hpp>

namespace gearman {

namespace {

/** Reconnect delays: 1s, 2s, 4s ... capped, so a gearmand that is down is not hammered. */
const unsigned int backoff_base_ms = 1000;
const unsigned int backoff_cap_ms = 60000;

/** Granularity of an interruptible sleep: how long a shutdown can be held up by a backoff. */
const unsigned int sleep_slice_ms = 100;

unsigned int backoff_ms(const unsigned int failures) {
  unsigned int delay = backoff_base_ms;
  for (unsigned int i = 1; i < failures && delay < backoff_cap_ms; ++i) delay *= 2;
  return std::min(delay, backoff_cap_ms);
}

bool has_nasty_characters(const std::string &value) { return value.find_first_of(NASTY_METACHARS) != std::string::npos; }

}  // namespace

worker::worker(std::string id, const worker_config &config, std::shared_ptr<query_executor> executor, std::shared_ptr<worker_logger> logger,
               std::shared_ptr<worker_counters> counters)
    : id_(std::move(id)),
      config_(config),
      host_names_text_(str::utils::joinEx(config.host_names, ", ")),
      executor_(std::move(executor)),
      logger_(std::move(logger)),
      counters_(std::move(counters)) {}

void worker::stop() { stop_.store(true); }

void worker::sleep_for(const unsigned int milliseconds) {
  for (unsigned int slept = 0; slept < milliseconds && !stopping(); slept += sleep_slice_ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(sleep_slice_ms, milliseconds - slept)));
  }
}

void worker::run() {
  while (!stopping()) {
    try {
      if (!connection_ || !connection_->is_open()) {
        connect();
        failures_ = 0;
      }
      pump();
    } catch (const std::exception &e) {
      disconnect();
      if (stopping()) break;
      ++counters_->errors;
      ++failures_;
      logger_->error(id_ + ": " + e.what());
      sleep_for(backoff_ms(failures_));
    } catch (...) {
      disconnect();
      if (stopping()) break;
      ++counters_->errors;
      ++failures_;
      logger_->error(id_ + ": unknown error in the worker loop");
      sleep_for(backoff_ms(failures_));
    }
  }
  disconnect();
}

void worker::connect() {
  if (config_.servers.empty()) throw connection_error("No gearmand server is configured");
  current_server_ = config_.servers[server_index_ % config_.servers.size()];
  ++server_index_;

  connection_.reset(new connection());
  connection_->connect(current_server_, config_.connect_timeout);
  // The order the C and Go workers use: name ourselves first so a failure
  // during registration is already attributable in gearman_top, then start
  // from a clean slate in case this is a reconnect the server has not
  // noticed yet.
  connection_->send(packet_type::set_client_id, {id_});
  connection_->send(packet_type::reset_abilities);
  for (const std::string &queue : config_.queues) connection_->send(packet_type::can_do, {queue});

  registered_ = true;
  ++counters_->connected;
  logger_->info(id_ + " connected to " + current_server_.to_string() + " for " + str::utils::joinEx(config_.queues, ", "));
}

void worker::disconnect() {
  if (connection_) {
    connection_->close();
    connection_.reset();
  }
  if (registered_) {
    --counters_->connected;
    registered_ = false;
  }
}

void worker::pump() {
  connection_->send(packet_type::grab_job);
  const std::function<bool()> should_stop = [this] { return stopping(); };
  bool sleeping = false;

  for (;;) {
    packet reply;
    const connection::receive_result result = connection_->receive(reply, config_.idle_timeout, should_stop);
    if (result == connection::receive_result::closed) throw connection_error("gearmand closed the connection");
    if (result == connection::receive_result::timed_out) {
      if (stopping()) return;
      // Asleep and nothing came: go round and grab again, which is also what
      // re-establishes that the socket still works.
      if (sleeping) return;
      throw connection_error("gearmand did not answer GRAB_JOB within " + std::to_string(config_.idle_timeout) + "s");
    }

    switch (reply.type) {
      case packet_type::no_job:
        // Nothing queued. PRE_SLEEP asks the server to wake us with a NOOP
        // rather than having every worker poll a busy gearmand.
        sleeping = true;
        connection_->send(packet_type::pre_sleep);
        continue;
      case packet_type::noop:
        return;
      case packet_type::job_assign:
        handle_job(reply);
        return;
      case packet_type::error:
        throw connection_error("gearmand reported an error: " + reply.arg(0) + ": " + reply.arg(1));
      default:
        // Something we did not ask for (an ECHO_RES from a previous life, a
        // packet type a newer gearmand invented). The decoder kept the stream
        // in sync, so just carry on waiting for the answer we want.
        logger_->debug(id_ + ": ignoring unexpected " + to_string(reply.type));
        continue;
    }
  }
}

bool worker::answers_for(const std::string &host_name) const { return config_.host_names.count(boost::algorithm::to_lower_copy(host_name)) > 0; }

bool worker::too_old(const check_job &job) const {
  if (config_.max_age <= 0) return false;
  double core_time = 0;
  // A job whose core_time cannot be read is not aged out: refusing checks
  // because a field is missing would be a worse failure than running one late.
  if (!parse_timestamp(job.core_time, core_time)) return false;
  return static_cast<double>(std::time(nullptr)) - core_time > static_cast<double>(config_.max_age);
}

void worker::handle_job(const packet &assignment) {
  const std::string handle = assignment.arg(0);
  const std::string queue = assignment.arg(1);

  std::string text;
  try {
    text = decode_payload(assignment.arg(2), config_.crypto);
    if (!looks_like_job_text(text)) throw job_error("the payload does not start with type=");
  } catch (const std::exception &e) {
    ++counters_->errors;
    // The payload is never logged: with the wrong key it is arbitrary bytes,
    // and with the right one it is somebody's check command line.
    logger_->error(id_ + ": could not decode a job from " + queue + " (wrong key?): " + e.what());
    connection_->send(packet_type::work_fail, {handle});
    return;
  }

  check_job job;
  try {
    job = parse_job(text);
  } catch (const std::exception &e) {
    ++counters_->errors;
    logger_->error(id_ + ": " + queue + " carried something that is not a check job: " + e.what());
    connection_->send(packet_type::work_fail, {handle});
    return;
  }

  ++counters_->jobs;
  counters_->last_job_time.store(static_cast<long long>(std::time(nullptr)));
  const std::string started = now_timestamp();
  const std::string what = job.is_service() ? job.host_name + "/" + job.service_description : job.host_name;

  // A refused job is still answered. Saying nothing would leave the core
  // waiting for its orphan timeout on every check, which looks exactly like
  // the agent being down.
  if (config_.bind_to_host && !answers_for(job.host_name)) {
    logger_->warning(id_ + ": refusing a check for " + what + " from " + queue + ": this agent answers for " + host_names_text_ +
                     ". Use proxy mode, or add the name to 'host names'.");
    answer(job, nagios_unknown, "Not run: this NSClient++ agent does not answer for " + job.host_name + ".", started, handle);
    return;
  }
  if (too_old(job)) {
    logger_->warning(id_ + ": discarding a check for " + what + " scheduled at " + job.core_time + ": older than the configured max age of " +
                     std::to_string(config_.max_age) + "s.");
    answer(job, nagios_unknown, "Not run: the job is older than the configured max age of " + std::to_string(config_.max_age) + "s.", started, handle);
    return;
  }

  // The agent's own quoting rules, the ones an alias or a script definition
  // is read with: split on unquoted blanks, `"..."` with backslash escapes.
  // So a threshold with a space in it is written `"warn=load gt 80"` in the
  // core's check_command, and a backslash is an escape there as it is
  // everywhere else in the configuration.
  std::string command;
  std::list<std::string> arguments;
  str::utils::parse_command(job.command_line, command, arguments);
  if (command.empty()) {
    answer(job, nagios_unknown, "Not run: the job carries no command.", started, handle);
    return;
  }
  if (!config_.allow_arguments && !arguments.empty()) {
    logger_->error(id_ + ": refusing " + command + " for " + what + ": it has arguments and 'allow arguments' is false.");
    answer(job, nagios_unknown, "Not run: arguments are not allowed (see the 'allow arguments' option).", started, handle);
    return;
  }
  if (!config_.allow_nasty_characters) {
    bool nasty = has_nasty_characters(command);
    for (const std::string &argument : arguments) nasty = nasty || has_nasty_characters(argument);
    if (nasty) {
      logger_->error(id_ + ": refusing " + command + " for " + what + ": it contains metacharacters and 'allow nasty characters' is false.");
      answer(job, nagios_unknown, "Not run: the command contains illegal metacharacters (see the 'allow nasty characters' option).", started, handle);
      return;
    }
  }

  const unsigned int timeout = job.timeout > 0 ? static_cast<unsigned int>(job.timeout) : config_.default_job_timeout;
  query_result result;
  try {
    result = executor_->execute(command, arguments, timeout);
  } catch (const std::exception &e) {
    ++counters_->errors;
    answer(job, nagios_unknown, "Failed to run " + command + ": " + e.what(), started, handle);
    return;
  }
  if (result.timed_out) {
    logger_->warning(id_ + ": " + command + " for " + what + " did not finish within " + std::to_string(timeout) + "s.");
    answer(job, config_.timeout_return, "(Check Timed Out)", started, handle);
    return;
  }
  answer(job, result.return_code, result.output, started, handle);
}

void worker::answer(const check_job &job, const int return_code, const std::string &output, const std::string &start_time, const std::string &handle) {
  check_result result;
  // `active` and `passive` are the result's own kinds; whether it belongs to a
  // host or a service is carried by service_description being set or not.
  result.type = "active";
  result.host_name = job.host_name;
  result.service_description = job.service_description;
  result.return_code = return_code;
  result.output = output;
  result.start_time = start_time;
  result.finish_time = now_timestamp();
  result.source = config_.source;
  result.core_start_time = job.core_time;
  // The check ran to completion as far as the core is concerned even when it
  // timed out: exited_ok=0 makes Nagios throw the output away and report its
  // own "did not exit properly" instead of the reason we just worked out.
  result.exited_ok = 1;

  const std::string queue = job.result_queue.empty() ? default_result_queue : job.result_queue;
  submit(queue, encode_payload(format_result(result), config_.crypto));

  if (connection_ && connection_->is_open()) {
    // The data is empty on purpose: the core reads results off check_results,
    // not off the job's own completion, and both reference workers do the same.
    connection_->send(packet_type::work_complete, {handle, ""});
  } else {
    logger_->debug(id_ + ": result submitted but the job could not be completed; gearmand may hand it to another worker.");
  }
}

void worker::submit(const std::string &queue, const std::string &payload) {
  try {
    connection_->submit_background(queue, payload, config_.idle_timeout, [this] { return stopping(); });
    return;
  } catch (const unconfirmed_submission &e) {
    // The result is already on the wire. Whether gearmand queued it before the
    // acknowledgement went missing is exactly what cannot be known from here,
    // so it is not sent again: a duplicate is indistinguishable from a real
    // second check on the core, while a missing one shows up as a check that
    // went stale. The connection goes, because what is left on it is a reply
    // to a packet nobody is waiting for any more.
    disconnect();
    if (stopping()) {
      logger_->debug(id_ + ": stopped before " + queue + " acknowledged a result; not retrying, gearmand may already have it.");
      return;
    }
    ++counters_->errors;
    logger_->error(id_ + ": could not confirm a result submitted to " + queue + " (" + e.what() + "); not retrying, gearmand may already have it.");
    return;
  } catch (const std::exception &e) {
    ++counters_->errors;
    logger_->warning(id_ + ": could not submit a result to " + queue + " (" + e.what() + "); retrying on a new connection.");
  }

  // Only the payload that never left this host reaches here. The worker's own
  // connection is unusable, and a result is the one thing the core is waiting
  // for, so it gets a connection of its own rather than waiting out the
  // reconnect back in the loop.
  disconnect();
  if (stopping()) {
    logger_->debug(id_ + ": not opening a connection for a result submission while shutting down.");
    return;
  }
  try {
    // The same server the result was meant for: a second gearmand in the
    // list may well be feeding a different core, which would file this result
    // against a host that never scheduled the check.
    connection retry;
    retry.connect(current_server_, config_.connect_timeout);
    retry.submit_background(queue, payload, config_.idle_timeout);
    retry.close();
  } catch (const std::exception &e) {
    ++counters_->errors;
    logger_->error(id_ + ": lost a check result, could not submit to " + queue + ": " + e.what());
  }
}

worker_pool::~worker_pool() {
  try {
    stop();
  } catch (...) {
  }
  // One more bounded wait for anything parked: a check that will not end must
  // not hold up an agent that is shutting down, but it is worth the wait.
  for (const std::shared_ptr<boost::thread> &thread : parked_) {
    try {
      thread->timed_join(boost::posix_time::seconds(30));
    } catch (...) {
    }
  }
}

void worker_pool::start(const worker_config &config, const std::shared_ptr<query_executor> &executor, const std::shared_ptr<worker_logger> &logger) {
  // A settings reload runs loadModuleEx again on the live module, so the
  // previous set of threads has to go before this one starts or every reload
  // doubles the workers sharing the queue.
  stop();
  const unsigned int count = std::max(1u, config.workers);
  for (unsigned int i = 0; i < count; ++i) {
    const std::string id = config.client_id + "-" + std::to_string(i + 1);
    const std::shared_ptr<worker> w = std::make_shared<worker>(id, config, executor, logger, counters_);
    workers_.push_back(w);
    const std::shared_ptr<worker_logger> log = logger;
    threads_.push_back(threads::start_guarded_thread(
        id, [w] { w->run(); }, [log](const std::string &message) { log->error(message); }));
  }
}

bool worker_pool::stop(const unsigned int timeout_seconds) {
  for (const std::shared_ptr<worker> &w : workers_) w->stop();
  bool joined_all = true;
  for (const std::shared_ptr<boost::thread> &thread : threads_) {
    if (!thread->timed_join(boost::posix_time::seconds(timeout_seconds))) {
      // It is running a check that overran; the worker and the counters are
      // held through shared_ptr, so letting it finish on its own is safe.
      parked_.push_back(thread);
      joined_all = false;
    }
  }
  workers_.clear();
  threads_.clear();
  return joined_all;
}

}  // namespace gearman
