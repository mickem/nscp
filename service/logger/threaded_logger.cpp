// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "threaded_logger.hpp"

#include <iostream>
#include <nsclient/logger/logger_helper.hpp>
#include <utility>

const static std::string QUIT_MESSAGE = "$$QUIT$$";
const static std::string CONFIGURE_MESSAGE = "$$CONFIGURE$$";
const static std::string SET_CONFIG_MESSAGE = "$$SET_CONFIG$$";

namespace nsclient {
namespace logging {
namespace impl {
threaded_logger::threaded_logger(logging_subscriber *subscriber_manager, log_driver_instance background_logger) : state_(std::make_shared<shared_state>()) {
  state_->subscriber_manager = subscriber_manager;
  state_->background_logger = std::move(background_logger);
}
threaded_logger::~threaded_logger() { threaded_logger::shutdown(); }

void threaded_logger::do_log(const std::string data) { push(data); }
void threaded_logger::push(const std::string &data) { state_->queue.push(data); }

void threaded_logger::thread_proc(std::shared_ptr<shared_state> state) {
  std::string data;
  while (true) {
    try {
      state->queue.wait_and_pop(data);
      // Abandoned by a shutdown that gave up waiting: the logger this thread
      // belongs to is gone, so touch nothing but the shared state.
      if (state->abandoned) return;
      if (data == QUIT_MESSAGE) {
        return;
      }
      if (data == CONFIGURE_MESSAGE) {
        if (state->background_logger) state->background_logger->asynch_configure();
      } else if (data.size() > SET_CONFIG_MESSAGE.size() && data.substr(0, SET_CONFIG_MESSAGE.size()) == SET_CONFIG_MESSAGE) {
        state->background_logger->set_config(data.substr(SET_CONFIG_MESSAGE.size()));
      } else {
        if (!state->background_logger || state->background_logger->is_console()) {
          std::pair<bool, std::string> m = logger_helper::render_console_message(state->oneline, data);
          if (!state->no_std_err && m.first)
            std::cerr << m.second;
          else
            // Flush every message; see simple_console_logger::do_log for why
            // an unflushed std::cout makes the console log lag on Windows.
            std::cout << m.second << std::flush;
        }
        if (state->background_logger) state->background_logger->do_log(data);
        // Under the mutex, and only while shutdown has not cleared it: the
        // subscriber manager is the nsclient_logger that owns this logger, so
        // an abandoned worker calling it would run against an object already
        // being destroyed.
        boost::lock_guard<boost::mutex> lock(state->subscriber_mutex);
        if (state->subscriber_manager) state->subscriber_manager->on_log_message(data);
      }
    } catch (const std::exception &e) {
      logger_helper::log_fatal(std::string("Failed to process log message: ") + e.what());
    } catch (...) {
      logger_helper::log_fatal("Failed to process log message");
    }
  }
}

void threaded_logger::asynch_configure() { push(CONFIGURE_MESSAGE); }
void threaded_logger::synch_configure() { state_->background_logger->synch_configure(); }
bool threaded_logger::startup() {
  if (is_started()) return true;
  // Snapshot the rendering flags into the state before the worker starts, so
  // it never has to read them back off this object. See shared_state for why
  // they cannot change afterwards.
  state_->oneline = is_oneline();
  state_->no_std_err = is_no_std_err();
  std::shared_ptr<shared_state> state = state_;
  thread_ = boost::thread([state]() { thread_proc(state); });
  return log_driver_interface_impl::startup();
}
bool threaded_logger::shutdown() {
  if (!is_started()) return true;
  try {
    push(QUIT_MESSAGE);
    if (!thread_.timed_join(join_timeout_)) {
      logger_helper::log_fatal("Failed to exit log slave!");
      // Leave the thread running on state it co-owns rather than destroy the
      // queue it will return to. Clearing the subscriber under its mutex both
      // stops the worker calling into the nsclient_logger that owns us - a
      // bare pointer, and about to be destroyed - and waits out a call already
      // in flight, which is the case the abandoned flag alone cannot catch.
      // The background logger is deliberately left running: the state holds a
      // shared_ptr to it, so it stays alive for as long as the worker can
      // still write to it.
      {
        boost::lock_guard<boost::mutex> lock(state_->subscriber_mutex);
        state_->subscriber_manager = nullptr;
      }
      state_->abandoned = true;
      thread_.detach();
      log_driver_interface_impl::shutdown();
      return false;
    }
    state_->background_logger->shutdown();
    return log_driver_interface_impl::shutdown();
  } catch (const std::exception &e) {
    logger_helper::log_fatal(std::string("Failed to exit log slave: ") + e.what());
  } catch (...) {
    logger_helper::log_fatal("Failed to exit log slave");
  }
  return false;
}
void threaded_logger::set_config(const std::string &key) {
  const std::string message = SET_CONFIG_MESSAGE + key;
  push(message);
}
}  // namespace impl
}  // namespace logging
}  // namespace nsclient