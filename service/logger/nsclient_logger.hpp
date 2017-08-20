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

#pragma once

#include <list>
#include <string>

#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/base_logger_impl.hpp>

namespace nsclient {
	namespace logging {
		namespace impl {
			class nsclient_logger : public nsclient::logging::logger_impl, nsclient::logging::logging_subscriber {


				typedef std::list<nsclient::logging::logging_subscriber_instance> subscribers_type;


				nsclient::logging::log_driver_instance backend_;
				subscribers_type subscribers_;
				mutable boost::timed_mutex mutex_;

			public:

				nsclient_logger();
				~nsclient_logger();

				void add(nsclient::logging::logging_subscriber_instance subscriber) {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					subscribers_.push_back(subscriber);
				}
				void clear() {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					subscribers_.clear();
				}

				void on_log_message(std::string &data) {
					boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
					if (!lock.owns_lock())
						return;
					if (subscribers_.empty())
						return;
					BOOST_FOREACH(nsclient::logging::logging_subscriber_instance & s, subscribers_) {
						s->on_log_message(data);
					}
				}


				virtual void set_log_level(const std::string level) {
					if (level == "console") {
						if (backend_) {
							backend_->set_config(level);
						}
					} else {
						nsclient::logging::logger_impl::set_log_level(level);
					}
				}


				void do_log(const std::string data);



				void set_backend(std::string backend);
				void destroy();


				void add_subscriber(nsclient::logging::logging_subscriber_instance);
				void clear_subscribers();
				bool startup();
				bool shutdown();
				void configure();

			};
		}
	}
}