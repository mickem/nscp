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

#include <boost/thread/condition.hpp>
#pragma once

#include <iostream>
#include <string>
#include <signal.h>

namespace service_helper_impl {
	/**
	* @ingroup NSClient++
	* Helper class to implement a NT service
	*
	* @version 1.0
	* first version
	*
	* @date 02-13-2005
	*
	* @author mickem
	*
	* @par license
	* This code is absolutely free to use and modify. The code is provided "as is" with
	* no expressed or implied warranty. The author accepts no liability if it causes
	* any damage to your computer, causes your pet to fall ill, increases baldness
	* or makes your car start emitting strange noises when you start it up.
	* This code has no bugs, just undocumented features!
	* 
	* @todo 
	*
	* @bug 
	*
	*/
	template <class TBase>
	class unix_service : public TBase {
	private:
		boost::mutex			stop_mutex_;
		bool is_running_;
		boost::condition shutdown_condition_;

	public:
		unix_service() {
		}
		virtual ~unix_service() {
		}
		inline void print_debug(const std::string s) {
			std::cout << s << std::endl;
		}
		inline void print_debug(const char *s) {
			std::cout << s << std::endl;
		}

		static void handleSigTerm(int) {
			TBase::get_global_instance()->stop_service();
		}
		static void handleSigInt(int) {
			TBase::get_global_instance()->stop_service();
		}
/** start */
		void start_and_wait(std::string name) {
			is_running_ = true;

			if (signal(SIGTERM, unix_service<TBase>::handleSigTerm) == SIG_ERR) 
				handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");
			if (signal(SIGINT, unix_service<TBase>::handleSigInt) == SIG_ERR) 
				handle_error(__LINE__, __FILE__, "Failed to hook SIGTERM!");

			TBase::handle_startup("TODO");

			print_debug("Service started waiting for termination event...");
			{
				boost::unique_lock<boost::mutex> lock(stop_mutex_);
				while(is_running_)
					shutdown_condition_.wait(lock);
			}

			print_debug("Shutting down...");
			TBase::handle_shutdown("TODO");
			print_debug("Shutting down (down)...");
		}
		void stop_service() {
			{
				boost::lock_guard<boost::mutex> lock(stop_mutex_);
				is_running_ = false;
			}
			shutdown_condition_.notify_one();
		}
		static void handle_error(unsigned int line, const char *file, std::string message) {
			TBase::get_global_instance()->handle_error(line, file, message);
		}
	};
}
