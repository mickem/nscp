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

#include <nsclient/logger/logger.hpp>
#include <nsclient/logger/log_driver_interface.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/log_level.hpp>

#include <string>

namespace nsclient {
	namespace logging {

		class log_driver_interface_impl : public log_driver_interface  {
			bool console_log_;
			bool oneline_;
			bool no_std_err_;
			bool is_running_;
		public:
			log_driver_interface_impl() : console_log_(false), oneline_(false), no_std_err_(false), is_running_(false){}
			virtual ~log_driver_interface_impl() {}

			virtual bool is_console() const {
				return console_log_;
			}
			bool is_oneline() const {
				return oneline_;
			}
			bool is_no_std_err() const {
				return no_std_err_;
			}
			virtual bool shutdown() {
				is_running_ = false;
				return true;
			}
			virtual bool startup() {
				is_running_ = true;
				return true;
			}
			bool is_started() const {
				return is_running_;
			}

			virtual void set_config(const nsclient::logging::log_driver_instance other) {
				if (other->is_console())
					set_config("console");
				if (other->is_no_std_err())
					set_config("no-std-err");
				if (other->is_oneline())
					set_config("oneline");
				if (other->is_started())
					startup();
			}

			virtual void set_config(const std::string &key) {
				if (key == "console")
					console_log_ = true;
				else if (key == "oneline")
					oneline_ = true;
				else if (key == "no-std-err")
					no_std_err_ = true;
				else
					do_log(log_message_factory::create_error("logger", __FILE__, __LINE__, "Invalid key: " + key));
			}

		};

		class logger_impl : public nsclient::logging::logger {
			log_level level_;
			bool is_running_;
		public:
			logger_impl() : is_running_(false) {}
			virtual ~logger_impl() {}

			bool should_trace() const {
				return level_.should_trace();
			}
			bool should_debug() const {
				return level_.should_debug();
			}
			bool should_info() const {
				return level_.should_info();
			}
			bool should_warning() const {
				return level_.should_warning();
			}
			bool should_error() const {
				return level_.should_error();
			}
			bool should_critical() const {
				return level_.should_critical();
			}

			virtual void set_log_level(std::string level) {
				if (!level_.set(level)) {
					do_log(log_message_factory::create_error("logger", __FILE__, __LINE__, "Invalid log level: " + level));
				}
			}
			std::string get_log_level() const {
				return level_.get();
			}
			void debug(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_debug())
					do_log(log_message_factory::create_debug(module, file, line, message));
			}
			void trace(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_trace())
					do_log(log_message_factory::create_trace(module, file, line, message));
			}
			void info(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_info())
					do_log(log_message_factory::create_info(module, file, line, message));
			}
			void warning(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_warning())
					do_log(log_message_factory::create_warning(module, file, line, message));
			}
			void error(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_error())
					do_log(log_message_factory::create_error(module, file, line, message));
			}
			void critical(const std::string &module, const char* file, const int line, const std::string &message) {
				if (should_critical())
					do_log(log_message_factory::create_critical(module, file, line, message));
			}
			void raw(const std::string &message) {
				do_log(message);
			}

			virtual void do_log(const std::string data) = 0;
		};
	}
}