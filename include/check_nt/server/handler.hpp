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

#include <check_nt/packet.hpp>
#include <boost/tuple/tuple.hpp>

namespace check_nt {
	namespace server {
		class handler : public boost::noncopyable {
		public:
			virtual check_nt::packet handle(check_nt::packet packet) = 0;
			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual check_nt::packet create_error(std::string msg) = 0;

			virtual void set_allow_arguments(bool) = 0;
			virtual void set_allow_nasty_arguments(bool) = 0;
			virtual void set_perf_data(bool) = 0;

			virtual void set_password(std::string password) = 0;
			virtual std::string get_password() const = 0;

		};
	}// namespace server
} // namespace check_nt
