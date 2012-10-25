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
#include "stdafx.h"
#include "Scheduler.h"
#include <strEx.h>
#include <time.h>
#include <utils.h>
#include <settings/macros.h>

#include <nscapi/nscapi_core_helper.hpp>
#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

bool Scheduler::loadModule() {
	return false;
}

bool Scheduler::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(alias, _T("scheduler"));
		schedule_path = settings.alias().get_settings_path(_T("schedules"));

		settings.alias().add_path_to_settings()
			(_T("SCHEDULER SECTION"), _T("Section for the Scheduler module."))

			;

		settings.alias().add_key_to_settings()
			(_T("threads"), sh::int_fun_key<unsigned int>(boost::bind(&scheduler::simple_scheduler::set_threads, &scheduler_, _1), 5),
			_T("THREAD COUNT"), _T("Number of threads to use."))
			;

		settings.alias().add_path_to_settings()
			(_T("schedules"), sh::fun_values_path(boost::bind(&Scheduler::add_schedule, this, _1, _2)), 
			_T("SCHEDULER SECTION"), _T("Section for the Scheduler module."))
			;

		settings.register_all();
		settings.notify();

		BOOST_FOREACH(const schedules::schedule_handler::object_list_type::value_type &o, schedules_.object_list) {
			NSC_DEBUG_MSG(_T("Adding scheduled item: ") + o.second.to_wstring());
			scheduler_.add_task(o.second);
		}

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
		return false;
	}
	try {
		NSC_DEBUG_MSG_STD(_T("Thread count: ") + boost::lexical_cast<std::wstring>(scheduler_.get_threads()));
		if (mode == NSCAPI::normalStart) {
			scheduler_.set_handler(this);
			scheduler_.start();
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown Exception in module Scheduler!"));
		return false;
	}
	return true;
}

void Scheduler::add_schedule(std::wstring key, std::wstring arg) {
	try {
		schedules_.add(get_settings_proxy(), schedule_path, key, arg, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

bool Scheduler::unloadModule() {
	scheduler_.unset_handler();
	scheduler_.stop();
	return true;
}

void Scheduler::on_error(std::wstring error) {
	NSC_LOG_ERROR_STD(error);
}
#include <nscapi/functions.hpp>
void Scheduler::handle_schedule(schedules::schedule_object item) {
	try {
		std::string response;
		NSCAPI::nagiosReturn code = nscapi::core_helper::simple_query(item.command.c_str(), item.arguments, response);
		if (code == NSCAPI::returnIgnored) {
			NSC_LOG_ERROR_STD(_T("Command was not found: ") + item.command.c_str());
			if (item.channel.empty()) {
				NSC_LOG_ERROR_STD(_T("No channel specified for ") + item.alias + _T(" mssage will not be sent."));
				return;
			}
			//make_submit_from_query(response, item.channel, item.alias);
			nscapi::functions::create_simple_submit_request(item.channel, item.command, NSCAPI::returnUNKNOWN, _T("Command was not found: ") + item.command, _T(""), response);
			std::string result;
			get_core()->submit_message(item.channel, response, result);
		} else if (nscapi::report::matches(item.report, code)) {
			if (item.channel.empty()) {
				NSC_LOG_ERROR_STD(_T("No channel specified for ") + item.alias + _T(" mssage will not be sent."));
				return;
			}
			// @todo: allow renaming of commands here item.alias, 
			// @todo this is broken, fix this (uses the wrong message)
			std::wstring alias = item.alias;
			nscapi::functions::make_submit_from_query(response, item.channel, item.alias, item.target_id);
			std::string result;
			get_core()->submit_message(item.channel, response, result);
		} else {
			NSC_DEBUG_MSG(_T("Filter not matched for: ") + item.alias + _T(" so nothing is reported"));
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + utf8::cvt<std::wstring>(e.what()));
		scheduler_.remove_task(item.id);
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		scheduler_.remove_task(item.id);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown Exception handling: ") + item.alias);
		scheduler_.remove_task(item.id);
	}
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(Scheduler, _T("scheduler"))
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_IGNORE_CMD_DEF()
