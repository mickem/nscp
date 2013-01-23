#include "StdAfx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>


#include <parsers/where.hpp>
#include <parsers/filter/where_filter.hpp>
#include <parsers/filter/where_filter_impl.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <strEx.h>
#include "filter.hpp"

#define DATE_FORMAT _T("%#c")
using namespace boost::assign;
using namespace parsers::where;

tasksched_filter::filter_obj_handler::filter_obj_handler() {
		insert(types)
			(_T("title"), (type_string))
// 			(_T("account"), (type_string))
// 			(_T("application"), (type_string))
// 			(_T("comment"), (type_string))
// 			(_T("creator"), (type_string))
// 			(_T("parameters"), (type_string))
// 			(_T("working_directory"), (type_string))
// 			(_T("error_retry_count"), (type_int))
// 			(_T("error_retry_interval"), (type_int))
//			(_T("idle_wait"), (type_int))
			(_T("exit_code"), (type_int))
// 			(_T("flags"), (type_int))
// 			(_T("max_run_time"), (type_int))
// 			(_T("priority"), (type_int))
			(_T("status"), (type_custom_hresult))
			(_T("most_recent_run_time"), (type_date));
	}

bool tasksched_filter::filter_obj_handler::has_variable(std::wstring key) {
	return types.find(key) != types.end();
}
parsers::where::value_type tasksched_filter::filter_obj_handler::get_type(std::wstring key) {
	types_type::const_iterator cit = types.find(key);
	if (cit == types.end())
		return parsers::where::type_invalid;
	return cit->second;
}
bool tasksched_filter::filter_obj_handler::can_convert(parsers::where::value_type from, parsers::where::value_type to) {
	if ((from == parsers::where::type_string)&&(to == type_custom_hresult))
		return true;
	if ((from == parsers::where::type_int)&&(to == type_custom_hresult))
		return true;
	return false;
}

tasksched_filter::filter_obj_handler::base_handler::bound_string_type tasksched_filter::filter_obj_handler::bind_simple_string(std::wstring key) {
	base_handler::bound_string_type ret;
	if (key == _T("title"))
		ret = &object_type::get_title;
// 	else if (key == _T("account"))
// 		ret = &object_type::get_account_name;
// 	else if (key == _T("application"))
// 		ret = &object_type::get_application_name;
// 	else if (key == _T("comment"))
// 		ret = &object_type::get_comment;
// 	else if (key == _T("creator"))
// 		ret = &object_type::get_creator;
// 	else if (key == _T("parameters"))
// 		ret = &object_type::get_parameters;
// 	else if (key == _T("working_directory"))
// 		ret = &object_type::get_working_directory;
  	else
		NSC_DEBUG_MSG_STD(_T("Failed to bind (string): ") + key);
	return ret;
}


tasksched_filter::filter_obj_handler::base_handler::bound_int_type tasksched_filter::filter_obj_handler::bind_simple_int(std::wstring key) {
	base_handler::bound_int_type ret;
// 	if (key == _T("error_retry_count"))
// 		ret = &object_type::get_error_retry_count;
// 	else if (key == _T("error_retry_interval"))
// 		ret = &object_type::get_error_retry_interval;
// // 	else if (key == _T("idle_wait"))
// // 		ret = &object_type::get_idle_wait;
	if (key == _T("exit_code"))
		ret = &object_type::get_exit_code;
// 	else if (key == _T("flags"))
// 		ret = &object_type::get_flags;
// 	else if (key == _T("max_run_time"))
// 		ret = &object_type::get_max_run_time;
// 	else if (key == _T("priority"))
// 		ret = &object_type::get_priority;
 	else if (key == _T("status"))
 		ret = &object_type::get_status;
	else if (key == _T("most_recent_run_time"))
		ret = &object_type::get_most_recent_run_time;
 	else
		NSC_DEBUG_MSG_STD(_T("Failed to bind (int): ") + key);
	return ret;
}

bool tasksched_filter::filter_obj_handler::has_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	if (to == type_custom_hresult)
		return true;
	return false;
}

long tasksched_filter::filter_obj::convert_status(std::wstring status) {
	if (status == _T("ready"))
		return 3;
	if (status == _T("running"))
		return 4;
	if (status == _T("unknown"))
		return 0;
	if (status == _T("disabled"))
		return 1;
	if (status == _T("queued"))
		return 2;
	return strEx::stoi(status);
}

std::wstring tasksched_filter::filter_obj::convert_status(long status) {
	std::wstring ret;
	if (status == 3)
		return _T("ready");
	if (status == 4)
		return _T("running");
	if (status == 0)
		return _T("unknown");
	if (status == 1)
		return _T("disabled");
	if (status == 2)
		return _T("queued");
	return strEx::itos(status);
}

tasksched_filter::filter_obj_handler::base_handler::bound_function_type tasksched_filter::filter_obj_handler::bind_simple_function(parsers::where::value_type to, std::wstring name, expression_ast_type *subject) {
	base_handler::bound_function_type ret;
	if (to == type_custom_hresult)
		ret = &object_type::fun_convert_status;
	else
		NSC_DEBUG_MSG_STD(_T("Failed to bind (function): ") + name);
	return ret;
}





//////////////////////////////////////////////////////////////////////////

#define DEFINE_GET_EX(type, variable, helper, func) type tasksched_filter::filter_obj::get_ ## variable() { return helper.fetch(this, &IRegisteredTask::func, variable); }

#define DEFINE_GET_STRING(variable, helper, func) DEFINE_GET_EX(std::wstring, variable, helper, func)
#define DEFINE_GET_DWORD(variable, helper, func) DEFINE_GET_EX(unsigned long, variable, helper, func)
#define DEFINE_GET_WORD(variable, helper, func) DEFINE_GET_EX(unsigned short, variable, helper, func)
#define DEFINE_GET_DATE(variable, helper, func) DEFINE_GET_EX(tasksched_filter::filter_obj::task_sched_date, variable, helper, func)
#define DEFINE_GET_HRESULT(variable, helper, func) DEFINE_GET_EX(long, variable, helper, func)

DEFINE_GET_STRING(title, string_fetcher, get_Name);
DEFINE_GET_HRESULT(exit_code, hresult_fetcher, get_LastTaskResult);
DEFINE_GET_WORD(status, state_fetcher, get_State);
DEFINE_GET_DATE(most_recent_run_time, date_fetcher, get_LastRunTime);

tasksched_filter::filter_obj::expression_ast_type tasksched_filter::filter_obj::fun_convert_status(parsers::where::value_type target_type, parsers::where::filter_handler handler, expression_ast_type const& subject) {
	return expression_ast_type(parsers::where::int_value(convert_status(subject.get_string(handler))));
}


std::wstring tasksched_filter::filter_obj::render(std::wstring format, std::wstring datesyntax) {
 	strEx::replace(format, _T("%title%"), get_title());
// 	strEx::replace(format, _T("%account%"), get_account_name());
// 	strEx::replace(format, _T("%application%"), get_application_name());
// 	strEx::replace(format, _T("%comment%"), get_comment());
// 	strEx::replace(format, _T("%creator%"), get_creator());
// 	strEx::replace(format, _T("%parameters%"), get_parameters());
// 	strEx::replace(format, _T("%working_directory%"), get_working_directory());
// 
	strEx::replace(format, _T("%exit_code%"), strEx::itos(get_exit_code()));
// 	strEx::replace(format, _T("%error_retry_count%"), strEx::itos(get_error_retry_count()));
// 	strEx::replace(format, _T("%error_retry_interval%"), strEx::itos(get_error_retry_interval()));
// 	strEx::replace(format, _T("%flags%"), strEx::itos(get_flags()));
// 	//strEx::replace(format, _T("%idle_wait%"), strEx::itos(get_idle_wait()));
// 	strEx::replace(format, _T("%max_run_time%"), strEx::itos(get_max_run_time()));
// 	strEx::replace(format, _T("%priority%"), strEx::itos(get_priority()));
 	strEx::replace(format, _T("%status%"), convert_status(get_status()));
// 
// 	//	strEx::replace(format, _T("%next_run%"), strEx::format_date(get_next_run()));
 	if (get_most_recent_run_time()) {
 		task_sched_date date = get_most_recent_run_time();
		unsigned long long t = date;
		if (t == 0 || date.never_) {
			strEx::replace(format, _T("%most_recent_run_time%"), _T("never"));
			strEx::replace(format, _T("%most_recent_run_time-raw%"),  _T("never"));
		} else {
			strEx::replace(format, _T("%most_recent_run_time%"), strEx::format_date(t, datesyntax));
			strEx::replace(format, _T("%most_recent_run_time-raw%"), strEx::itos(t));
		}
	}

	strEx::replace(format, _T("\n"), _T(""));
	return format;
}

//////////////////////////////////////////////////////////////////////////

tasksched_filter::filter_engine tasksched_filter::factories::create_engine(tasksched_filter::filter_argument arg) {
	return filter_engine(new filter_engine_type(arg));
}
tasksched_filter::filter_argument tasksched_filter::factories::create_argument(std::wstring syntax, std::wstring datesyntax) {
	return filter_argument(new tasksched_filter::filter_argument_type(tasksched_filter::filter_argument_type::error_type(new where_filter::nsc_error_handler()), syntax, datesyntax));
}

tasksched_filter::filter_result tasksched_filter::factories::create_result(tasksched_filter::filter_argument arg) {
	return filter_result(new where_filter::simple_count_result<filter_obj>(arg));
}





