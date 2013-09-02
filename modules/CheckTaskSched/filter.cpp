#include "StdAfx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <strEx.h>
#include <format.hpp>
#include "filter.hpp"

#define DATE_FORMAT _T("%#c")
using namespace boost::assign;
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
//		("line", &filter_obj::get_line, "Match the content of an entire line")
		("title", boost::bind(&filter_obj::get_title, _1), "The task title")
		("account", boost::bind(&filter_obj::get_account_name, _1), "TODO")
		("application", boost::bind(&filter_obj::get_application_name, _1), "TODO")
		("comment", boost::bind(&filter_obj::get_comment, _1), "TODO")
		("creator", boost::bind(&filter_obj::get_creator, _1), "TODO")
		("parameters", boost::bind(&filter_obj::get_parameters, _1), "TODO")
		("working_directory", boost::bind(&filter_obj::get_working_directory, _1), "TODO")
		;

	registry_.add_int()
		("error_retry_count", boost::bind(&filter_obj::get_error_retry_count, _1), "TODO")
		("error_retry_interval", boost::bind(&filter_obj::get_error_retry_interval, _1), "TODO")
		("exit_code", boost::bind(&filter_obj::get_exit_code, _1), "TODO")
		("flags", boost::bind(&filter_obj::get_flags, _1), "TODO")
		("max_run_time", boost::bind(&filter_obj::get_max_run_time, _1), "TODO")
		("priority", boost::bind(&filter_obj::get_priority, _1), "TODO")
		("status", boost::bind(&filter_obj::get_status, _1), "TODO")
		("most_recent_run_time", boost::bind(&filter_obj::get_most_recent_run_time, _1), "TODO")
		;

// TODO: Add types!
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

//////////////////////////////////////////////////////////////////////////

#define DEFINE_GET_EX(type, variable, helper, func) type tasksched_filter::filter_obj::get_ ## variable() { return helper.fetch(this, &ITask::func, variable); }

#define DEFINE_GET_STRING(variable, helper, func) DEFINE_GET_EX(std::string, variable, helper, func)
#define DEFINE_GET_DWORD(variable, helper, func) DEFINE_GET_EX(unsigned long, variable, helper, func)
#define DEFINE_GET_WORD(variable, helper, func) DEFINE_GET_EX(unsigned short, variable, helper, func)
#define DEFINE_GET_DATE(variable, helper, func) DEFINE_GET_EX(tasksched_filter::filter_obj::task_sched_date, variable, helper, func)
#define DEFINE_GET_HRESULT(variable, helper, func) DEFINE_GET_EX(long, variable, helper, func)

DEFINE_GET_STRING(account_name, string_fetcher, GetAccountInformation);
DEFINE_GET_STRING(application_name, string_fetcher, GetApplicationName);
DEFINE_GET_STRING(comment, string_fetcher, GetComment);
DEFINE_GET_STRING(creator, string_fetcher, GetCreator);
DEFINE_GET_STRING(parameters, string_fetcher, GetParameters);
DEFINE_GET_STRING(working_directory, string_fetcher, GetWorkingDirectory);

DEFINE_GET_WORD(error_retry_count, word_fetcher, GetErrorRetryCount);
DEFINE_GET_WORD(error_retry_interval, word_fetcher, GetErrorRetryInterval);
DEFINE_GET_DWORD(exit_code, dword_fetcher, GetExitCode);
DEFINE_GET_DWORD(flags, dword_fetcher, GetFlags);
DEFINE_GET_DWORD(max_run_time, dword_fetcher, GetMaxRunTime);
DEFINE_GET_DWORD(priority, dword_fetcher, GetPriority);
//DEFINE_GET_WORD(idle_wait, word_fetcher, GetIdleWait);


DEFINE_GET_HRESULT(status, hresult_fetcher, GetStatus);

DEFINE_GET_DATE(most_recent_run_time, date_fetcher, GetMostRecentRunTime);
// FETCH_TASK_SIMPLE_TIME(nextRunTime,GetNextRunTime);
