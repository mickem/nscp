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
#include "filter.hpp"

#include <parsers/where.hpp>

#include <simple_timer.hpp>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <str/format.hpp>
#include <nsclient/nsclient_exception.hpp>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <map>
#include <list>



typedef boost::optional<std::string> op_str;
template<eventlog::api::EVT_PUBLISHER_METADATA_PROPERTY_ID T_object, DWORD T_id, DWORD T_desc>
struct data_cache {
	typedef std::map<std::string, eventlog::eventlog_table> task_table;
	boost::shared_mutex mutex_;

	task_table providers;

	boost::optional<std::string> get_cached(std::string &provider, long long id) {
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
		if (!readLock.owns_lock()) {
			return boost::optional<std::string>();
		}
		task_table::const_iterator pit = providers.find(provider);
		if (pit == providers.end())
			return boost::optional<std::string>();
		eventlog::eventlog_table::const_iterator cit = pit->second.find(id);
		if (cit == pit->second.end())
			return boost::optional<std::string>();
		return cit->second;
	}
	boost::optional<std::string> get(eventlog::evt_handle &hMetadata, std::string &provider, long long id) {
		eventlog::eventlog_table table = eventlog::fetch_table(hMetadata, T_object, T_id, T_desc);
		{
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
			if (writeLock.owns_lock()) {
				providers[provider] = table;
			}
		}
		eventlog::eventlog_table::const_iterator cit = table.find(id);
		if (cit == table.end())
			return boost::optional<std::string>();
		return cit->second;
	}
	boost::optional<std::string> apply_cached(std::string &provider, long long mask) {
		boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
		if (!readLock.owns_lock()) {
			return boost::optional<std::string>();
		}
		task_table::const_iterator pit = providers.find(provider);
		if (pit == providers.end())
			return boost::optional<std::string>();
		return do_apply(pit->second, mask);
	}
	boost::optional<std::string> apply(eventlog::evt_handle &hMetadata, std::string &provider, long long mask) {
		eventlog::eventlog_table table = eventlog::fetch_table(hMetadata, T_object, T_id, T_desc);
		{
			boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(1));
			if (writeLock.owns_lock()) {
				providers[provider] = table;
			}
		}
		return do_apply(table, mask);
	}
private:
	std::string do_apply(const eventlog::eventlog_table &table, long long mask) {
		std::string keys = "";
		BOOST_FOREACH(const eventlog::eventlog_table::value_type &cit, table) {
			if ((mask&cit.first) == cit.first)
				str::format::append_list(keys, cit.second, ",");
		}
		return keys;
	}

};

data_cache<eventlog::api::EvtPublisherMetadataTasks, eventlog::api::EvtPublisherMetadataTaskValue, eventlog::api::EvtPublisherMetadataTaskName> task_cache_;
data_cache<eventlog::api::EvtPublisherMetadataKeywords, eventlog::api::EvtPublisherMetadataKeywordValue, eventlog::api::EvtPublisherMetadataKeywordName> keyword_cache_;

namespace eventlog_filter {
	new_filter_obj::new_filter_obj(unsigned long long now, const std::string &logfile, eventlog::api::EVT_HANDLE hEvent, eventlog::evt_handle &hContext, const int truncate_message)
		: filter_obj(now)
		, logfile(logfile)
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
					throw nsclient::nsclient_exception("EvtRender failed: " + error::lookup::last_error());
			} else 
				throw nsclient::nsclient_exception("EvtRender failed: " + error::lookup::last_error(status));
		}
	}

	long long new_filter_obj::get_written() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemTimeCreated].Type)
			return 0;
		return static_cast<long long>(str::format::filetime_to_time(buffer.get()[eventlog::api::EvtSystemTimeCreated].FileTimeVal));
	}
	std::string new_type_to_string(long long ival) {
		if (ival == 1)
			return "critical";
		if (ival == 2)
			return "error";
		if (ival == 3)
			return "warning";
		if (ival == 4)
			return "information";
		if (ival == 5)
			return "verbose";
		return "unknown";
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

	std::string new_filter_obj::get_el_type_s() const {
		return new_type_to_string(get_el_type());
	}
	std::string old_filter_obj::get_el_type_s() const {
		return old_type_to_string(get_el_type());
	}

	long long new_filter_obj::get_el_type() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemLevel].Type) {
			NSC_DEBUG_MSG(" --> missing level: " + str::xtos(get_id()));
			return 0;
		}
		return buffer.get()[eventlog::api::EvtSystemLevel].ByteVal;
	}
	std::string new_filter_obj::get_task() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemTask].Type)
			return "";
		int id = buffer.get()[eventlog::api::EvtSystemTask].Int16Val;
		std::string provider = get_provider();
		op_str os = task_cache_.get_cached(provider, id);
		if (os)
			return *os;
		os = task_cache_.get(get_provider_handle(provider), provider, id);
		if (os)
			return *os;
		return "";
	}
	#define WINLOG_KEYWORD_AUDITFAILURE 0x0010000000000000
	#define WINLOG_KEYWORD_AUDITSUCCESS 0x0020000000000000
	#define WINLOG_KEYWORD_RESERVED     0x00FFFFFFFFFFFFFF

	std::string new_filter_obj::get_keyword() {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemKeywords].Type)
			return "";
		long long id = buffer.get()[eventlog::api::EvtSystemKeywords].Int64Val;
		id = id & WINLOG_KEYWORD_RESERVED;
		if (id == 0) {
			return "";
		}
		std::string provider = get_provider();
		std::string ret = "";
		if ((id & WINLOG_KEYWORD_AUDITFAILURE) == WINLOG_KEYWORD_AUDITFAILURE) {
			id = id & ~WINLOG_KEYWORD_AUDITFAILURE;
			str::format::append_list(ret, "Audit Failure");
		}
		if ((id & WINLOG_KEYWORD_AUDITSUCCESS) == WINLOG_KEYWORD_AUDITSUCCESS) {
			id = id & ~WINLOG_KEYWORD_AUDITSUCCESS;
			str::format::append_list(ret, "Audit Success");
		}
		if (id != 0) {
			op_str os = keyword_cache_.apply_cached(provider, id);
			if (!os) {
				os = keyword_cache_.apply(get_provider_handle(provider), provider, id);
			}
			if (os) {
				str::format::append_list(ret, *os);
			}
		}
		return ret;
	}

	eventlog::evt_handle& new_filter_obj::get_provider_handle(const std::string provider) {
		if (providers_.find(provider) == providers_.end()) {
			eventlog::api::EVT_HANDLE tmp = eventlog::EvtOpenPublisherMetadata(NULL, utf8::cvt<std::wstring>(provider).c_str(), NULL, 0, 0);
			if (!tmp)
				throw nsclient::nsclient_exception("EvtOpenPublisherMetadata failed for '" + provider + "': " + error::lookup::last_error());
			providers_[provider] = tmp;
		}
		return providers_[provider];
	}

	std::string new_filter_obj::get_message() {
		try {
			std::string msg;
			int status = eventlog::EvtFormatMessage(get_provider_handle(get_provider()), hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageEvent, msg);
			if (status != ERROR_SUCCESS) {
				NSC_DEBUG_MSG("Failed to format eventlog record: ID=" + str::xtos(get_id()) + ": " + error::format::from_system(status));
				if (status == ERROR_INVALID_PARAMETER)
					return "";
				else if (status == ERROR_EVT_MESSAGE_NOT_FOUND)
					return "";
				else if (status == ERROR_EVT_MESSAGE_ID_NOT_FOUND)
					return "";
				else if (status == ERROR_EVT_UNRESOLVED_VALUE_INSERT)
					throw nsclient::nsclient_exception("Invalidly formatted eventlog message for: " + error::lookup::last_error(status));
				throw nsclient::nsclient_exception("EvtFormatMessage failed: " + error::lookup::last_error(status));
			}
			boost::replace_all(msg, "\n", " ");
			boost::replace_all(msg, "\r", " ");
			boost::replace_all(msg, "\t", " ");
			boost::replace_all(msg, "  ", " ");
			if (truncate_message > 0 && msg.length() > truncate_message)
				msg = msg.substr(0, truncate_message);
			return msg;
		} catch (const nsclient::nsclient_exception &e) {
			return e.reason();
		}
	}

	std::string new_filter_obj::get_xml() {
		try {
			std::string xml;
			int status = eventlog::EvtFormatMessage(get_provider_handle(get_provider()), hEvent, 0, 0, NULL, eventlog::api::EvtFormatMessageXml, xml);
			if (status != ERROR_SUCCESS) {
				NSC_DEBUG_MSG("Failed to format eventlog record: ID=" + str::xtos(get_id()) + ": " + error::format::from_system(status));
				if (status == ERROR_INVALID_PARAMETER)
					return "";
				else if (status == ERROR_EVT_MESSAGE_NOT_FOUND)
					return "";
				else if (status == ERROR_EVT_MESSAGE_ID_NOT_FOUND)
					return "";
				else if (status == ERROR_EVT_UNRESOLVED_VALUE_INSERT)
					throw nsclient::nsclient_exception("Invalidly formatted eventlog message for: " + error::lookup::last_error(status));
				throw nsclient::nsclient_exception("EvtFormatMessage failed: " + error::lookup::last_error(status));
			}
			boost::replace_all(xml, "\n", "&#10;");
			boost::replace_all(xml, "\r", "&#13;");
			boost::replace_all(xml, "\t", "  ");
			return xml;
		} catch (const nsclient::nsclient_exception &e) {
			return e.reason();
		}
	}

	std::string new_filter_obj::get_provider() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemProviderName].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemProviderName].StringVal);
	}
	std::string new_filter_obj::get_log() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemChannel].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemChannel].StringVal);
	}
	std::string new_filter_obj::get_computer() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemComputer].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemComputer].StringVal);
	}
	std::string new_filter_obj::get_guid() const {
		if (eventlog::api::EvtVarTypeNull == buffer.get()[eventlog::api::EvtSystemProviderGuid].Type)
			return "";
		return utf8::cvt<std::string>(buffer.get()[eventlog::api::EvtSystemProviderGuid].StringVal);
	}
	long long new_filter_obj::get_category() const {
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
		return str::stox<int>(str);
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
			return str::stox<int>(str);
		} catch (const std::exception&) {
			context->error("Failed to convert: " + str);
			return EVENTLOG_ERROR_TYPE;
		}
	}
	int convert_new_type(parsers::where::evaluation_context context, std::string str) {
		if (str == "critical")
			return 1;
		if (str == "error")
			return 2;
		if (str == "warning" || str == "warn")
			return 3;
		if (str == "informational" || str == "info" || str == "information" || str == "success" || str == "auditSuccess")
			return 4;
		if (str == "debug" || str == "verbose")
			return 5;
		try {
			return str::stox<int>(str);
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
			("source", boost::bind(&filter_obj::get_provider, _1), "Source system.")
			("message", boost::bind(&filter_obj::get_message, _1), "The message rendered as a string.")
			("computer", boost::bind(&filter_obj::get_computer, _1), "Which computer generated the message")
			("log", boost::bind(&filter_obj::get_log, _1), "alias for file")
			("file", boost::bind(&filter_obj::get_log, _1), "The logfile name")
			("guid", boost::bind(&filter_obj::get_guid, _1), "The logfile name")
			("provider", boost::bind(&filter_obj::get_provider, _1), "Source system.")
			("task", boost::bind(&filter_obj::get_task, _1), "The type of event (task)")
			("keyword", boost::bind(&filter_obj::get_keyword, _1), "The keyword associated with this event")
			("written_str", boost::bind(&filter_obj::get_written_hs, _1), "When the message was written to file as an absolute date string")
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
			registry_.add_string()
				("xml", boost::bind(&filter_obj::get_xml, _1), "Get event as XML message.")
				;
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

	std::string filter_obj::get_written_hs() const {
		long long time = get_written();
		static const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
		boost::posix_time::ptime pt = epoch + boost::posix_time::seconds(time);
		return boost::posix_time::to_iso_extended_string(pt);
	}

}