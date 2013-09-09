#include "StdAfx.h"

#include <map>
#include <list>

#include <parsers/where/node.hpp>

#include "filter.hpp"
#include <error_com.hpp>

using namespace parsers::where;

long convert_status(std::string status) {
	if (status == "ready")
		return SCHED_S_TASK_READY;
	if (status == "running")
		return SCHED_S_TASK_RUNNING;
	if (status == "not_scheduled")
		return SCHED_S_TASK_NOT_SCHEDULED;
	if (status == "has_not_run")
		return SCHED_S_TASK_HAS_NOT_RUN;
	if (status == "disabled")
		return SCHED_S_TASK_DISABLED;
	if (status == "no_more_runs")
		return SCHED_S_TASK_NO_MORE_RUNS;
	if (status == "no_valid_triggers")
		return SCHED_S_TASK_NO_VALID_TRIGGERS;
	return 0;
}

std::string convert_status(long status) {
	std::wstring ret;
	if (status == SCHED_S_TASK_READY)
		return "ready";
	if (status == SCHED_S_TASK_RUNNING)
		return "running";
	if (status == SCHED_S_TASK_NOT_SCHEDULED)
		return "not_scheduled";
	if (status == SCHED_S_TASK_HAS_NOT_RUN)
		return "has_not_run";
	if (status == SCHED_S_TASK_DISABLED)
		return "disabled";
	if (status == SCHED_S_TASK_NO_MORE_RUNS)
		return "has_more_runs";
	if (status == SCHED_S_TASK_NO_VALID_TRIGGERS)
		return "no_valid_triggers";
	return strEx::s::xtos(status);
}
node_type fun_convert_status(const value_type target_type, evaluation_context context, const node_type subject) {
	return factory::create_int(convert_status(subject->get_string_value(context)));
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
 		("status", boost::bind(&filter_obj::get_status, _1), "Retrieves the status of the work item.")
 		("most_recent_run_time", boost::bind(&filter_obj::get_most_recent_run_time, _1), "Retrieves the most recent time the work item began running.")
		;

// 	registr_.add_conversions()
// 		(parsers::where::type_string, type_custom_hresult, &fun_convert_status)
// 		(parsers::where::type_int, type_custom_hresult, &fun_convert_status)
// 		;
// 	if ((from == parsers::where::type_string)&&(to == type_custom_hresult))
// 		return true;
// 	if ((from == parsers::where::type_int)&&(to == type_custom_hresult))
// 		return true;
// 	return false;

}

namespace tasksched_filter {

	CComPtr<IRegistrationInfo> new_filter_obj::get_reginfo() {
		if (reginfo)
			return reginfo;
		if (!SUCCEEDED(get_def()->get_RegistrationInfo(&reginfo)))
			throw nscp_exception("Failed to get IRegistrationInfo: " + error::com::get());
		return reginfo;
	}

	CComPtr<ITaskDefinition> new_filter_obj::get_def() {
		if (def)
			return def;
		if (!SUCCEEDED(task->get_Definition(&def)))
			throw nscp_exception("Failed to get ITaskDefinition: " + error::com::get());
		return def;
	}

	CComPtr<ITaskSettings> new_filter_obj::get_settings() {
		if (settings)
			return settings;
		if (!SUCCEEDED(get_def()->get_Settings(&settings)))
			throw nscp_exception("Failed to get ITaskSettings: " + error::com::get());
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
		, most_recent_run_time(&ITask::GetMostRecentRunTime)
	{}

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
		, max_run_time(&ITaskSettings::get_ExecutionTimeLimit)
	{}

}
