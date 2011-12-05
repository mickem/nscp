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

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

bool Scheduler::loadModule() {
	return false;
}

bool Scheduler::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		typedef std::map<std::wstring,std::wstring> schedule_map;
		schedule_map schedules;
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("scheduler"), alias);

		settings.alias().add_path_to_settings()
			(_T("SCHEDULER SECTION"), _T("Section for the Scheduler module."))

			;

		settings.alias().add_key_to_settings()
			(_T("threads"), sh::int_fun_key<unsigned int>(boost::bind(&scheduler::simple_scheduler::set_threads, &scheduler_, _1), 5),
			_T("THREAD COUNT"), _T("Number of threads to use."))
			;

		scheduler::target def = read_schedule(settings.alias().get_settings_path(_T("default")), _T("Default schedule"));

		std::wstring sch_path = settings.alias().get_settings_path(_T("schedules"));

		settings.alias().add_path_to_settings()
			(_T("schedules"), sh::fun_values_path(boost::bind(&Scheduler::add_schedule, this, sch_path, _1, _2, def)), 
			_T("SCHEDULER SECTION"), _T("Section for the Scheduler module."))
			;

		settings.register_all();
		settings.notify();

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

scheduler::target Scheduler::read_schedule(std::wstring path, std::wstring schedule_name, scheduler::target *def) {

	scheduler::target item;
	std::wstring report, duration;

	sh::settings_registry settings(get_settings_proxy());

	settings.path(path).add_path()
		(_T("SCHEDULE DEFENITION"), _T("Schedule defenition for: ") + schedule_name)
		;

	settings.path(path).add_key()
		(_T("channel"), sh::wstring_key(&item.channel, def==NULL?_T("NSCA"):def->channel),
		_T("SCHEDULE CHANNEL"), _T("Channel to send results on"))

		(_T("command"), sh::wstring_key(&item.command, def==NULL?_T("check_ok"):def->command),
		_T("SCHEDULE COMMAND"), _T("Command to execute"))

		(_T("alias"), sh::wstring_key(&item.alias, def==NULL?_T(""):def->alias),
		_T("SCHEDULE ALIAS"), _T("The alias (service name) to report to server"))

		(_T("report"), sh::wstring_key(&report, def==NULL?_T("all"):nscapi::report::to_string(def->report)),
		_T("REPORT MODE"), _T("What to report to the server (any of the following: all, critical, warning, unknown, ok)"))

		// TODO: get the proper default value here!
		(_T("interval"), sh::wstring_key(&duration, _T("5s")),
		_T("SCHEDULE INTERAVAL"), _T("Time in seconds between each check"))

		;

	settings.register_all();
	settings.notify();

	item.report = nscapi::report::parse(report);
	item.duration = boost::posix_time::seconds(strEx::stoui_as_time_sec(duration, 1));
	return item;
}
void Scheduler::add_schedule(std::wstring path, std::wstring alias, std::wstring command, scheduler::target def) {
	try {
		def.alias = alias;
		def.command = command;
		scheduler::target item = read_schedule(path + _T("/") + alias, alias, &def);
		strEx::parse_command(item.command, item.command, item.arguments);
		NSC_DEBUG_MSG_STD(_T("Adding scheduled task: ") + alias);
		scheduler_.add_task(item);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add schedule: ") + alias);
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
void Scheduler::handle_schedule(scheduler::target item) {
	try {
		std::string response;
		NSCAPI::nagiosReturn code = GET_CORE()->simple_query(item.command.c_str(), item.arguments, response);
		if (code == NSCAPI::returnIgnored) {
			NSC_LOG_ERROR_STD(_T("Command was not found: ") + item.command.c_str());
			//make_submit_from_query(response, item.channel, item.alias);
			nscapi::functions::create_simple_submit_request(item.channel, item.command, NSCAPI::returnUNKNOWN, _T("Command was not found: ") + item.command, _T(""), response);
			std::string result;
			GET_CORE()->submit_message(item.channel, response, result);
		} else if (nscapi::report::matches(item.report, code)) {
			// @todo: allow renaming of commands here item.alias, 
			// @todo this is broken, fix this (uses the wrong message)
			nscapi::functions::make_submit_from_query(response, item.channel, item.alias);
			std::string result;
			GET_CORE()->submit_message(item.channel, response, result);
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




NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(Scheduler);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
