#include <boost/thread/condition.hpp>


/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <signal.h>
#include <unicode_char.hpp>
#include <strEx.h>

namespace service_helper_impl {
	class service_exception {
		std::wstring what_;
	public:
		service_exception(std::wstring what) : what_(what) {
			std::wcout << _T("ERROR:") <<  what;
		}
		std::wstring what() {
			return what_;
		}
	};
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
		inline void print_debug(const std::wstring s) {
			std::wcout << s << std::endl;
		}
		inline void print_debug(const wchar_t *s) {
			std::wcout << s << std::endl;
		}

		static void handleSigTerm(int) {
			TBase::get_global_instance()->stop_service();
		}
		static void handleSigInt(int) {
			TBase::get_global_instance()->stop_service();
		}
/** start */
		void start_and_wait(std::wstring name) {
			is_running_ = true;

			if (signal(SIGTERM, unix_service<TBase>::handleSigTerm) == SIG_ERR) 
				handle_error(__LINE__, __FILEW__, _T("Failed to hook SIGTERM!"));
			if (signal(SIGINT, unix_service<TBase>::handleSigInt) == SIG_ERR) 
				handle_error(__LINE__, __FILEW__, _T("Failed to hook SIGTERM!"));

			TBase::handle_startup(_T("TODO"));

			print_debug(_T("Service started waiting for termination event..."));
			{
				boost::unique_lock<boost::mutex> lock(stop_mutex_);
				while(is_running_)
					shutdown_condition_.wait(lock);
			}

			print_debug(_T("Shutting down..."));
			TBase::handle_shutdown(_T("TODO"));
			print_debug(_T("Shutting down (down)..."));
		}
		void stop_service() {
			{
				boost::lock_guard<boost::mutex> lock(stop_mutex_);
				is_running_ = false;
			}
			shutdown_condition_.notify_one();
		}
		static void handle_error(unsigned int line, wchar_t *file, std::wstring message) {
			TBase::get_global_instance()->handle_error(line, file, message);
		}
	};
}