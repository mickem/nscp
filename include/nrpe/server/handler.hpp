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

#include <nrpe/packet.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

#include <list>
namespace nrpe {
	namespace server {
		class handler : boost::noncopyable {
		public:
			virtual std::list<nrpe::packet> handle(nrpe::packet packet) = 0;
			virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
			virtual nrpe::packet create_error(std::string msg) = 0;
			virtual unsigned int get_payload_length() = 0;
		};
	}// namespace server
} // namespace nrpe