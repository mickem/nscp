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

#include <nsclient/logger/base_logger_impl.hpp>

#include <concurrent_queue.hpp>

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <string>


namespace nsclient {
	namespace logging {
		namespace impl {
			class threaded_logger : public nsclient::logging::log_driver_interface_impl {
				concurrent_queue<std::string> log_queue_;
				boost::thread thread_;

				log_driver_instance background_logger_;
				logging_subscriber *subscriber_manager_;

			public:

				threaded_logger(logging_subscriber *subscriber_manager, log_driver_instance background_logger);
				virtual ~threaded_logger();

				virtual void do_log(const std::string data);
				void push(const std::string &data);

				void thread_proc();

				virtual void asynch_configure();
				virtual void synch_configure();
				virtual bool startup();
				virtual bool shutdown();

				//virtual void set_log_level(NSCAPI::log_level::level level);
				virtual void set_config(const std::string &key);
			};
		}
	}
}