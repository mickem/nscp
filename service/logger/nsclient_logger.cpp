// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsclient_logger.hpp"

#include <nsclient/logger/logger.hpp>

#include "simple_console_logger.hpp"
#include "simple_file_logger.hpp"
#include "threaded_logger.hpp"

#define CONSOLE_BACKEND "console"
#define THREADED_FILE_BACKEND "threaded-file"
#define FILE_BACKEND "file"
// Console everywhere, on every platform. Writing to a log file is a thing the
// *service* does - it selects the file backend explicitly when it starts (see
// cli_parser::parse_service, and the --log-backend in the systemd unit).
//
// It used to default to the file backend on Windows, which meant every CLI
// invocation opened a file next to the executable: under Program Files, so an
// ordinary user running `nscp client ...` failed to open it and logging quietly
// degraded. Nothing about a one-shot command needs a log file, and needing
// write access to the install directory to run one is worse than useless.
#define DEFAULT_BACKEND CONSOLE_BACKEND

void nsclient::logging::impl::nsclient_logger::set_backend(const std::string backend) {
  log_driver_instance tmp;
  if (backend == CONSOLE_BACKEND) {
    tmp = std::make_shared<simple_console_logger>(this);
  } else if (backend == THREADED_FILE_BACKEND) {
    log_driver_instance inner = std::make_shared<simple_file_logger>("nsclient.log");
    tmp = std::make_shared<threaded_logger>(this, inner);
  } else if (backend == FILE_BACKEND) {
    tmp = std::make_shared<simple_file_logger>("nsclient.log");
  } else {
    tmp = std::make_shared<simple_console_logger>(this);
  }
  if (backend_ && tmp) {
    tmp->set_config(backend_);
  }
  tmp->startup();
  backend_.swap(tmp);
}

nsclient::logging::impl::nsclient_logger::nsclient_logger() { nsclient_logger::set_backend(DEFAULT_BACKEND); }

nsclient::logging::impl::nsclient_logger::~nsclient_logger() { nsclient_logger::destroy(); }

void nsclient::logging::impl::nsclient_logger::destroy() { backend_.reset(); }

// Route through the mutex-protected add()/clear() helpers: on_log_message()
// iterates subscribers_ under mutex_ from the logging thread, so mutating the
// list without that lock (during plugin load / shutdown) is a data race that
// can free list nodes mid-iteration.
void nsclient::logging::impl::nsclient_logger::add_subscriber(const logging_subscriber_instance subscriber) { add(subscriber); }

void nsclient::logging::impl::nsclient_logger::clear_subscribers() { clear(); }
bool nsclient::logging::impl::nsclient_logger::startup() {
  if (backend_) {
    return backend_->startup();
  } else {
    return false;
  }
}
bool nsclient::logging::impl::nsclient_logger::shutdown() {
  if (backend_) {
    return backend_->shutdown();
  }
  return false;
}
void nsclient::logging::impl::nsclient_logger::configure() {
  if (backend_) {
    backend_->synch_configure();
    backend_->asynch_configure();
  }
}

void nsclient::logging::impl::nsclient_logger::do_log(const std::string data) {
  if (backend_) {
    backend_->do_log(data);
  }
}
