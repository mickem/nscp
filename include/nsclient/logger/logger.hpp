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

#include <boost/shared_ptr.hpp>

#include <string>

#define LOG_CRITICAL_CORE(msg) { get_logger()->critical("core", __FILE__, __LINE__, msg);}
#define LOG_CRITICAL_CORE_STD(msg) LOG_CRITICAL_CORE(std::string(msg))
#define LOG_ERROR_CORE(msg) { get_logger()->error("core", __FILE__, __LINE__, msg);}
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::string(msg))
#define LOG_INFO_CORE(msg) { get_logger()->info("core", __FILE__, __LINE__, msg);}
#define LOG_INFO_CORE_STD(msg) LOG_INFO_CORE(std::string(msg))
#define LOG_DEBUG_CORE(msg) { get_logger()->debug("core", __FILE__, __LINE__, msg);}
#define LOG_DEBUG_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define IS_LOG_DEBUG_CORE(msg) { if (get_logger()->should_debug())
#define LOG_TRACE_CORE(msg) { get_logger()->trace("core", __FILE__, __LINE__, msg);}
#define LOG_TRACE_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define IS_LOG_TRACE_CORE(msg) if (get_logger()->should_trace())

namespace nsclient {
	namespace logging {
		struct logging_subscriber {
			virtual void on_log_message(std::string &payload) = 0;
		};
		typedef boost::shared_ptr<logging_subscriber> logging_subscriber_instance;

		struct log_interface {
			virtual void trace(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void debug(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void info(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void warning(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void error(const std::string &module, const char* file, const int line, const std::string &message) = 0;
			virtual void critical(const std::string &module, const char* file, const int line, const std::string &message) = 0;

			virtual bool should_trace() const = 0;
			virtual bool should_debug() const = 0;
			virtual bool should_info() const = 0;
			virtual bool should_warning() const = 0;
			virtual bool should_error() const = 0;
			virtual bool should_critical() const = 0;
		};

		struct logger : public log_interface {

			virtual void raw(const std::string &message) = 0;

			virtual void add_subscriber(logging_subscriber_instance subscriber) = 0;
			virtual void clear_subscribers() = 0;

			virtual bool startup() = 0;
			virtual bool shutdown() = 0;
			virtual void destroy() = 0;
			virtual void configure() = 0;

			virtual void set_log_level(std::string level) = 0;
			virtual std::string get_log_level() const = 0;

			virtual void set_backend(std::string backend) = 0;
		};
		typedef boost::shared_ptr<logger> logger_instance;
	}
}