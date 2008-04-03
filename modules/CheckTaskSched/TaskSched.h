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
#include <map>
#include <strEx.h>
#include <error.hpp>
#include <filter_framework.hpp>
#include <error_com.hpp>
#include <Mstask.h>

class TaskSched
{
public:
	struct result {
		struct fetch_key {
			bool nextRunTime;
			bool triggers;
			bool idleMinutes;
			bool deadlineMinutes;
			bool mostRecentRunTime;
			bool status;
			bool exitCode;
			bool comment;
			bool creator;
			bool data;
			bool flags;
			bool accountName;
			bool workingDirectory;
			bool priority;
			bool maxRunTime;
			bool applicationName;
			bool parameters;
			fetch_key() :
			nextRunTime(false),
			triggers(false),
			idleMinutes(false),
			deadlineMinutes(false),
			mostRecentRunTime(false),
			status(false),
			exitCode(false),
			comment(false),
			creator(false),
			data(false),
			flags(false),
			accountName(false),
			workingDirectory(false),
			priority(false),
			maxRunTime(false),
			applicationName(false),
			parameters(false)
			{}
			fetch_key(bool bval) :
			nextRunTime(bval),
				triggers(bval),
				idleMinutes(bval),
				deadlineMinutes(bval),
				mostRecentRunTime(bval),
				status(bval),
				exitCode(bval),
				comment(bval),
				creator(bval),
				data(bval),
				flags(bval),
				accountName(bval),
				workingDirectory(bval),
				priority(bval),
				maxRunTime(bval),
				applicationName(bval),
				parameters(bval)
			{}
		};
		struct task_sched_date {
			unsigned long long date_;
			bool never_;
			bool has_value_;
			task_sched_date() : never_(false), has_value_(false) {}
			void operator=(unsigned long long date) {
				has_value_ = true;
				date_ = date;
				never_ = false;
			}
			operator unsigned long long () {
				return date_;
			}
			operator const unsigned long long () const {
				return date_;
			}
			/*
			operator unsigned __int64 () {
				return date_;
			}
			*/
		};
		task_sched_date nextRunTime;
		std::list<std::wstring> triggers;
		WORD idleMinutes;
		WORD deadlineMinutes;
		task_sched_date mostRecentRunTime;
		HRESULT status;
		DWORD exitCode;
		std::wstring comment;
		std::wstring creator;
		std::wstring data;
		std::wstring parameters;
		DWORD flags;
		std::wstring accountName;
		std::wstring workingDirectory;
		DWORD priority;
		DWORD maxRunTime;
		std::wstring title;
		std::wstring applicationName;

		unsigned long long ullNow;

#define accountName_alias "account-name"
#define applicationName_alias "application-name"
#define workingDirectory_alias "working-directory"
#define exitCode_alias "exit-code"
#define nextRunTime_alias "next-run-time"
#define mostRecentRunTime_alias "most-recent-run-time"

#define REPLACER_NORMAL(key) \
	strEx::replace(format, _T("%") _T(#key) _T("%"), key);
#define REPLACER_ALIAS(key) \
	strEx::replace(format, _T("%") _T(key ## _alias) _T("%"), key);
#define REPLACER_R_NORMAL(key) \
	strEx::replace(format, _T("%") _T(#key) _T("%"), render_ ## key());
#define REPLACER_R_ALIAS(key) \
	strEx::replace(format, _T("%") _T(key ## _alias) _T("%"), render_ ## key());
#define REPLACER_I_NORMAL(key) \
	strEx::replace(format, _T("%") _T(#key) _T("%"), strEx::itos(key));
#define REPLACER_I_ALIAS(key) \
	strEx::replace(format, _T("%") _T(key ## _alias) _T("%"), strEx::itos(key));
#define REPLACER_D_EX(key, val) \
	if (key.never_ == true) \
		strEx::replace(format, _T("%") _T(key ## _alias) _T("%"), _T("never")); \
	else if (key.has_value_) { \
		strEx::replace(format, _T("%") _T(val) _T("%"), strEx::format_filetime(key)); \
	}
#define REPLACER_D_NORMAL(key) REPLACER_D_EX(key, #key)
#define REPLACER_D_ALIAS(key) REPLACER_D_EX(key, key ## _alias)
		

		std::wstring render(std::wstring format) const {
			REPLACER_NORMAL(title);
			REPLACER_ALIAS(accountName);
			REPLACER_ALIAS(applicationName);
			REPLACER_NORMAL(comment);
			REPLACER_NORMAL(creator);
			REPLACER_NORMAL(data);
			REPLACER_ALIAS(workingDirectory);
			REPLACER_I_ALIAS(exitCode);
			REPLACER_R_NORMAL(flags);
			REPLACER_NORMAL(parameters);
			//REPLACER_D_ALIAS(nextRunTime);
			REPLACER_D_ALIAS(mostRecentRunTime);

			strEx::replace(format, _T("\n"), _T(""));
			return format;
		}
		std::wstring render_exitCode() const {
			return strEx::itos(exitCode);
		}
#define RENDER_FLAG(key) if ((flags&key)!=0) strEx::append_list(ret, std::wstring(_T(#key)))

			std::wstring render_flags() const {
			std::wstring ret;
			RENDER_FLAG(TASK_FLAG_INTERACTIVE);
			RENDER_FLAG(TASK_FLAG_DELETE_WHEN_DONE);
			RENDER_FLAG(TASK_FLAG_DISABLED);
			RENDER_FLAG(TASK_FLAG_HIDDEN);
			RENDER_FLAG(TASK_FLAG_RUN_ONLY_IF_LOGGED_ON);
			RENDER_FLAG(TASK_FLAG_START_ONLY_IF_IDLE);
			RENDER_FLAG(TASK_FLAG_SYSTEM_REQUIRED);
			RENDER_FLAG(TASK_FLAG_KILL_ON_IDLE_END);
			RENDER_FLAG(TASK_FLAG_RESTART_ON_IDLE_RESUME);
			RENDER_FLAG(TASK_FLAG_DONT_START_IF_ON_BATTERIES);
			RENDER_FLAG(TASK_FLAG_KILL_IF_GOING_ON_BATTERIES);
			RENDER_FLAG(TASK_FLAG_RUN_IF_CONNECTED_TO_INTERNET);
			RENDER_FLAG(TASK_FLAG_INTERACTIVE);
			return ret;
		}
	};
	typedef std::list<result> result_type;
	struct wmi_filter {
#define STRING_FILTER(key) filters::filter_all_strings key;
#define NUMERIC_FILTER(key) filters::filter_all_numeric<unsigned long, checkHolders::int_handler >  key;
		static const __int64 MSECS_TO_100NS = 10000;

		STRING_FILTER(title);
		STRING_FILTER(accountName);
		STRING_FILTER(applicationName);
		STRING_FILTER(comment);
		STRING_FILTER(creator);
		STRING_FILTER(data);
		STRING_FILTER(workingDirectory);
		NUMERIC_FILTER(exitCode);
		NUMERIC_FILTER(flags);
		STRING_FILTER(parameters);
		filters::filter_all_times nextRunTime;
		filters::filter_all_times mostRecentRunTime;

#define HAS_FILTER(key) || key.hasFilter()

		inline bool hasFilter() {
			return false HAS_FILTER(title)
				HAS_FILTER(accountName)
				HAS_FILTER(applicationName)
				HAS_FILTER(comment)
				HAS_FILTER(creator)
				HAS_FILTER(data)
				HAS_FILTER(workingDirectory)
				HAS_FILTER(exitCode)
				HAS_FILTER(flags)
				HAS_FILTER(nextRunTime)
				HAS_FILTER(mostRecentRunTime)
				HAS_FILTER(parameters);
		}

#define MATCH_FILTER(key) if ((key.hasFilter())&&(key.matchFilter(value.key))) return true;
		bool matchFilter(const result &value) const {
			MATCH_FILTER(title);
			MATCH_FILTER(accountName);
			MATCH_FILTER(applicationName);
			MATCH_FILTER(comment);
			MATCH_FILTER(creator);
			MATCH_FILTER(data);
			MATCH_FILTER(parameters);
			MATCH_FILTER(workingDirectory);
			MATCH_FILTER(exitCode);
			MATCH_FILTER(flags);

			if ((nextRunTime.hasFilter())&&(nextRunTime.matchFilter((value.ullNow-value.nextRunTime)/MSECS_TO_100NS)))
				return true;
			if ((mostRecentRunTime.hasFilter())&&(mostRecentRunTime.matchFilter((value.ullNow-value.mostRecentRunTime)/MSECS_TO_100NS)))
				return true;

			MATCH_FILTER(parameters);
			return false;
		}

#define DEBUG_FILTER(key) if (key.hasFilter()) ss << _T("        ") << _T( # key) << _T(" = ") << key.to_string() << std::endl;

		std::wstring to_string() const {
			std::wstringstream ss; 
			ss << _T("filter = {") << std::endl;
			DEBUG_FILTER(title);
			DEBUG_FILTER(accountName);
			DEBUG_FILTER(applicationName);
			DEBUG_FILTER(comment);
			DEBUG_FILTER(creator);
			DEBUG_FILTER(data);
			DEBUG_FILTER(parameters);
			DEBUG_FILTER(workingDirectory);
			DEBUG_FILTER(exitCode);
			DEBUG_FILTER(flags);
			DEBUG_FILTER(nextRunTime);
			DEBUG_FILTER(mostRecentRunTime);
			DEBUG_FILTER(parameters);
			ss << _T("    }") << std::endl;
			return ss.str();
		}
	};
	class Exception {
		std::wstring message_;
	public:
		Exception(std::wstring str, HRESULT code) {
			message_ = str + _T(":") + error::format::from_system(code);
		}
		std::wstring getMessage() {
			return message_;
		}
	};

	result_type findAll(result::fetch_key key);
	std::wstring sanitize_string(LPTSTR in);

};
