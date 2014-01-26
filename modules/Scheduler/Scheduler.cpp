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
#include <nscapi/nscapi_plugin_interface.hpp>
#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

bool Scheduler::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	sh::settings_registry settings(get_settings_proxy());
	settings.set_alias(alias, "scheduler");
	schedule_path = settings.alias().get_settings_path("schedules");

	settings.alias().add_path_to_settings()
		("SCHEDULER SECTION", "Section for the Scheduler module.")

		;

	settings.alias().add_key_to_settings()
		("threads", sh::int_fun_key<unsigned int>(boost::bind(&scheduler::simple_scheduler::set_threads, &scheduler_, _1), 5),
		"THREAD COUNT", "Number of threads to use.")
		;

	settings.alias().add_path_to_settings()
		("schedules", sh::fun_values_path(boost::bind(&Scheduler::add_schedule, this, _1, _2)), 
		"SCHEDULER SECTION", "Section for the Scheduler module.",
		"SCHEDULE", "For more configuration options add a dedicated section")
		;

	settings.register_all();
	settings.notify();

	BOOST_FOREACH(const schedules::schedule_handler::object_list_type::value_type &o, schedules_.object_list) {
		NSC_DEBUG_MSG("Adding scheduled item: " + o.second.to_string());
		scheduler_.add_task(o.second);
	}

	NSC_DEBUG_MSG_STD("Thread count: " + strEx::s::xtos(scheduler_.get_threads()));
	if (mode == NSCAPI::normalStart) {
		scheduler_.set_handler(this);
		scheduler_.start();
	}
	return true;
}

void Scheduler::add_schedule(std::string key, std::string arg) {
	try {
		schedules_.add(get_settings_proxy(), schedule_path, key, arg, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

bool Scheduler::unloadModule() {
	scheduler_.unset_handler();
	scheduler_.stop();
	return true;
}

void Scheduler::on_error(std::string error) {
	NSC_LOG_ERROR(error);
}
#include <nscapi/functions.hpp>
void Scheduler::handle_schedule(schedules::schedule_object item) {
	try {
		std::string response;
		NSCAPI::nagiosReturn code = nscapi::core_helper::simple_query(item.command.c_str(), item.arguments, response);
		if (code == NSCAPI::returnIgnored) {
			NSC_LOG_ERROR_WA("Command was not found: ", item.command);
			if (item.channel.empty()) {
				NSC_LOG_ERROR_WA("No channel specified for ", item.tpl.alias);
				return;
			}
			nscapi::protobuf::functions::create_simple_submit_request(item.channel, item.command, NSCAPI::returnUNKNOWN, "Command was not found: " + item.command, "", response);
			std::string result;
			get_core()->submit_message(item.channel, response, result);
		} else if (nscapi::report::matches(item.report, code)) {
			if (item.channel.empty()) {
				NSC_LOG_ERROR_STD("No channel specified for " + utf8::cvt<std::string>(item.tpl.alias) + " mssage will not be sent.");
				return;
			}
			// @todo: allow renaming of commands here item.alias, 
			// @todo this is broken, fix this (uses the wrong message)
			nscapi::protobuf::functions::make_submit_from_query(response, item.channel, item.tpl.alias, item.target_id, item.source_id);
			std::string result;
			NSCAPI::errorReturn ret = get_core()->submit_message(item.channel, response, result);
			if (ret != NSCAPI::isSuccess) {
				NSC_LOG_ERROR_STD("Failed to submit: " + item.tpl.alias);
				return;
			}
			std::string error;
			ret = nscapi::protobuf::functions::parse_simple_submit_response(result, error);
			if (ret != NSCAPI::isSuccess) {
				NSC_LOG_ERROR_STD("Failed to submit " + item.tpl.alias + ": "  + error);
				return;
			}
		} else {
			NSC_DEBUG_MSG("Filter not matched for: " + utf8::cvt<std::string>(item.tpl.alias) + " so nothing is reported");
		}
	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("Failed to register command: ", e);
		scheduler_.remove_task(item.id);
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Exception: ", e);
		scheduler_.remove_task(item.id);
	} catch (...) {
		NSC_LOG_ERROR_EX(item.tpl.alias);
		scheduler_.remove_task(item.id);
	}
}
