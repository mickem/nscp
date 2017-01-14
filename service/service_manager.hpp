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

#ifdef _WIN32
#include <ServiceCmd.h>
#endif
#include <config.h>

namespace nsclient {
	namespace client {
#ifdef _WIN32
		class service_manager {
		private:
			std::string service_name_;

		public:
			service_manager(std::string service_name) : service_name_(service_name) {}

			static std::string get_default_service_name() {
				return DEFAULT_SERVICE_NAME;
			}
			static std::string get_default_service_desc() {
				return DEFAULT_SERVICE_DESC;
			}
			static std::string get_default_service_deps() {
				return DEFAULT_SERVICE_DEPS;
			}
			static std::string get_default_arguments() {
				return "service --run";
			}
			inline void print_msg(std::string str) {
				std::cout << str << std::endl;
			}
			inline void print_error(std::string str) {
				std::cerr << "ERROR: " << utf8::to_encoding(str, "") << std::endl;
			}
		public:
			int install(std::string service_description) {
				try {
					std::string args = get_default_arguments();
					if (service_name_ != get_default_service_name())
						args += " --name " + service_name_;
					serviceControll::Install(utf8::cvt<std::wstring>(service_name_), utf8::cvt<std::wstring>(service_description), utf8::cvt<std::wstring>(get_default_service_deps()), SERVICE_WIN32_OWN_PROCESS, utf8::cvt<std::wstring>(args));
				} catch (const serviceControll::SCException& e) {
					print_error("Service installation failed of '" + service_name_ + "' failed: " + e.error_);
					return -1;
				}
				try {
					serviceControll::SetDescription(utf8::cvt<std::wstring>(service_name_), utf8::cvt<std::wstring>(service_description));
				} catch (const serviceControll::SCException& e) {
					print_error("Couldn't set service description: " + e.error_);
				}
				print_msg("Service installed successfully!");
				return 0;
			}
			int uninstall() {
				try {
					serviceControll::Uninstall(utf8::cvt<std::wstring>(service_name_));
				} catch (const serviceControll::SCException& e) {
					print_error("Service de-installation (" + service_name_ + ") failed; " + e.error_ + "\nMaybe the service was not previously installed properly?");
					return 0;
				}
				print_msg("Service deinstalled successfully!");
				return 0;
			}
			int start() {
				try {
					serviceControll::Start(utf8::cvt<std::wstring>(service_name_));
				} catch (const serviceControll::SCException& e) {
					print_error("Service failed to start: " + e.error_);
					return -1;
				}
				return 0;
			}
			int stop() {
				try {
					serviceControll::Stop(utf8::cvt<std::wstring>(service_name_));
				} catch (const serviceControll::SCException& e) {
					print_error("Service failed to stop: " + e.error_);
					return -1;
				}
				return 0;
			}
			std::string info() {
				try {
					return utf8::cvt<std::string>(serviceControll::get_exe_path(utf8::cvt<std::wstring>(service_name_)));
				} catch (const serviceControll::SCException& e) {
					print_error("Failed to find service: " + e.error_);
					return "";
				}
			}
		};
#else
		class service_manager {
		public:
			service_manager(std::string service_name) {}
			int unsupported() {
				std::cout << "Service management is not supported on non Windows operating systems..." << std::endl;
				return -1;
			}
			int install(std::string service_description) {
				return unsupported();
			}
			int uninstall() {
				return unsupported();
			}
			int start() {
				return unsupported();
			}
			int stop() {
				return unsupported();
			}
			std::string info() {
				return "";
			}
			static std::string get_default_service_name() {
				return DEFAULT_SERVICE_NAME;
			}
		};
#endif
	}
}