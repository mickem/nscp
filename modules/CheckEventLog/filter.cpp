#include "StdAfx.h"

#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

#include <nscapi/nscapi_plugin_interface.hpp>

namespace eventlog_filter {

	new_filter_obj::new_filter_obj(const std::string &logfile, eventlog::evt_handle &hEvent, eventlog::evt_handle &hContext) 
		: logfile(logfile)
		, hEvent(hEvent)
		, buffer(4096) 
	{
		DWORD dwBufferSize = 0;
		DWORD dwPropertyCount = 0;
		if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, buffer.size(), buffer.get(), &dwBufferSize, &dwPropertyCount)) {
			DWORD status = GetLastError();
			if (status == ERROR_INSUFFICIENT_BUFFER) {
				buffer.resize(dwBufferSize);
				if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, buffer.size(), buffer.get(), &dwBufferSize, &dwPropertyCount))
					throw nscp_exception("EvtRender failed: " + error::lookup::last_error());
			}
		}
	}

	long long new_filter_obj::get_written() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemTimeCreated].Type)
			return 0;
		return static_cast<long long>(strEx::filetime_to_time(buffer.get()[eventlog::api::EvtSystemTimeCreated].FileTimeVal));
	}
	long long new_filter_obj::get_el_type() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemLevel].Type)
			return 0;
		return buffer.get()[eventlog::api::EvtSystemLevel].ByteVal;
	}
	std::string new_filter_obj::get_message() {
		hlp::buffer<wchar_t, LPWSTR> message_buffer(4096);
		DWORD dwBufferSize = 0;
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemProviderName].Type)
			throw nscp_exception("Failed to get system provider");
		eventlog::evt_handle hMetadata = eventlog::EvtOpenPublisherMetadata(NULL, buffer.get()[eventlog::api::EvtSystemProviderName].StringVal, NULL, 0, 0);
		if (!hMetadata)
			throw nscp_exception("EvtOpenPublisherMetadata failed: " + error::lookup::last_error());

		if (!eventlog::EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageEvent, message_buffer.size(), message_buffer.get(), &dwBufferSize)) {
			DWORD status = GetLastError();
			if (status == ERROR_INSUFFICIENT_BUFFER) {
				message_buffer.resize(dwBufferSize);
				if (!eventlog::EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageEvent, message_buffer.size(), message_buffer.get(), &dwBufferSize))
					throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error());
			}
			else if (status != ERROR_EVT_MESSAGE_NOT_FOUND  && ERROR_EVT_MESSAGE_ID_NOT_FOUND != status)
				throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error(status));
		}
		return utf8::cvt<std::string>(message_buffer.get_t<wchar_t*>());
	}

	std::string new_filter_obj::get_source() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemChannel].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemChannel].StringVal);
	}
	std::string new_filter_obj::get_computer() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemComputer].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemComputer].StringVal);
	}

	using namespace parsers::where;

	int convert_severity(std::string str) {
		if (str == "success" || str == "ok")
			return 0;
		if (str == "informational" || str == "info")
			return 1;
		if (str == "warning" || str == "warn")
			return 2;
		if (str == "error" || str == "err")
			return 3;
		return strEx::s::stox<int>(str);
	}
	int convert_old_type(parsers::where::evaluation_context context, std::string str) {
		if (str == "error")
			return EVENTLOG_ERROR_TYPE;
		if (str == "warning")
			return EVENTLOG_WARNING_TYPE;
		if (str == "info")
			return EVENTLOG_INFORMATION_TYPE;
		if (str == "success")
			return EVENTLOG_SUCCESS;
		if (str == "auditSuccess")
			return EVENTLOG_AUDIT_SUCCESS;
		if (str == "auditFailure")
			return EVENTLOG_AUDIT_FAILURE;
		try {
			return strEx::s::stox<int>(str);
		} catch (const std::exception &e) {
			context->error("Failed to convert: " + str);
			return EVENTLOG_ERROR_TYPE;
		}
	}
	int convert_new_type(parsers::where::evaluation_context context, std::string str) {
		if (str == "audit")
			return 0;
		if (str == "critical")
			return 1;
		if (str == "error")
			return 2;
		if (str == "warning" || str == "warn")
			return 3;
		if (str == "information" || str == "info")
			return 4;
		try {
			return strEx::s::stox<int>(str);
		} catch (const std::exception &e) {
			context->error("Failed to convert: " + str);
			return 2;
		}
	}

	parsers::where::node_type fun_convert_severity(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_severity(subject->get_string_value(context)));
	}
	parsers::where::node_type fun_convert_new_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_new_type(context, subject->get_string_value(context)));
	}
	parsers::where::node_type fun_convert_old_type(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_old_type(context, subject->get_string_value(context)));
	}

	//////////////////////////////////////////////////////////////////////////

	filter_obj_handler::filter_obj_handler() {

		registry_.add_string()
			("source", boost::bind(&filter_obj::get_source, _1), "Source system.")
			("message", boost::bind(&filter_obj::get_message, _1), "The message renderd as a string.")
			("computer", boost::bind(&filter_obj::get_computer, _1), "Which computer generated the message")
			("log", boost::bind(&filter_obj::get_log, _1), "alias for file")
			("file", boost::bind(&filter_obj::get_log, _1), "The logfile name")
			;

		registry_.add_int()
			("id", boost::bind(&filter_obj::get_id, _1), "Eventlog id")
			("type", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "alias for level (old)")
			("level", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "Severity level (error, warning, info, success, auditSucess, auditFailure)")
			("written", type_date, boost::bind(&filter_obj::get_written, _1), "When the message was written to file")
			("category", boost::bind(&filter_obj::get_category, _1), "TODO")
			("customer", boost::bind(&filter_obj::get_customer, _1), "TODO")
			("rawid", boost::bind(&filter_obj::get_raw_id, _1), "Raw message id (contains many other fields all baked into a single number)")
			;
		if (eventlog::api::supports_modern()) {
			registry_.add_converter()
				(type_custom_type, &fun_convert_new_type)
				;
		} else {
			registry_.add_int()
				("severity", type_custom_severity, boost::bind(&filter_obj::get_severity, _1), "Probably not what you want.This is the technical severity of the mseesage often level is what you are looking for.")
				("generated", type_date, boost::bind(&filter_obj::get_generated, _1), "When the message was generated")
				("qualifier", boost::bind(&filter_obj::get_facility, _1), "TODO")
				("facility", boost::bind(&filter_obj::get_facility, _1), "TODO")
				;
			registry_.add_string()
				("strings", boost::bind(&filter_obj::get_strings, _1), "The message content. Significantly faster than message yet yields similar results.")
				;
			registry_.add_converter()
				(type_custom_severity, &fun_convert_severity)
				(type_custom_type, &fun_convert_old_type)
				;
		}
	}
}