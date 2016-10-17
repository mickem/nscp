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

#include <boost/thread/condition.hpp>
#pragma once

#include <iostream>
#include <string>
#include <signal.h>
#include <unicode_char.hpp>
#include <strEx.h>

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
