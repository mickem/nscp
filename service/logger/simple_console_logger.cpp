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

#include "simple_console_logger.hpp"

#include "../libs/settings_manager/settings_manager_impl.h"

#include <nscapi/nscapi_settings_helper.hpp>
#include <nsclient/nsclient_exception.hpp>

#include <iostream>

namespace nsclient {
	namespace logging {
		namespace impl {

			namespace sh = nscapi::settings_helper;


				simple_console_logger::simple_console_logger() : format_("%Y-%m-%d %H:%M:%S"), buf_(65536) {
					std::cout.rdbuf()->pubsetbuf(buf_.data(), buf_.size());
				}

				void simple_console_logger::do_log(const std::string data) {
					if (is_console()) {
						std::pair<bool, std::string> m = logger_helper::render_console_message(is_oneline(), data);
						if (!is_no_std_err() && m.first)
							std::cerr << m.second;
						else
							std::cout << m.second;
					}
				}
				simple_console_logger::config_data simple_console_logger::do_config() {
					config_data ret;
					try {
						sh::settings_registry settings(settings_manager::get_proxy());
						settings.set_alias("log");

//						settings.add_path_to_settings()
//							("log", "Log file", "Configure log file properties.")
//							;

						settings.add_key_to_settings("log")
							("date format", sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
								"Console date mask", "The syntax of the dates in the log file.")
							;

						settings.register_all();
						settings.notify();
					} catch (nsclient::nsclient_exception &e) {
						logger_helper::log_fatal(std::string("Failed to register command: ") + e.what());
					} catch (std::exception &e) {
						logger_helper::log_fatal(std::string("Exception caught: ") + e.what());
					} catch (...) {
						logger_helper::log_fatal("Failed to register command.");
					}
					return ret;
				}
				void simple_console_logger::synch_configure() {
					do_config();
				}
				void simple_console_logger::asynch_configure() {
					try {
						config_data config = do_config();
						format_ = config.format;
					} catch (nsclient::nsclient_exception &e) {
						logger_helper::log_fatal(std::string("Failed to register command: ") + e.what());
					} catch (std::exception &e) {
						logger_helper::log_fatal(std::string("Exception caught: ") + e.what());
					} catch (...) {
						logger_helper::log_fatal("Failed to register command.");
					}
				}
		}
	}
}