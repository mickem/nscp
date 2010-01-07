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

bool Scheduler::loadModule(NSCAPI::moduleLoadMode mode) {
	try {
		SETTINGS_REG_PATH(scheduler::SECTION);
		SETTINGS_REG_PATH(scheduler::DEFAULT_SCHEDULE_SECTION);
		SETTINGS_REG_PATH(scheduler::SCHEDULES_SECTION);

		SETTINGS_REG_KEY_S(scheduler::INTERVAL_D);
		SETTINGS_REG_KEY_S(scheduler::COMMAND_D);
		SETTINGS_REG_KEY_S(scheduler::CHANNEL_D);
		SETTINGS_REG_KEY_S(scheduler::REPORT_MODE_D);

		SETTINGS_REG_KEY_I(scheduler::THREADS);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}


	try {

		scheduler_.set_threads(SETTINGS_GET_INT(scheduler::THREADS));

		if (mode == NSCAPI::normalStart) {
			scheduler_.start();
		}

		bool found = false;
		scheduler::target def = read_schedule(setting_keys::scheduler::DEFAULT_SCHEDULE_SECTION_PATH);
		std::list<std::wstring> items = NSCModuleHelper::getSettingsSection(setting_keys::scheduler::SCHEDULES_SECTION_PATH);

		for (std::list<std::wstring>::const_iterator cit = items.begin(); cit != items.end(); ++cit) {
			found = true;
			add_schedule(*cit, def);
		}

		if (!found) {
			NSC_DEBUG_MSG_STD(_T("No scheduled commands found!"));
			SETTINGS_REG_KEY_S(scheduler::INTERVAL);
			SETTINGS_REG_KEY_S(scheduler::COMMAND);
			SETTINGS_REG_KEY_S(scheduler::CHANNEL);
			SETTINGS_REG_KEY_S(scheduler::REPORT_MODE);

		}
/*
		add_schedule(_T("test: FIRST"));
		for (int i=0;i<1000;i++)
			add_schedule(_T("test: ") + to_wstring(i));
		add_schedule(_T("test: LAST"));
		*/
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Exception in module Scheduler: ") + e.msg_);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown Exception in module Scheduler!"));
		return false;
	}
	return true;
}

scheduler::target Scheduler::read_schedule(std::wstring path) {
	scheduler::target item;
	item.channel = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::CHANNEL, setting_keys::scheduler::CHANNEL_DEFAULT);
	item.command = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::COMMAND, setting_keys::scheduler::COMMAND_PATH);
	/*
	std::wstring report = SETTINGS_GET_STRING(scheduler::REPORT_MODE);
	item.report = parse_report_string(report);
	*/
	std::wstring duration = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::INTERVAL, setting_keys::scheduler::INTERVAL_DEFAULT);
	item.duration = boost::posix_time::seconds(strEx::stoui_as_time(duration));
	return item;
}
scheduler::target Scheduler::read_schedule(std::wstring path, scheduler::target def) {
	scheduler::target item;
	
	item.channel = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::CHANNEL, def.channel);
	item.command = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::COMMAND, def.command);
	/*
	std::wstring report = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::REPORT_MODE, def.report);
	item.report = parse_report_string(report);
	*/
	std::wstring duration = NSCModuleHelper::getSettingsString(path, setting_keys::scheduler::INTERVAL, to_wstring(def.duration.total_seconds()) + _T("s"));
	item.duration = boost::posix_time::seconds(strEx::stoui_as_time(duration));
	return item;
}

void Scheduler::add_schedule(std::wstring command, scheduler::target def) {
	NSC_DEBUG_MSG_STD(_T("Adding scheduled command: ") + command);
	scheduler::target item = read_schedule(setting_keys::scheduler::SCHEDULES_SECTION_PATH + _T("/") + command, def);

// 	std::wstring report = SETTINGS_GET_STRING(scheduler::REPORT_MODE);
// 	report_ = parse_report_string(report);

	item.command = command;
	item.set_duration(boost::posix_time::seconds(5));
	scheduler_.add_task(item);
	/*
	std::wcout << _T("*** DURATION ") << item.duration << _T(" ***") << std::endl;
	*/
}
bool Scheduler::unloadModule() {
	scheduler_.stop();
	return true;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gInstance);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
