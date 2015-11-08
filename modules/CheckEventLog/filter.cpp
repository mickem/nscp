#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace eventlog_filter {
	new_filter_obj::new_filter_obj(const std::string &logfile, eventlog::evt_handle &hEvent, eventlog::evt_handle &hContext, const int truncate_message)
		: logfile(logfile)
		, hEvent(hEvent)
		, buffer(4096)
		, truncate_message(truncate_message) {
		DWORD dwBufferSize = 0;
		DWORD dwPropertyCount = 0;
		if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.get(), &dwBufferSize, &dwPropertyCount)) {
			DWORD status = GetLastError();
			if (status == ERROR_INSUFFICIENT_BUFFER) {
				buffer.resize(dwBufferSize);
				if (!EvtRender(hContext, hEvent, eventlog::api::EvtRenderEventValues, static_cast<DWORD>(buffer.size()), buffer.get(), &dwBufferSize, &dwPropertyCount))
					throw nscp_exception("EvtRender failed: " + error::lookup::last_error());
			}
		}
	}

	long long new_filter_obj::get_written() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemTimeCreated].Type)
			return 0;
		return static_cast<long long>(strEx::filetime_to_time(buffer.get()[eventlog::api::EvtSystemTimeCreated].FileTimeVal));
	}
	std::string new_type_to_string(long long ival) {
		//if (ival == 0)
		//	return "audit";
		//if (ival == 1)
		//	return "critical";
		if (ival == 2)
			return "error";
		if (ival == 3)
			return "warning";
		//if (ival == 4)
		return "information";
		//return "unknown";
	}
	std::string old_type_to_string(long long ival) {
		if (ival == 0)
			return "audit";
		if (ival == 1)
			return "error";
		if (ival == 2)
			return "error";
		if (ival == 3)
			return "warning";
		if (ival == 4)
			return "information";
		return "unknown";
	}

	std::string new_filter_obj::get_el_type_s() {
		return new_type_to_string(get_el_type());
	}
	std::string old_filter_obj::get_el_type_s() {
		return old_type_to_string(get_el_type());
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
			throw nscp_exception("EvtOpenPublisherMetadata failed for '" + utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemProviderName].StringVal) + "': " + error::lookup::last_error());

		if (!eventlog::EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageEvent, static_cast<DWORD>(message_buffer.size()), message_buffer.get(), &dwBufferSize)) {
			DWORD status = GetLastError();
			if (status == ERROR_INSUFFICIENT_BUFFER) {
				message_buffer.resize(dwBufferSize);
				if (!eventlog::EvtFormatMessage(hMetadata, hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageEvent, static_cast<DWORD>(message_buffer.size()), message_buffer.get(), &dwBufferSize))
					throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error());
			} else if (status != ERROR_EVT_MESSAGE_NOT_FOUND  && ERROR_EVT_MESSAGE_ID_NOT_FOUND != status)
				throw nscp_exception("EvtFormatMessage failed: " + error::lookup::last_error(status));
		}
		std::string msg = utf8::cvt<std::string>(message_buffer.get_t<wchar_t*>());
		boost::replace_all(msg, "\n", " ");
		boost::replace_all(msg, "\r", " ");
		boost::replace_all(msg, "\t", " ");
		boost::replace_all(msg, "  ", " ");
		if (truncate_message > 0 && msg.length() > truncate_message)
			msg = msg.substr(0, truncate_message);
		return msg;
	}

	std::string new_filter_obj::get_source() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemProviderName].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemProviderName].StringVal);
	}
	std::string new_filter_obj::get_log() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemChannel].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemChannel].StringVal);
	}
	std::string new_filter_obj::get_computer() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemComputer].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemComputer].StringVal);
	}
	long long new_filter_obj::get_category() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemTask].Type)
			return 0;
		return buffer.get()[eventlog::api::EvtSystemTask].UInt16Val;
	}

	using namespace parsers::where;

	int convert_old_severity(parsers::where::evaluation_context context, std::string str) {
		if (str == "success" || str == "ok")
			return 0;
		if (str == "informational" || str == "info" || str == "information")
			return 1;
		if (str == "warning" || str == "warn")
			return 2;
		if (str == "error" || str == "err")
			return 3;
		context->error("Invalid severity: " + str);
		return strEx::s::stox<int>(str);
	}
	int convert_old_type(parsers::where::evaluation_context context, std::string str) {
		if (str == "error")
			return EVENTLOG_ERROR_TYPE;
		if (str == "warning")
			return EVENTLOG_WARNING_TYPE;
		if (str == "informational" || str == "info" || str == "information")
			return EVENTLOG_INFORMATION_TYPE;
		if (str == "success")
			return EVENTLOG_SUCCESS;
		if (str == "auditSuccess")
			return EVENTLOG_AUDIT_SUCCESS;
		if (str == "auditFailure")
			return EVENTLOG_AUDIT_FAILURE;
		try {
			context->error("Invalid severity: " + str);
			return strEx::s::stox<int>(str);
		} catch (const std::exception&) {
			context->error("Failed to convert: " + str);
			return EVENTLOG_ERROR_TYPE;
		}
	}
	int convert_new_type(parsers::where::evaluation_context context, std::string str) {
		// TODO: auditFailure
		if (str == "audit")
			return 0;
		if (str == "critical")
			return 1;
		if (str == "error")
			return 2;
		if (str == "warning" || str == "warn")
			return 3;
		if (str == "informational" || str == "info" || str == "information" || str == "success" || str == "auditSuccess")
			return 4;
		try {
			return strEx::s::stox<int>(str);
		} catch (const std::exception&) {
			context->error("Failed to convert: " + str);
			return 2;
		}
	}

	parsers::where::node_type fun_convert_old_severity(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
		return parsers::where::factory::create_int(convert_old_severity(context, subject->get_string_value(context)));
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
			("message", boost::bind(&filter_obj::get_message, _1), "The message rendered as a string.")
			("computer", boost::bind(&filter_obj::get_computer, _1), "Which computer generated the message")
			("log", boost::bind(&filter_obj::get_log, _1), "alias for file")
			("file", boost::bind(&filter_obj::get_log, _1), "The logfile name")
			;

		registry_.add_int()
			("id", boost::bind(&filter_obj::get_id, _1), "Eventlog id")
			("type", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "alias for level (old, deprecated)")
			("written", type_date, boost::bind(&filter_obj::get_written, _1), boost::bind(&filter_obj::get_written_s, _1), "When the message was written to file")
			("category", boost::bind(&filter_obj::get_category, _1), "TODO")
			("customer", boost::bind(&filter_obj::get_customer, _1), "TODO")
			("rawid", boost::bind(&filter_obj::get_raw_id, _1), "Raw message id (contains many other fields all baked into a single number)")
			;

		registry_.add_human_string()
			("type", boost::bind(&filter_obj::get_el_type_s, _1), "")
			("level", boost::bind(&filter_obj::get_el_type_s, _1), "")
			;
		if (eventlog::api::supports_modern()) {
			registry_.add_converter()
				(type_custom_type, &fun_convert_new_type)
				;
			registry_.add_int()
				("level", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "Severity level (error, warning, info, success, auditSucess, auditFailure)")
				;
		} else {
			registry_.add_int()
				("level", type_custom_type, boost::bind(&filter_obj::get_el_type, _1), "Severity level (error, warning, info)")
				("severity", type_custom_severity, boost::bind(&filter_obj::get_severity, _1), "Legacy: Probably not what you want.This is the technical severity of the message often level is what you are looking for.")
				("generated", type_date, boost::bind(&filter_obj::get_generated, _1), "When the message was generated")
				("qualifier", boost::bind(&filter_obj::get_facility, _1), "TODO")
				("facility", boost::bind(&filter_obj::get_facility, _1), "TODO")
				;
			registry_.add_string()
				("strings", boost::bind(&filter_obj::get_strings, _1), "The message content. Significantly faster than message yet yields similar results.")
				;
			registry_.add_converter()
				(type_custom_severity, &fun_convert_old_severity)
				(type_custom_type, &fun_convert_old_type)
				;
		}
	}
}