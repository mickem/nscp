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

#include <nsclient/nsclient_exception.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

#include <error/error.hpp>
#include <str/format.hpp>
#include <str/utils.hpp>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <map>
#include <string>

#include <atlbase.h>
#include <atlsafe.h>

#include <MSTask.h>
#include <taskschd.h>


namespace tasksched_filter {
	namespace helpers {
		struct task_sched_date {
			unsigned long long date_;
			bool never_;
			task_sched_date() : never_(false), date_(0) {}
			task_sched_date(bool never) : never_(never), date_(0) {}
			task_sched_date(unsigned long long date) : never_(false), date_(date) {}
			void operator=(unsigned long long date) {
				date_ = date;
				never_ = false;
			}
			bool has_run() const {
				return !never_;
			}
			operator unsigned long long() {
				if (never_ || date_ == 0)
					return 0;
				return str::format::filetime_to_time(date_);
			}
			operator const unsigned long long() const {
				if (never_ || date_ == 0)
					return 0;
				return str::format::filetime_to_time(date_);
			}
		};
		typedef boost::optional<std::string> op_string;
		typedef boost::optional<unsigned long> op_dword;
		typedef boost::optional<unsigned short> op_word;
		typedef boost::optional<long> op_long;
		typedef boost::optional<task_sched_date> op_date;
		typedef boost::optional<bool> op_bool;

		template<typename TRawType, typename TType, typename TObject>
		struct fetch_traits {
			typedef typename TRawType raw_type;
			typedef typename TType data_type;
			typedef typename TObject object_type;
			typedef HRESULT(__stdcall TObject::*fun_ptr_type)(TRawType*);
		};
		template<typename TObject>
		struct lpwstr_traits : public fetch_traits<LPWSTR, std::string, TObject> {
			static std::string get_default() { return ""; }
			static void cleanup(LPWSTR obj) { CoTaskMemFree(obj); }
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static std::string convert(HRESULT, LPWSTR value) { return utf8::cvt<std::string>(value); }
		};
		template<typename TObject>
		struct bstr_traits : public fetch_traits<BSTR, std::string, TObject> {
			static std::string get_default() { return ""; }
			static void cleanup(LPWSTR) { /*CoTaskMemFree(obj);*/ }
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static std::string convert(HRESULT, LPWSTR value) { return utf8::cvt<std::string>(value); }
		};
		template<typename TTarget, typename TReturn, typename TObject>
		struct word_traits : public fetch_traits<TTarget, TReturn, TObject> {
			static TReturn get_default() { return 0; }
			static void cleanup(TTarget) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static TReturn convert(HRESULT, TTarget value) { return value; }
		};
		template<typename TRawType, typename TObject>
		struct date_traits : public fetch_traits<TRawType, task_sched_date, TObject> {
			typedef task_sched_date TReturn;
			static TReturn get_default() { return task_sched_date(); }
			static void cleanup(TRawType) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr) && hr != SCHED_S_TASK_HAS_NOT_RUN; }

			static unsigned long long convert_time(const SYSTEMTIME &time) {
				FILETIME localFileTime, fileTime;
				SystemTimeToFileTime(&time, &localFileTime);
				LocalFileTimeToFileTime(&localFileTime, &fileTime);
				return ((fileTime.dwHighDateTime * ((unsigned long long)MAXDWORD + 1)) + (unsigned long long)fileTime.dwLowDateTime);
			}
			static unsigned long long convert_time(const DATE &date) {
				SYSTEMTIME time;
				::VariantTimeToSystemTime(date, &time);
				return convert_time(time);
			}

			static TReturn convert(HRESULT hr, TRawType &value) {
				if (hr == SCHED_S_TASK_HAS_NOT_RUN)
					return task_sched_date(true);
				return task_sched_date(convert_time(value));
			}
		};
		template<typename TTarget, typename TReturn, typename TObject>
		struct bool_traits : public fetch_traits<TTarget, TReturn, TObject> {
			static TReturn get_default() { return false; }
			static void cleanup(TTarget) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static TReturn convert(HRESULT, TTarget value) { return value == VARIANT_TRUE; }
		};

		template<typename traits>
		struct com_variable {
			typedef typename traits::fun_ptr_type fun_ptr_type;
			typedef typename traits::data_type data_type;
			typedef typename traits::raw_type raw_type;
			typedef typename traits::object_type object_type;

			boost::optional<data_type> data_;
			fun_ptr_type fun_;

			com_variable(fun_ptr_type fun) : fun_(fun) {}

			bool inline fetch_data(object_type *object, data_type &data, const std::string &title) {
				raw_type tmp;
				HRESULT hr = (object->*fun_)(&tmp);
				if (traits::has_failed(hr)) {
					throw nsclient::nsclient_exception("Failed to fetch " + title + ": " + ::error::format::from_system(hr));
				} else {
					data = traits::convert(hr, tmp);
					traits::cleanup(tmp);
					return true;
				}
			}

			data_type operator() (object_type *object, const std::string &title) {
				if (!data_) {
					data_type tmp;
					if (fetch_data(object, tmp, title))
						data_.reset(tmp);
					else
						data_.reset(traits::get_default());
				}
				return *data_;
			}
		};
	}

	struct filter_obj {
		virtual bool is_new() = 0;

		virtual std::string get_folder() = 0;
		virtual std::string get_title() = 0;

		virtual std::string get_account_name() = 0;
		virtual std::string get_application_name() = 0;
		virtual std::string get_comment() = 0;
		virtual std::string get_creator() = 0;
		virtual std::string get_parameters() = 0;
		virtual std::string get_working_directory() = 0;

		virtual long long get_exit_code() = 0;
		virtual long long get_flags() = 0;
		virtual long long get_max_run_time() = 0;
		virtual long long get_priority() = 0;

		virtual long long is_enabled() = 0;
		virtual long long get_status() = 0;
		virtual std::string get_status_s() = 0;
		virtual long long get_most_recent_run_time() = 0;
		virtual std::string get_most_recent_run_time_s() = 0;
		virtual bool get_has_run() = 0;
	};

	struct old_filter_obj : public filter_obj {
		typedef helpers::com_variable<helpers::lpwstr_traits<ITask> > string_variable;
		typedef helpers::com_variable<helpers::word_traits<WORD, long, ITask> > word_variable;
		typedef helpers::com_variable<helpers::word_traits<DWORD, long long, ITask> > dword_variable;
		typedef helpers::com_variable<helpers::word_traits<HRESULT, long long, ITask> > hresult_variable;
		typedef helpers::com_variable<helpers::date_traits< SYSTEMTIME, ITask> > date_variable;

		ITask* task;
		std::string title;
		string_variable account_name;
		string_variable application_name;
		string_variable comment;
		string_variable creator;
		string_variable parameters;
		string_variable working_directory;

		dword_variable exit_code;
		dword_variable flags;
		dword_variable max_run_time;
		dword_variable priority;

		hresult_variable status;
		date_variable most_recent_run_time;

		old_filter_obj(ITask* task, std::string title);

		bool is_new() { return false; }

		std::string get_title() { return title; }
		std::string get_folder() { return "/"; }

		long long is_enabled() { return (get_status()&SCHED_S_TASK_DISABLED == SCHED_S_TASK_DISABLED) ? 0 : 1; }

		std::string get_account_name() { return account_name(task, title); }
		std::string get_application_name() { return application_name(task, title); }
		std::string get_comment() { return comment(task, title); }
		std::string get_creator() { return creator(task, title); }
		std::string get_parameters() { return parameters(task, title); }
		std::string get_working_directory() { return working_directory(task, title); }

		long long get_exit_code() { return exit_code(task, title); }
		long long get_flags() { return flags(task, title); }
		long long get_max_run_time() { return max_run_time(task, title); }
		long long get_priority() { return priority(task, title); }

		long long get_status() { return status(task, title); }
		std::string get_status_s() {
			long long i = get_status();
			if (i == SCHED_S_TASK_READY)
				return "ready";
			if (i == SCHED_S_TASK_RUNNING)
				return "running";
			if (i == SCHED_S_TASK_NOT_SCHEDULED)
				return "not_scheduled";
			if (i == SCHED_S_TASK_HAS_NOT_RUN)
				return "has_not_run";
			if (i == SCHED_S_TASK_DISABLED)
				return "disabled";
			if (i == SCHED_S_TASK_NO_MORE_RUNS)
				return "has_more_runs";
			if (i == SCHED_S_TASK_NO_VALID_TRIGGERS)
				return "no_valid_triggers";
			return str::xtos(i);
		}
		long long get_most_recent_run_time() { return most_recent_run_time(task, title); }
		std::string get_most_recent_run_time_s() {
			return str::format::format_date(get_most_recent_run_time());
		}
		bool get_has_run() {
			return most_recent_run_time(task, title).has_run();
		}

	};

	struct new_filter_obj : public filter_obj {
		typedef helpers::com_variable<helpers::bstr_traits<IRegisteredTask> > string_variable;
		typedef helpers::com_variable<helpers::word_traits<WORD, long, IRegisteredTask> > word_variable;
		typedef helpers::com_variable<helpers::word_traits<LONG, long, IRegisteredTask> > long_variable;
		typedef helpers::com_variable<helpers::word_traits<DWORD, long long, IRegisteredTask> > dword_variable;
		typedef helpers::com_variable<helpers::word_traits<TASK_STATE, long long, IRegisteredTask> > state_variable;
		typedef helpers::com_variable<helpers::word_traits<HRESULT, long long, IRegisteredTask> > hresult_variable;
		typedef helpers::com_variable<helpers::date_traits<DATE, IRegisteredTask> > date_variable;
		typedef helpers::com_variable<helpers::bool_traits<VARIANT_BOOL, bool, IRegisteredTask> > bool_variable;
		typedef helpers::com_variable<helpers::bstr_traits<IRegistrationInfo> > info_string_variable;
		typedef helpers::com_variable<helpers::word_traits<int, long long, ITaskSettings> > settings_int_variable;
		typedef helpers::com_variable<helpers::bstr_traits<ITaskSettings> > settings_string_variable;

		IRegisteredTask* task;
		CComPtr<IRegistrationInfo> reginfo;
		CComPtr<ITaskSettings> settings;
		CComPtr<ITaskDefinition> def;

		std::string folder;
		string_variable title;
		long_variable exit_code;
		state_variable status;
		bool_variable enabled;
		date_variable most_recent_run_time;
		info_string_variable comment;
		info_string_variable creator;
		settings_int_variable priority;
		settings_string_variable max_run_time;
		std::string str_title;

		new_filter_obj(IRegisteredTask* task, std::string folder);

		bool is_new() { return true; }

		CComPtr<IRegistrationInfo> get_reginfo();
		CComPtr<ITaskSettings> get_settings();
		CComPtr<ITaskDefinition> get_def();

		std::string get_folder() { return folder; }

		long long is_enabled() { return enabled(task, get_title()); }

		std::string get_title() { if (str_title.empty()) str_title = title(task, "unknown"); return str_title; }
		std::string get_account_name() { throw nsclient::nsclient_exception("account_name is not supported"); }
		std::string get_application_name() { throw nsclient::nsclient_exception("application_name is not supported"); }
		std::string get_comment() { return comment(get_reginfo(), get_title()); }
		std::string get_creator() { return creator(get_reginfo(), get_title()); }

		std::string get_parameters() { throw nsclient::nsclient_exception("get_parameters is not supported"); }
		std::string get_working_directory() { throw nsclient::nsclient_exception("working_directory is not supported"); }

		long long get_exit_code() { return exit_code(task, get_title()); }
		long long get_flags() { throw nsclient::nsclient_exception("flags is not supported"); }
		long long get_max_run_time() { return convert_runtime(max_run_time(get_settings(), get_title())); }
		long long get_priority() { return priority(get_settings(), get_title()); }

		long long get_status() { return status(task, get_title()); }
		std::string get_status_s() {
			long long i = get_status();
			if (i == TASK_STATE_QUEUED)
				return "queued";
			if (i == TASK_STATE_UNKNOWN)
				return "unknown";
			if (i == TASK_STATE_READY)
				return "ready";
			if (i == TASK_STATE_RUNNING)
				return "running";
			if (i == TASK_STATE_DISABLED)
				return "disabled";
			return str::xtos(i);
		}
		long long get_most_recent_run_time() { return most_recent_run_time(task, get_title()); }
		std::string get_most_recent_run_time_s() {
			return str::format::format_date(get_most_recent_run_time());
		}
		bool get_has_run() {
			return most_recent_run_time(task, get_title()).has_run();
		}

		long long convert_runtime(std::string &) {
			return 0;
		}
	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {
		static const parsers::where::value_type type_custom_state = parsers::where::type_custom_int_1;

		filter_obj_handler();
	};
	typedef modern_filter::modern_filters<tasksched_filter::filter_obj, tasksched_filter::filter_obj_handler> filter;
}