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

//#include <psapi.h>
#include <list>
#include <string>
//#include <error.hpp>

#define DEFAULT_BUFFER_SIZE 4096

namespace process_helper {

	struct int_var {
		long long value;
		int_var() : value(0) {}
		long long get() const { return value; }
		int_var& operator=(const long long v) {
			value = v;
			return *this;
		}
		operator long long() const {
			return value;
		}
	};
	struct bool_var {
		bool value;
		bool_var() : value(false) {}
		bool get() const { return value; }
		bool_var& operator=(const bool v) {
			value = v;
			return *this;
		}
		operator bool() const {
			return value;
		}
	};
	struct string_var {
		std::string value;
		string_var() {}
		string_var(const std::string &s) : value(s) {}
		std::string get() const { return value; }
		string_var& operator=(const std::string &v) {
			value = v;
			return *this;
		}
		operator std::string() const {
			return value;
		}
	};

#define STR_GETTER(name) std::string get_ ## name() const { return name; }
#define INT_GETTER(name) long long get_ ## name() const { return name; }
#define BOL_GETTER(name) bool get_ ## name() const { return name; }

	struct process_info {
		string_var filename;
		string_var command_line;
		string_var exe;
		STR_GETTER(filename);
		STR_GETTER(command_line);
		STR_GETTER(exe);

		int_var pid;
		INT_GETTER(pid);

		bool started;
		bool_var hung;
		bool_var wow64;
		bool_var has_error;
		bool_var unreadable;
		string_var error;
		BOL_GETTER(started);
		BOL_GETTER(hung);
		BOL_GETTER(wow64);
		BOL_GETTER(has_error);
		BOL_GETTER(unreadable);

		process_info() {}
		process_info(const std::string s) : exe(s), started(false) {}

		std::string get_state_s() const {
			if (unreadable)
				return "unreadable";
			if (hung)
				return "hung";
			if (started)
				return "started";
			return "stopped";
		}
		bool get_stopped() const {
			return !started;
		}


		// Handles
		int_var handleCount;
		int_var gdiHandleCount;
		int_var userHandleCount;
		INT_GETTER(handleCount);
		INT_GETTER(gdiHandleCount);
		INT_GETTER(userHandleCount);

		// TImes
		int_var creation_time;
		int_var kernel_time;
		int_var user_time;
		INT_GETTER(creation_time);
		INT_GETTER(kernel_time);
		INT_GETTER(user_time);

		// IO Counters
		int_var readOperationCount;
		int_var writeOperationCount;
		int_var otherOperationCount;
		int_var readTransferCount;
		int_var writeTransferCount;
		int_var otherTransferCount;

		// Mem Counters
		int_var PeakVirtualSize;
		int_var VirtualSize;
		int_var PageFaultCount;
		int_var PeakWorkingSetSize;
		int_var WorkingSetSize;
		int_var QuotaPeakPagedPoolUsage;
		int_var QuotaPagedPoolUsage;
		int_var QuotaPeakNonPagedPoolUsage;
		int_var QuotaNonPagedPoolUsage;
		int_var PagefileUsage;
		int_var PeakPagefileUsage;
		INT_GETTER(PeakVirtualSize);
		INT_GETTER(VirtualSize);
		INT_GETTER(PageFaultCount);
		INT_GETTER(PeakWorkingSetSize);
		INT_GETTER(WorkingSetSize);
		INT_GETTER(QuotaPeakPagedPoolUsage);
		INT_GETTER(QuotaPagedPoolUsage);
		INT_GETTER(QuotaPeakNonPagedPoolUsage);
		INT_GETTER(QuotaNonPagedPoolUsage);
		INT_GETTER(PagefileUsage);
		INT_GETTER(PeakPagefileUsage);

		void set_error(std::string msg) {
			has_error = true;
		}

		static const long long state_started = 1;
		static const long long state_stopped = 0;
		static const long long state_unreadable = -1;
		static const long long state_hung = -2;
		static const long long state_unknown = -10;

		long long get_state_i() const {
			if (unreadable)
				return state_unreadable;
			if (hung)
				return state_hung;
			if (started)
				return state_started;
			return state_stopped;
		}

		static long long parse_state(const std::string &s) {
			if (s == "started")
				return state_started;
			if (s=="stopped")
				return state_stopped;
			if (s=="hung")
				return state_hung;
			if (s=="unreadable")
				return state_unreadable;
			return state_unknown;
		}

	};

	class error_reporter {
	public:
		virtual void report_error(std::string error) = 0;
		virtual void report_warning(std::string error) = 0;
		virtual void report_debug(std::string error) = 0;
	};

	typedef std::list<process_info> process_list;
	process_list enumerate_processes(bool ignore_unreadable = false, bool find_16bit = false, bool deep_scan = true, error_reporter *error_interface = NULL, unsigned int buffer_size = DEFAULT_BUFFER_SIZE);
}