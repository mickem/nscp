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

#include <map>
#include <list>

#include <parsers/where/node.hpp>

#include "filter.hpp"
#include <error/error_com.hpp>

using namespace parsers::where;

node_type fun_convert_status(boost::shared_ptr<tasksched_filter::filter_obj> object, evaluation_context context, node_type subject) {
	std::string status = subject->get_string_value(context);
	long long istat = 0;
	if (object->is_new()) {
		if (status == "queued")
			istat = TASK_STATE_QUEUED;
		else if (status == "unknown")
			istat = TASK_STATE_UNKNOWN;
		else if (status == "ready")
			istat = TASK_STATE_READY;
		else if (status == "running")
			istat = TASK_STATE_RUNNING;
		else if (status == "disabled")
			istat = TASK_STATE_DISABLED;
		else
			context->error("Failed to convert: " + status);
	} else {
		if (status == "ready")
			istat = SCHED_S_TASK_READY;
		else if (status == "running")
			istat = SCHED_S_TASK_RUNNING;
		else if (status == "not_scheduled")
			istat = SCHED_S_TASK_NOT_SCHEDULED;
		else if (status == "has_not_run")
			istat = SCHED_S_TASK_HAS_NOT_RUN;
		else if (status == "disabled")
			istat = SCHED_S_TASK_DISABLED;
		else if (status == "no_more_runs")
			istat = SCHED_S_TASK_NO_MORE_RUNS;
		else if (status == "no_valid_triggers")
			istat = SCHED_S_TASK_NO_VALID_TRIGGERS;
		else
			context->error("Failed to convert: " + status);
	}
	return factory::create_int(istat);
}

tasksched_filter::filter_obj_handler::filter_obj_handler() {
	registry_.add_string()
		("folder", boost::bind(&filter_obj::get_folder, _1), "The task folder")
		("title", boost::bind(&filter_obj::get_title, _1), "The task title")
		//		("account", boost::bind(&filter_obj::get_account_name, _1), "Retrieves the account name for the work item.")
		("application", boost::bind(&filter_obj::get_application_name, _1), "Retrieves the name of the application that the task is associated with.")
		("comment", boost::bind(&filter_obj::get_comment, _1), "Retrieves the comment or description for the work item.")
		("creator", boost::bind(&filter_obj::get_creator, _1), "Retrieves the creator of the work item.")
		("parameters", boost::bind(&filter_obj::get_parameters, _1), "Retrieves the command-line parameters of a task.")
		("working_directory", boost::bind(&filter_obj::get_working_directory, _1), "Retrieves the working directory of the task.")
		;

	registry_.add_int()
		("exit_code", boost::bind(&filter_obj::get_exit_code, _1), "Retrieves the work item's last exit code.")
		("enabled", boost::bind(&filter_obj::is_enabled, _1), "TODO.")
		// 		("flags", boost::bind(&filter_obj::get_flags, _1), "TODO")
		("max_run_time", boost::bind(&filter_obj::get_max_run_time, _1), "Retrieves the maximum length of time the task can run.")
		("priority", boost::bind(&filter_obj::get_priority, _1), "Retrieves the priority for the task.")
		("task_status", type_custom_state, boost::bind(&filter_obj::get_status, _1), "Retrieves the status of the work item.")
		("most_recent_run_time", type_date, boost::bind(&filter_obj::get_most_recent_run_time, _1), "Retrieves the most recent time the work item began running.")
		("has_run", type_bool, boost::bind(&filter_obj::get_has_run, _1), "True if the task has ever executed.")
		;

	registry_.add_human_string()
		("task_status", boost::bind(&filter_obj::get_status_s, _1), "")
		("most_recent_run_time", boost::bind(&filter_obj::get_most_recent_run_time_s, _1), "")
		;

	registry_.add_converter()
		(type_custom_state, &fun_convert_status)
		// 		(type_int, type_custom_hresult, &fun_convert_status)
		;
}

namespace tasksched_filter {
	CComPtr<IRegistrationInfo> new_filter_obj::get_reginfo() {
		if (reginfo)
			return reginfo;
		HRESULT hr = get_def()->get_RegistrationInfo(&reginfo);
		if (!SUCCEEDED(hr))
			throw nsclient::nsclient_exception("Failed to get IRegistrationInfo: " + error::com::get(hr));
		return reginfo;
	}

	CComPtr<ITaskDefinition> new_filter_obj::get_def() {
		if (def)
			return def;
		HRESULT hr = task->get_Definition(&def);
		if (!SUCCEEDED(hr))
			throw nsclient::nsclient_exception("Failed to get ITaskDefinition: " + error::com::get(hr));
		return def;
	}

	CComPtr<ITaskSettings> new_filter_obj::get_settings() {
		if (settings)
			return settings;
		HRESULT hr = get_def()->get_Settings(&settings);
		if (!SUCCEEDED(hr))
			throw nsclient::nsclient_exception("Failed to get ITaskSettings: " + error::com::get(hr));
		return settings;
	}

	old_filter_obj::old_filter_obj(ITask* task, std::string title)
		: task(task)
		, title(title)
		, account_name(&ITask::GetAccountInformation)
		, application_name(&ITask::GetApplicationName)
		, comment(&ITask::GetComment)
		, creator(&ITask::GetCreator)
		, parameters(&ITask::GetParameters)
		, working_directory(&ITask::GetWorkingDirectory)
		, exit_code(&ITask::GetExitCode)
		, flags(&ITask::GetFlags)
		, max_run_time(&ITask::GetMaxRunTime)
		, priority(&ITask::GetPriority)
		, status(&ITask::GetStatus)
		, most_recent_run_time(&ITask::GetMostRecentRunTime) {}

	new_filter_obj::new_filter_obj(IRegisteredTask* task, std::string folder)
		: task(task)
		, folder(folder)
		, title(&IRegisteredTask::get_Name)
		, exit_code(&IRegisteredTask::get_LastTaskResult)
		, status(&IRegisteredTask::get_State)
		, enabled(&IRegisteredTask::get_Enabled)
		, most_recent_run_time(&IRegisteredTask::get_LastRunTime)
		, comment(&IRegistrationInfo::get_Description)
		, creator(&IRegistrationInfo::get_Author)
		, priority(&ITaskSettings::get_Priority)
		, max_run_time(&ITaskSettings::get_ExecutionTimeLimit) {}
}