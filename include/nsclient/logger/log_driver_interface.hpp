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

#pragma once

#include <nsclient/logger/logger.hpp>

#include <boost/shared_ptr.hpp>

#include <string>

namespace nsclient {
	namespace logging {


		struct log_driver_interface {

			log_driver_interface() {}
			virtual ~log_driver_interface() {}
			virtual void do_log(const std::string data) = 0;
			virtual void synch_configure() = 0;
			virtual void asynch_configure() = 0;

			virtual void set_config(const std::string &key) = 0;
			virtual void set_config(const boost::shared_ptr<log_driver_interface> other) = 0;
			virtual bool shutdown() = 0;
			virtual bool startup() = 0;
			virtual bool is_console() const = 0;
			virtual bool is_oneline() const = 0;
			virtual bool is_no_std_err() const = 0;
			virtual bool is_started() const = 0;

		};

		typedef boost::shared_ptr<log_driver_interface> log_driver_instance;

	}
}