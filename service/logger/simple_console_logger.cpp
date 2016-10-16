#pragma once

#include "simple_console_logger.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"

#include <iostream>
#include <sstream>

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
						settings.set_alias("log/file");

						settings.add_path_to_settings()
							("log", "LOG SECTION", "Configure log properties.")
							;

						settings.add_key_to_settings("log")
							("date format", sh::string_key(&format_, "%Y-%m-%d %H:%M:%S"),
								"DATEMASK", "The syntax of the dates in the log file.")
							;

						settings.register_all();
						settings.notify();
					} catch (nscapi::nscapi_exception &e) {
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
					} catch (nscapi::nscapi_exception &e) {
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