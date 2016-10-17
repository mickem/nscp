/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nsclient_logger.hpp"

#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/base_logger_impl.hpp>

#include "simple_console_logger.hpp"
#include "simple_file_logger.hpp"
#include "threaded_logger.hpp"

#define CONSOLE_BACKEND "console"
#define THREADED_FILE_BACKEND "threaded-file"
#define FILE_BACKEND "file"
#define DEFAULT_BACKEND THREADED_FILE_BACKEND

void nsclient::logging::impl::nsclient_logger::set_backend(std::string backend) {
	nsclient::logging::log_driver_instance tmp;
	if (backend == CONSOLE_BACKEND) {
		tmp = log_driver_instance(new simple_console_logger());
	} else if (backend == THREADED_FILE_BACKEND) {
		nsclient::logging::log_driver_instance inner = log_driver_instance(new simple_file_logger("nsclient.log"));
		tmp = log_driver_instance(new threaded_logger(this, inner));
	} else if (backend == FILE_BACKEND) {
		tmp = log_driver_instance(new simple_file_logger("nsclient.log"));
	} else {
		tmp = log_driver_instance(new simple_console_logger());
	}
	if (backend_ && tmp) {
		tmp->set_config(backend_);
	}
	tmp->startup();
	backend_.swap(tmp);
	backend_->do_log(log_message_factory::create_debug("log", __FILE__, __LINE__, "Creating logger: " + backend));
}

nsclient::logging::impl::nsclient_logger::nsclient_logger() {
	set_backend(DEFAULT_BACKEND);
}

nsclient::logging::impl::nsclient_logger::~nsclient_logger() {
	destroy();
}

void nsclient::logging::impl::nsclient_logger::destroy() {
	backend_.reset();
}

void nsclient::logging::impl::nsclient_logger::add_subscriber(nsclient::logging::logging_subscriber_instance subscriber) {
	subscribers_.push_back(subscriber);
}

void nsclient::logging::impl::nsclient_logger::clear_subscribers() {
	subscribers_.clear();
}
bool nsclient::logging::impl::nsclient_logger::startup() {
	return backend_->startup();
}
bool nsclient::logging::impl::nsclient_logger::shutdown() {
	return backend_->shutdown();
}
void nsclient::logging::impl::nsclient_logger::configure() {
	backend_->synch_configure();
	backend_->asynch_configure();
}

void nsclient::logging::impl::nsclient_logger::do_log(const std::string data) {
	backend_->do_log(data);
}

