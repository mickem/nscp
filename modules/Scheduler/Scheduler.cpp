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

Scheduler gInstance;

bool Scheduler::loadModule() {
	return false;
}

bool Scheduler::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		SETTINGS_REG_PATH(scheduler::SECTION);
		SETTINGS_REG_PATH(scheduler::DEFAULT_SCHEDULE_SECTION);
		SETTINGS_REG_PATH(scheduler::SCHEDULES_SECTION);

		SETTINGS_REG_KEY_S(scheduler::INTERVAL_D);
		SETTINGS_REG_KEY_S(scheduler::COMMAND_D);
		SETTINGS_REG_KEY_S(scheduler::CHANNEL_D);
		SETTINGS_REG_KEY_S(scheduler::REPORT_MODE_D);

		SETTINGS_REG_KEY_I(scheduler::THREADS);
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}


	try {

		scheduler_.set_threads(SETTINGS_GET_INT(scheduler::THREADS));
		NSC_DEBUG_MSG_STD(_T("Thread count: ") + to_wstring(scheduler_.get_threads()));

		if (mode == NSCAPI::normalStart) {
			scheduler_.set_handler(this);
			scheduler_.start();
		}

		bool found = false;
		scheduler::target def = read_defaut_schedule(setting_keys::scheduler::DEFAULT_SCHEDULE_SECTION_PATH);
		std::list<std::wstring> items = GET_CORE()->getSettingsSection(setting_keys::scheduler::SCHEDULES_SECTION_PATH);

		for (std::list<std::wstring>::const_iterator cit = items.begin(); cit != items.end(); ++cit) {
			found = true;
			add_schedule(*cit, GET_CORE()->getSettingsString(setting_keys::scheduler::SCHEDULES_SECTION_PATH, *cit, _T("")), def);
		}

		if (!found) {
			NSC_DEBUG_MSG_STD(_T("No scheduled commands found!"));
			SETTINGS_REG_KEY_S(scheduler::INTERVAL);
			SETTINGS_REG_KEY_S(scheduler::COMMAND);
			SETTINGS_REG_KEY_S(scheduler::CHANNEL);
			SETTINGS_REG_KEY_S(scheduler::REPORT_MODE);

		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception in module Scheduler: ") + e.msg_);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown Exception in module Scheduler!"));
		return false;
	}
	return true;
}

scheduler::target Scheduler::read_defaut_schedule(std::wstring path) {
	scheduler::target item;
	item.channel = GET_CORE()->getSettingsString(path, setting_keys::scheduler::CHANNEL_D, setting_keys::scheduler::CHANNEL_D_DEFAULT);
	item.command = GET_CORE()->getSettingsString(path, setting_keys::scheduler::COMMAND_D, setting_keys::scheduler::COMMAND_D_DEFAULT);
	std::wstring report = GET_CORE()->getSettingsString(path, setting_keys::scheduler::REPORT_MODE_D, setting_keys::scheduler::REPORT_MODE_D_DEFAULT);
	item.report = nscapi::report::parse(report);
	std::wstring duration = GET_CORE()->getSettingsString(path, setting_keys::scheduler::INTERVAL_D, setting_keys::scheduler::INTERVAL_D_DEFAULT);
	item.duration = boost::posix_time::seconds(strEx::stoui_as_time_sec(duration, 1));
	return item;
}
void Scheduler::add_schedule(std::wstring alias, std::wstring command, scheduler::target def) {
	scheduler::target item;	
	std::wstring detail_path = setting_keys::scheduler::SCHEDULES_SECTION_PATH + _T("/") + alias;
	item.alias = alias;
	item.command = command;
	item.channel = GET_CORE()->getSettingsString(detail_path, setting_keys::scheduler::CHANNEL, def.channel);
	item.command = GET_CORE()->getSettingsString(detail_path, setting_keys::scheduler::COMMAND, item.command);

	strEx::parse_command(item.command, item.command, item.arguments);

	std::wstring report = GET_CORE()->getSettingsString(detail_path, setting_keys::scheduler::REPORT_MODE, nscapi::report::to_string(def.report));
	item.report = nscapi::report::parse(report);
	std::wstring duration = GET_CORE()->getSettingsString(detail_path, setting_keys::scheduler::INTERVAL, to_wstring(def.duration.total_seconds()) + _T("s"));
	item.duration = boost::posix_time::seconds(strEx::stoui_as_time_sec(duration, 1));
	scheduler_.add_task(item);
}

bool Scheduler::unloadModule() {
	scheduler_.unset_handler();
	scheduler_.stop();
	return true;
}

void Scheduler::on_error(std::wstring error) {
	NSC_LOG_ERROR_STD(error);
}

void Scheduler::handle_schedule(scheduler::target item) {
	try {
		std::string response;
		NSCAPI::nagiosReturn code = GET_CORE()->InjectCommand(item.command.c_str(), item.arguments, response);
		if (nscapi::report::matches(item.report, code)) {
			GET_CORE()->NotifyChannel(item.channel, item.alias, code, response);
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception handling: ") + item.alias + _T(": ") + e.msg_);
		scheduler_.remove_task(item.id);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown Exception handling: ") + item.alias);
		scheduler_.remove_task(item.id);
	}
}




NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gInstance);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
