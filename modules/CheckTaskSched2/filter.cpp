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
		return 3;
	if (status == "running")
		return 4;
	if (status == "unknown")
		return 0;
	if (status == "disabled")
		return 1;
	if (status == "queued")
		return 2;
	return strEx::s::stox<long>(status);
}

std::string convert_status(long status) {
	std::string ret;
	if (status == 3)
		return "ready";
	if (status == 4)
		return "running";
	if (status == 0)
		return "unknown";
	if (status == 1)
		return "disabled";
	if (status == 2)
		return "queued";
	return strEx::s::xtos(status);
}
node_type fun_convert_status(const value_type target_type, evaluation_context context, const node_type subject) {
	return factory::create_int(convert_status(subject->get_string_value(context)));
}



tasksched_filter::filter_obj_handler::filter_obj_handler() {

	registry_.add_string()
//		("line", &filter_obj::get_line, "Match the content of an entire line")
		("title", boost::bind(&filter_obj::get_title, _1), "The task title")
		("folder", boost::bind(&filter_obj::get_folder, _1), "The folder of the task")
// 		("account", &filter_obj::get_account_name, "TODO")
// 		("application", &filter_obj::get_application_name, "TODO")
// 		("comment", &filter_obj::get_comment, "TODO")
// 		("creator", &filter_obj::get_creator, "TODO")
// 		("parameters", &filter_obj::get_parameters, "TODO")
// 		("working_directory", &filter_obj::get_working_directory, "TODO")
		;

	registry_.add_int()
// 		("error_retry_count", &filter_obj::get_error_retry_count, "TODO")
// 		("error_retry_interval", &filter_obj::get_error_retry_interval, "TODO")
		("exit_code", boost::bind(&filter_obj::get_exit_code, _1), "TODO")
		("enabled", boost::bind(&filter_obj::get_enabled, _1), "If task is enabled")
// 		("flags", &filter_obj::get_flags, "TODO")
// 		("max_run_time", &filter_obj::get_max_run_time, "TODO")
// 		("priority", &filter_obj::get_priority, "TODO")
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

#define DEFINE_GET_EX(type, variable, helper, func) type tasksched_filter::filter_obj::get_ ## variable() { return helper.fetch(this, &IRegisteredTask::func, variable); }

#define DEFINE_GET_STRING(variable, helper, func) DEFINE_GET_EX(std::string, variable, helper, func)
#define DEFINE_GET_DWORD(variable, helper, func) DEFINE_GET_EX(unsigned long, variable, helper, func)
#define DEFINE_GET_WORD(variable, helper, func) DEFINE_GET_EX(unsigned short, variable, helper, func)
#define DEFINE_GET_DATE(variable, helper, func) DEFINE_GET_EX(tasksched_filter::filter_obj::task_sched_date, variable, helper, func)
#define DEFINE_GET_HRESULT(variable, helper, func) DEFINE_GET_EX(long, variable, helper, func)
#define DEFINE_GET_BOOL(variable, helper, func) DEFINE_GET_EX(bool, variable, helper, func)

DEFINE_GET_STRING(title, string_fetcher, get_Name);
DEFINE_GET_HRESULT(exit_code, hresult_fetcher, get_LastTaskResult);
DEFINE_GET_WORD(status, state_fetcher, get_State);
DEFINE_GET_DATE(most_recent_run_time, date_fetcher, get_LastRunTime);
DEFINE_GET_BOOL(enabled, bool_fetcher, get_Enabled);




