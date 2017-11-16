/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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

