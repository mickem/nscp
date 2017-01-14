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

#include <boost/shared_ptr.hpp>

#include <list>
#include <string>

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
		int_var &operator +=(long long other) {
			value += other;
			return *this;
		}
		int_var &operator -=(long long other) {
			value -= other;
			return *this;
		}
		void delta(long long previous, long long total) {
			if (total == 0) {
				value = 0;
			} else {
				value = 100 * (value - previous) / total;
			}
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
		bool_var is_new;
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
		STR_GETTER(error);

		process_info() {}
		process_info(const std::string s) : exe(s), started(false) {}

		std::string get_state_s() const {
			if (has_error)
				return "error";
			if (unreadable)
				return "unreadable";
			if (hung)
				return "hung";
			if (started)
				return "started";
			return "stopped";
		}
		std::string get_legacy_state_s() const {
			if (unreadable)
				return "unreadable";
			if (hung)
				return "hung";
			if (started)
				return "Running";
			return "not running";
		}
		bool get_stopped() const {
			return !started;
		}
		bool get_is_new() const {
			return is_new;
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
		unsigned long long user_time_raw;
		unsigned long long kernel_time_raw;
		int_var user_time;
		int_var total_time;
		INT_GETTER(creation_time);
		INT_GETTER(kernel_time);
		INT_GETTER(user_time);
		INT_GETTER(total_time);

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
			error = msg;
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
			if (s == "stopped")
				return state_stopped;
			if (s == "hung")
				return state_hung;
			if (s == "unreadable")
				return state_unreadable;
			return state_unknown;
		}

		process_info& operator += (const process_info &other) {
			// Handles
			handleCount += other.handleCount;
			gdiHandleCount += other.gdiHandleCount;
			userHandleCount += other.userHandleCount;

			// TImes
			creation_time += other.creation_time;
			kernel_time += other.kernel_time;
			user_time += other.user_time;
			kernel_time_raw += other.kernel_time_raw;
			user_time_raw += other.user_time_raw;

			// IO Counters
			readOperationCount += other.readOperationCount;
			writeOperationCount += other.writeOperationCount;
			otherOperationCount += other.otherOperationCount;
			readTransferCount += other.readTransferCount;
			writeTransferCount += other.writeTransferCount;
			otherTransferCount += other.otherTransferCount;

			// Mem Counters
			PeakVirtualSize += other.PeakVirtualSize;
			VirtualSize += other.VirtualSize;
			PageFaultCount += other.PageFaultCount;
			PeakWorkingSetSize += other.PeakWorkingSetSize;
			WorkingSetSize += other.WorkingSetSize;
			QuotaPeakPagedPoolUsage += other.QuotaPeakPagedPoolUsage;
			QuotaPagedPoolUsage += other.QuotaPagedPoolUsage;
			QuotaPeakNonPagedPoolUsage += other.QuotaPeakNonPagedPoolUsage;
			QuotaNonPagedPoolUsage += other.QuotaNonPagedPoolUsage;
			PagefileUsage += other.PagefileUsage;
			PeakPagefileUsage += other.PeakPagefileUsage;

			return *this;
		}

		process_info& operator -= (const process_info &other) {
			// Handles
			handleCount -= other.handleCount;
			gdiHandleCount -= other.gdiHandleCount;
			userHandleCount -= other.userHandleCount;

			// TImes
			creation_time -= other.creation_time;
			kernel_time -= other.kernel_time;
			user_time -= other.user_time;
			kernel_time_raw -= other.kernel_time_raw;
			user_time_raw -= other.user_time_raw;

			// IO Counters
			readOperationCount -= other.readOperationCount;
			writeOperationCount -= other.writeOperationCount;
			otherOperationCount -= other.otherOperationCount;
			readTransferCount -= other.readTransferCount;
			writeTransferCount -= other.writeTransferCount;
			otherTransferCount -= other.otherTransferCount;

			// Mem Counters
			PeakVirtualSize -= other.PeakVirtualSize;
			VirtualSize -= other.VirtualSize;
			PageFaultCount -= other.PageFaultCount;
			PeakWorkingSetSize -= other.PeakWorkingSetSize;
			WorkingSetSize -= other.WorkingSetSize;
			QuotaPeakPagedPoolUsage -= other.QuotaPeakPagedPoolUsage;
			QuotaPagedPoolUsage -= other.QuotaPagedPoolUsage;
			QuotaPeakNonPagedPoolUsage -= other.QuotaPeakNonPagedPoolUsage;
			QuotaNonPagedPoolUsage -= other.QuotaNonPagedPoolUsage;
			PagefileUsage -= other.PagefileUsage;
			PeakPagefileUsage -= other.PeakPagefileUsage;

			return *this;
		}

		void make_cpu_delta(unsigned long long kernel, unsigned long long user, unsigned long long total) {
			if (kernel > 0)
				kernel_time = kernel_time_raw * 100 / kernel;
			if (user > 0)
				user_time = user_time_raw * 100 / user;
			if (total > 0)
				total_time = (kernel_time_raw + user_time_raw) * 100 / total;
		}
		static boost::shared_ptr<process_helper::process_info> get_total();

		std::string to_string() {
			return exe.get();
		}

	};

	struct error_reporter {
		virtual void report_error(std::string error) = 0;
		virtual void report_warning(std::string error) = 0;
		virtual void report_debug(std::string error) = 0;
	};

	typedef std::list<process_info> process_list;
	process_list enumerate_processes(bool ignore_unreadable = false, bool find_16bit = false, bool deep_scan = true, error_reporter *error_interface = NULL, unsigned int buffer_size = DEFAULT_BUFFER_SIZE);
	process_list enumerate_processes_delta(bool ignore_unreadable, error_reporter *error_interface);

	void enable_token_privilege(LPTSTR privilege, bool enable);
}
