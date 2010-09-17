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
#include "CheckEventLog.h"
#include <filter_framework.hpp>
#include <boost/foreach.hpp>

#include <strEx.h>
#include <time.h>
#include <utils.h>
#include <error.hpp>
#include <map>
#include <vector>

#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>
#include <simple_timer.hpp>

#include "simple_registry.hpp"
#include "eventlog_record.hpp"
#include "eventlog_filter.hpp"

CheckEventLog gCheckEventLog;

CheckEventLog::CheckEventLog() {
}
CheckEventLog::~CheckEventLog() {
}
struct parse_exception {
	parse_exception(std::wstring) {}
};

#include <parsers/where.cpp>
#include <parsers/grammar.cpp>
#include <parsers/ast.cpp>

namespace filter {
	namespace where {
		struct type_obj : public parsers::where::varible_handler<type_obj> {
			typedef parsers::where::varible_handler<type_obj> handler;
			typedef std::list<std::wstring> error_type;
			typedef std::map<std::wstring,parsers::where::value_type> types_type;
			types_type types;
			error_type errors;
			static const parsers::where::value_type type_custom_severity = parsers::where::type_custom_int_1;
			EventLogRecord *record;
			type_obj() : record(NULL) {
				using namespace boost::assign;
				using namespace parsers::where;
				insert(types)
					(_T("id"), (type_int))
					(_T("source"), (type_string))
					(_T("type"), (type_int))
					(_T("severity"), (type_custom_severity))
					(_T("message"), (type_string))
					(_T("strings"), (type_string))
					(_T("written"), (type_date))
					(_T("generated"), (type_date));
			}
			type_obj(EventLogRecord *record) : record(record) {}
			bool has_variable(std::wstring key) {
				return types.find(key) != types.end();
			}
			parsers::where::value_type get_type(std::wstring key) {
				types_type::const_iterator cit = types.find(key);
				if (cit == types.end())
					return parsers::where::type_invalid;
				return cit->second;
			}
			bool can_convert(parsers::where::value_type from, parsers::where::value_type to) {
				if ((from == parsers::where::type_string)&&(to == type_custom_severity))
					return true;
				return false;
			}
			void error(std::wstring err) {
				errors.push_back(err);
			}
			bool has_error() {
				return !errors.empty();
			}
			long long get_id() {
				if (record == NULL) throw _T("Whoops"); return record->eventID(); 
			}
			std::wstring get_source() {
				if (record == NULL) throw _T("Whoops"); return record->eventSource(); 
			}
			long long get_el_type() {
				if (record == NULL) throw _T("Whoops"); return record->eventType(); 
			}
			long long get_severity() {
				if (record == NULL) throw _T("Whoops"); 
				//NSC_DEBUG_MSG_STD(_T("Severity: ") + strEx::itos(record->severity()));
				return record->severity();
			}
			std::wstring get_message() {
				if (record == NULL) throw _T("Whoops"); return record->render_message(); 
			}
			std::wstring get_strings() {
				if (record == NULL) throw _T("Whoops"); return record->enumStrings(); 
			}
			long long get_written() {
				if (record == NULL) throw _T("Whoops"); return record->written(); 
			}
			long long get_generated() {
				if (record == NULL) throw _T("Whoops"); return record->generated(); 
			}

			handler::bound_string_type bind_string(std::wstring key) {
				handler::bound_string_type ret;
				if (key == _T("source"))
					ret = &type_obj::get_source;
				else if (key == _T("message"))
					ret = &type_obj::get_message;
				else if (key == _T("strings"))
					ret = &type_obj::get_strings;
				else
					NSC_DEBUG_MSG_STD(_T("Failed to bind (string): ") + key);
				return ret;
			}
			handler::bound_int_type bind_int(std::wstring key) {
				handler::bound_int_type ret;
				if (key == _T("id"))
					ret = &type_obj::get_id;
				else if (key == _T("type"))
					ret = &type_obj::get_el_type;
				else if (key == _T("severity"))
					ret = &type_obj::get_severity;
				else if (key == _T("generated"))
					ret = &type_obj::get_generated;
				else if (key == _T("written"))
					ret = &type_obj::get_written;
				else
					NSC_DEBUG_MSG_STD(_T("Failed to bind (int): ") + key);
				return ret;
			}

			bool has_function(parsers::where::value_type to, std::wstring name, parsers::where::expression_ast<type_obj> subject) {
				if (to == type_custom_severity)
					return true;
				return false;
			}
			handler::bound_function_type bind_function(parsers::where::value_type to, std::wstring name, parsers::where::expression_ast<type_obj> subject) {
				handler::bound_function_type ret;
				if (to == type_custom_severity)
					ret = &type_obj::fun_convert_severity;
				return ret;
			}

			parsers::where::expression_ast<type_obj> fun_convert_severity(parsers::where::value_type target_type, parsers::where::expression_ast<type_obj> const& subject) {
				return parsers::where::expression_ast<type_obj>(parsers::where::int_value(convert_severity(subject.get_string(*this))));
			}
			int convert_severity(std::wstring str) {
				if (str == _T("success") || str == _T("ok"))
					return 0;
				if (str == _T("informational") || str == _T("info"))
					return 1;
				if (str == _T("warning") || str == _T("warn"))
					return 2;
				if (str == _T("error") || str == _T("err"))
					return 3;
				error(_T("Invalid severity: ") + str);
				return strEx::stoi(str);
			}


			std::wstring get_error() {
				std::wstring ret;
				BOOST_FOREACH(std::wstring s, errors) {
					if (!ret.empty()) ret += _T(", ");
					ret += s;
				}
				return ret;
			}
		};
	}
}




struct filter_container {
	enum filter_types {
		filter_plus = 1,
		filter_minus = 2,
		filter_normal = 3
	};
	typedef std::pair<int,eventlog_filter> filteritem_type;
	typedef std::list<filteritem_type > filterlist_type;

	filterlist_type filters;

	bool bFilterAll;
	bool bFilterIn;

	bool bDebug;
	int debugThreshold;

	bool bShowDescriptions;
	std::wstring syntax;

	std::wstring filter;

	filter_container(std::wstring syntax, bool debug) : bDebug(debug), debugThreshold(0), bFilterIn(true), bFilterAll(false), bShowDescriptions(false), syntax(syntax) {}

};

struct any_mode_filter {
	virtual bool boot() = 0;
	virtual bool validate(std::wstring &message) = 0;
	virtual bool match(EventLogRecord &record) = 0;
	virtual std::wstring get_name() = 0;
	virtual std::wstring get_subject() = 0;
};

struct first_mode_filter : public any_mode_filter {
	typedef filter_container::filterlist_type::const_iterator filter_iterator;
	filter_container &data;
	first_mode_filter(filter_container &data) : data(data) {}
	bool boot() {return true;}
	bool validate(std::wstring &message) {
		if (data.filters.empty()) {
			message = _T("No filters specified try adding: filter+generated=>2d");
			return false;
		}
		return true;
	}

	virtual bool match(EventLogRecord &record) {
		bool bMatch = !data.bFilterIn;
		for (filter_iterator cit3 = data.filters.begin(); cit3 != data.filters.end(); ++cit3) {
			std::wstring reason;
			int mode = (*cit3).first;
			bool bTmpMatched = (*cit3).second.matchFilter(record);
			if (data.bFilterAll) {
				if (!bTmpMatched) {
					bMatch = false;
					break;
				}
			} else {
				if (bTmpMatched) {
					bMatch = true;
					break;
				}
			}
		}
		if ((data.bFilterIn&&bMatch)||(!data.bFilterIn&&!bMatch)) {
			return true;
		}
		return false;

	}
	std::wstring get_name() {
		return _T("deprecated");
	}
	std::wstring get_subject() { return _T("TODO"); }

};
struct second_mode_filter : public any_mode_filter  {
	typedef filter_container::filterlist_type::const_iterator filter_iterator;

	filter_container &data;
	second_mode_filter(filter_container &data) : data(data) {}
	bool boot() {return true;}
	bool validate(std::wstring &message) {
		if (data.filters.empty()) {
			message = _T("No filters specified try adding: filter+generated=>2d");
			return false;
		}
		return true;
	}

	virtual bool match(EventLogRecord &record) {
		bool bMatch = !data.bFilterIn;
		int i=0;
		for (filter_iterator cit3 = data.filters.begin(); cit3 != data.filters.end(); ++cit3, i++ ) {
			std::wstring reason;
			int mode = (*cit3).first;
			bool bTmpMatched = (*cit3).second.matchFilter(record);
			if ((mode == filter_container::filter_minus)&&(bTmpMatched)) {
				// a -<filter> hit so thrash item and bail out!
				if (data.bDebug && (i>data.debugThreshold))
					NSC_DEBUG_MSG_STD(_T("[") + strEx::itos(i) + _T("] Matched: - ") + (*cit3).second.to_string() + _T(" for: ") + record.render(data.bShowDescriptions, data.syntax));
				return false;
			} else if ((mode == filter_container::filter_plus)&&(!bTmpMatched)) {
				// a +<filter> hit so keep item and bail out!
				if (data.bDebug && (i>data.debugThreshold))
					NSC_DEBUG_MSG_STD(_T("[") + strEx::itos(i) + _T("] Matched: + ") + (*cit3).second.to_string() + _T(" for: ") + record.render(data.bShowDescriptions, data.syntax));
				return false;
			} else if (bTmpMatched) {
				if (data.bDebug && (i>data.debugThreshold))
					NSC_DEBUG_MSG_STD(_T("[") + strEx::itos(i) + _T("] Matched: . ") + (*cit3).second.to_string() + _T(" for: ") + record.render(data.bShowDescriptions, data.syntax));
				bMatch = true;
			}
		}
		return bMatch;
	}
	std::wstring get_name() {
		return _T("old");
	}
	std::wstring get_subject() { return _T("TODO"); }
};

struct where_mode_filter : public any_mode_filter {
	filter_container &data;
	std::string message;
	parsers::where::parser<filter::where::type_obj> ast_parser;
	filter::where::type_obj dummy;

	where_mode_filter(filter_container &data) : data(data) {}
	bool boot() {return true; }

	bool validate(std::wstring &message) {
		if (data.bDebug)
			NSC_DEBUG_MSG_STD(_T("Parsing: ") + data.filter);

		if (!ast_parser.parse(data.filter)) {
			NSC_LOG_ERROR_STD(_T("Parsing failed of '") + data.filter + _T("' at: ") + ast_parser.rest);
			message = _T("Parsing failed: ") + ast_parser.rest;
			return false;
		}
		if (data.bDebug)
			NSC_DEBUG_MSG_STD(_T("Parsing succeeded: ") + ast_parser.result_as_tree());

		if (!ast_parser.derive_types(dummy) || dummy.has_error()) {
			message = _T("Invalid types: ") + dummy.get_error();
			return false;
		}
		if (data.bDebug)
			NSC_DEBUG_MSG_STD(_T("Type resolution succeeded: ") + ast_parser.result_as_tree());

		if (!ast_parser.bind(dummy) || dummy.has_error()) {
			message = _T("Variable and function binding failed: ") + dummy.get_error();
			return false;
		}
		if (data.bDebug)
			NSC_DEBUG_MSG_STD(_T("Binding succeeded: ") + ast_parser.result_as_tree());

		if (!ast_parser.static_eval(dummy) || dummy.has_error()) {
			message = _T("Static evaluation failed: ") + dummy.get_error();
			return false;
		}
		if (data.bDebug)
			NSC_DEBUG_MSG_STD(_T("Static evaluation succeeded: ") + ast_parser.result_as_tree());

		return true;
	}
	virtual bool match(EventLogRecord &record) {
		filter::where::type_obj obj(&record);
		//NSC_DEBUG_MSG_STD(_T("Evaluating: ") + ast_parser.result_as_tree() + _T(": ") + strEx::itos(record.severity()) + _T(" >> ") + strEx::itos(ast_parser.evaluate(obj)));
		bool ret = ast_parser.evaluate(obj);
		if (obj.has_error()) {
			NSC_LOG_ERROR_STD(_T("Error: ") + obj.get_error());
		}
		return ret;
	}
	std::wstring get_name() {
		return _T("where");
	}
	std::wstring get_subject() { return data.filter; }
};



void CheckEventLog::parse(std::wstring expr) {
//return false;
/*
	my_type_obj obj1(123);
	std::wcout << _T("Result (001): ") << ast_parser.evaluate(obj1) << std::endl;
	my_type_obj obj2(321);
	std::wcout << _T("Result (002): ") << ast_parser.evaluate(obj2) << std::endl;
	*/
}

bool CheckEventLog::loadModule() {
	try {
		NSCModuleHelper::registerCommand(_T("CheckEventLog"), _T("Check for errors in the event logger!"));
		debug_ = NSCModuleHelper::getSettingsInt(EVENTLOG_SECTION_TITLE, EVENTLOG_DEBUG, EVENTLOG_DEBUG_DEFAULT)==1;
		lookup_names_ = NSCModuleHelper::getSettingsInt(EVENTLOG_SECTION_TITLE, EVENTLOG_LOOKUP_NAMES, EVENTLOG_LOOKUP_NAMES_DEFAULT)==1;
		syntax_ = NSCModuleHelper::getSettingsString(EVENTLOG_SECTION_TITLE, EVENTLOG_SYNTAX, EVENTLOG_SYNTAX_DEFAULT);
		buffer_length_ = NSCModuleHelper::getSettingsInt(EVENTLOG_SECTION_TITLE, EVENTLOG_BUFFER, EVENTLOG_BUFFER_DEFAULT);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	/*
	parse(_T("321 = 123"));
	parse(_T("123 = 123"));
	parse(_T("id = 123"));
	parse(_T("id = 321"));

	parse(_T("id = '123'"));
	parse(_T("id = '321'"));

	parse(_T("id = convert(123)"));
	parse(_T("id = convert(321)"));

	parse(_T("id = 123 AND 123 = 123 AND id = 123x"));
	parse(_T("id = 123 AND 123 = 321 OR 123 = 456 OR 123 = 123"));
	
	parse(_T("foo"));
	parse(_T("1"));
	parse(_T("foo = "));
	parse(_T("foo = 1"));
	parse(_T("'foo' = 1"));
	parse(_T("foo = '1'"));
	parse(_T("'hello'='world'"));

	parse(_T("foo = bar"));
	parse(_T("foo = bar AND bar = foo"));
	parse(_T("foo = bar AND bar = 1"));
	parse(_T("foo = bar AND bar = foo OR foo = bar"));
	parse(_T("foo = bar AND bar = 1 OR foo = 1"));
	parse(_T(" foo = bar AND ( test > 120 OR foo < 123) OR ugh IN (123, 456, 789)"));

	parse(_T("aaa = 111 OR bbb = 222 OR ccc = 333"));
	parse(_T("(aaa = 111) OR bbb = 222 OR ccc = 333"));
	parse(_T("(aaa = 111 OR bbb = 222) OR ccc = 333"));
	parse(_T("(aaa = 111 OR bbb = 222 OR ccc = 333)"));
	parse(_T("aaa = 111 OR (bbb = 222 OR ccc = 333)"));
	parse(_T("aaa = 111 OR bbb = 222 OR (ccc = 333)"));
	parse(_T("ccc = -333"));
	parse(_T("ccc = -333 AND ccc = to_date('AABBCC', 1234)"));
	parse(_T("aaa = 111 OR bbb = 222 OR (ccc = -333)"));
	parse(_T("ccc = -333 AND ccc = to_date('AABBCC', 1234) OR aaa = 123x"));
	parse(_T("ccc = -333 AND ccc = to_date('AABBCC', 1234) OR aaa = 123x OR 123r = foo123"));
*/
	return true;
}
bool CheckEventLog::unloadModule() {
	return true;
}

bool CheckEventLog::hasCommandHandler() {
	return true;
}
bool CheckEventLog::hasMessageHandler() {
	return false;
}


std::wstring find_eventlog_name(std::wstring name) {
	try {
		simple_registry::registry_key key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog"));
		std::list<std::wstring> list = key.get_keys();
		for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
			try {
				simple_registry::registry_key sub_key(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\") + *cit);
				std::wstring file = sub_key.get_string(_T("DisplayNameFile"));
				int id = sub_key.get_int(_T("DisplayNameID"));
				std::wstring real_name = error::format::message::from_module(file, id);
				strEx::replace(real_name, _T("\n"), _T(""));
				strEx::replace(real_name, _T("\r"), _T(""));
				NSC_DEBUG_MSG(_T("Attempting to match: ") + real_name + _T(" with ") + name);
				if (real_name == name)
					return *cit;
			} catch (simple_registry::registry_exception &e) { e;}
		}
		return name;
	} catch (simple_registry::registry_exception &e) {
		NSC_DEBUG_MSG(_T("Failed to get eventlog name (assuming shorthand): ") + e.what());
		return name;
	} catch (...) {
		NSC_DEBUG_MSG(_T("Failed to get eventlog name (assuming shorthand)"));
		return name;
	}
}



class uniq_eventlog_record {
	DWORD ID;
	WORD type;
	WORD category;
public:
	std::wstring message;
	uniq_eventlog_record(EVENTLOGRECORD *pevlr) : ID(pevlr->EventID&0xffff), type(pevlr->EventType), category(pevlr->EventCategory) {}
	bool operator< (const uniq_eventlog_record &other) const { 
		return (ID < other.ID) || ((ID==other.ID)&&(type < other.type)) || (ID==other.ID&&type==other.type)&&(category < other.category);
	}
	std::wstring to_string() const {
		return _T("id=") + strEx::itos(ID) + _T("type=") + strEx::itos(type) + _T("category=") + strEx::itos(category);
	}
};
typedef std::map<uniq_eventlog_record,unsigned int> uniq_eventlog_map;





#define MAP_FILTER(value, obj, filtermode) \
			else if (p__.first == value) { filter.obj = p__.second; if (bPush) { data.filters.push_back(filter_container::filteritem_type(filtermode, filter)); filter = eventlog_filter(); } }
#define MAP_FILTER_LAST(value, obj) \
			else if (p__.first == value) { data.filters.front().second.obj = p__.second; }

struct event_log_buffer {
	BYTE *bBuffer;
	DWORD bufferSize_;
	event_log_buffer(DWORD bufferSize) : bufferSize_(bufferSize) {
		bBuffer = new BYTE[bufferSize+10];
	}
	~event_log_buffer() {
		delete [] bBuffer;
	}
	EVENTLOGRECORD* getBufferUnsafe() {
		return reinterpret_cast<EVENTLOGRECORD*>(bBuffer);
	}
	DWORD getBufferSize() {
		return bufferSize_;
	}
};

NSCAPI::nagiosReturn CheckEventLog::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	if (command != _T("CheckEventLog"))
		return NSCAPI::returnIgnored;
	simple_timer time;
	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsULongInteger> EventLogQuery1Container;
	typedef checkHolders::CheckContainer<checkHolders::ExactBoundsULongInteger> EventLogQuery2Container;
	
	NSCAPI::nagiosReturn returnCode = NSCAPI::returnOK;
	std::list<std::wstring> stl_args = arrayBuffer::arrayBuffer2list(argLen, char_args);

	std::list<std::wstring> files;
	EventLogQuery1Container query1;
	EventLogQuery2Container query2;


	filter_container data(syntax_, debug_);

	bool bPerfData = true;
	bool bFilterNew = true;
	bool unique = false;
	unsigned int truncate = 0;
	event_log_buffer buffer(buffer_length_);
	bool bPush = true;
	eventlog_filter filter;
	/*
	try {
		event_log_buffer buffer(buffer_length_);
	} catch (std::exception e) {
		message = std::wstring(_T("Failed to allocate memory: ")) + strEx::string_to_wstring(e.what());
		return NSCAPI::returnUNKNOWN;
	}
	*/

	try {
		MAP_OPTIONS_BEGIN(stl_args)
			MAP_OPTIONS_NUMERIC_ALL(query1, _T(""))
			MAP_OPTIONS_EXACT_NUMERIC_ALL(query2, _T(""))
			MAP_OPTIONS_STR2INT(_T("truncate"), truncate)
			MAP_OPTIONS_BOOL_TRUE(_T("unique"), unique)
			MAP_OPTIONS_BOOL_TRUE(_T("descriptions"), data.bShowDescriptions)
			MAP_OPTIONS_PUSH(_T("file"), files)
			MAP_OPTIONS_BOOL_FALSE(IGNORE_PERFDATA, bPerfData)
			MAP_OPTIONS_BOOL_EX(_T("filter"), bFilterNew, _T("new"), _T("old"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), data.bFilterIn, _T("in"), _T("out"))
			MAP_OPTIONS_BOOL_EX(_T("filter"), data.bFilterAll, _T("all"), _T("any"))
			MAP_OPTIONS_BOOL_EX(_T("debug"), data.bDebug, _T("true"), _T("false"))
			MAP_OPTIONS_STR2INT(_T("debug-threshold"), data.debugThreshold)
			MAP_OPTIONS_STR(_T("syntax"), data.syntax)
			/*
			MAP_FILTER_OLD("filter-eventType", eventType)
			MAP_FILTER_OLD("filter-severity", eventSeverity)
			MAP_FILTER_OLD("filter-eventID", eventID)
			MAP_FILTER_OLD("filter-eventSource", eventSource)
			MAP_FILTER_OLD("filter-generated", timeGenerated)
			MAP_FILTER_OLD("filter-written", timeWritten)
			MAP_FILTER_OLD("filter-message", message)
*/
			MAP_FILTER(_T("filter+eventType"), eventType, filter_container::filter_plus)
			MAP_FILTER(_T("filter+severity"), eventSeverity, filter_container::filter_plus)
			MAP_FILTER(_T("filter+eventID"), eventID, filter_container::filter_plus)
			MAP_FILTER(_T("filter+eventSource"), eventSource, filter_container::filter_plus)
			MAP_FILTER(_T("filter+generated"), timeGenerated, filter_container::filter_plus)
			MAP_FILTER(_T("filter+written"), timeWritten, filter_container::filter_plus)
			MAP_FILTER(_T("filter+message"), message, filter_container::filter_plus)

			MAP_FILTER(_T("filter.eventType"), eventType, filter_container::filter_normal)
			MAP_FILTER(_T("filter.severity"), eventSeverity, filter_container::filter_normal)
			MAP_FILTER(_T("filter.eventID"), eventID, filter_container::filter_normal)
			MAP_FILTER(_T("filter.eventSource"), eventSource, filter_container::filter_normal)
			MAP_FILTER(_T("filter.generated"), timeGenerated, filter_container::filter_normal)
			MAP_FILTER(_T("filter.written"), timeWritten, filter_container::filter_normal)
			MAP_FILTER(_T("filter.message"), message, filter_container::filter_normal)

			MAP_FILTER(_T("filter-eventType"), eventType, filter_container::filter_minus)
			MAP_FILTER(_T("filter-severity"), eventSeverity, filter_container::filter_minus)
			MAP_FILTER(_T("filter-eventID"), eventID, filter_container::filter_minus)
			MAP_FILTER(_T("filter-eventSource"), eventSource, filter_container::filter_minus)
			MAP_FILTER(_T("filter-generated"), timeGenerated, filter_container::filter_minus)
			MAP_FILTER(_T("filter-written"), timeWritten, filter_container::filter_minus)
			MAP_FILTER(_T("filter-message"), message, filter_container::filter_minus)

			MAP_FILTER_LAST(_T("append-filter-eventType"), eventType)
			MAP_FILTER_LAST(_T("append-filter-severity"), eventSeverity)
			MAP_FILTER_LAST(_T("append-filter-eventID"), eventID)
			MAP_FILTER_LAST(_T("append-filter-eventSource"), eventSource)
			MAP_FILTER_LAST(_T("append-filter-generated"), timeGenerated)
			MAP_FILTER_LAST(_T("append-filter-written"), timeWritten)
			MAP_FILTER_LAST(_T("append-filter-message"), message)

			MAP_OPTIONS_STR(_T("filter"), data.filter)

			MAP_OPTIONS_MISSING(message, _T("Unknown argument: "))
			MAP_OPTIONS_END()
	} catch (filters::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (filters::filter_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
		} catch (checkHolders::parse_exception e) {
		message = e.getMessage();
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Invalid command line!");
		return NSCAPI::returnUNKNOWN;
	}

	unsigned long int hit_count = 0;
	if (files.empty()) {
		message = _T("No file specified try adding: file=Application");
		return NSCAPI::returnUNKNOWN;
	}
	bool buffer_error_reported = false;

	if (data.bDebug) {
		std::wstring str;
		BOOST_FOREACH(filter_container::filteritem_type item, data.filters) {
			if (item.first == filter_container::filter_normal)
				str += _T(". {");
			else if (item.first == filter_container::filter_plus)
				str += _T("+ {");
			else if (item.first == filter_container::filter_minus)
				str += _T("- {");
			else 
				str += _T("? {");

			str += item.second.to_string() + _T(" }");
		}
		NSC_DEBUG_MSG_STD(_T("Filter: ") + str);
	}

	boost::shared_ptr<any_mode_filter> filter_impl;
	if (bFilterNew) {
		filter_impl = boost::shared_ptr<any_mode_filter>(new second_mode_filter(data));
	} else {
		filter_impl = boost::shared_ptr<any_mode_filter>(new second_mode_filter(data));
	} if (!data.filter.empty()) {
		filter_impl = boost::shared_ptr<any_mode_filter>(new where_mode_filter(data));
	}

	if (!filter_impl) {
		message = _T("Failed to initialize filter subsystem.");
		return NSCAPI::returnUNKNOWN;
	}

	filter_impl->boot();

	__time64_t ltime;
	_time64(&ltime);

	NSC_DEBUG_MSG_STD(_T("Using: ") + filter_impl->get_name() + _T(" ") + filter_impl->get_subject());

	if (!filter_impl->validate(message)) {
		return NSCAPI::returnUNKNOWN;
	}


	NSC_DEBUG_MSG_STD(_T("Boot time: ") + strEx::itos(time.stop()));

	for (std::list<std::wstring>::const_iterator cit2 = files.begin(); cit2 != files.end(); ++cit2) {
		std::wstring name = *cit2;
		if (lookup_names_) {
			name = find_eventlog_name(*cit2);
			if ((*cit2) != name) {
				NSC_DEBUG_MSG_STD(_T("Opening alternative log: ") + name);
			}
		}
		HANDLE hLog = OpenEventLog(NULL, name.c_str());
		if (hLog == NULL) {
			message = _T("Could not open the '") + (*cit2) + _T("' event log: ") + error::lookup::last_error();
			return NSCAPI::returnUNKNOWN;
		}
		uniq_eventlog_map uniq_records;

		//DWORD dwThisRecord;
		DWORD dwRead, dwNeeded;


		//GetOldestEventLogRecord(hLog, &dwThisRecord);

		while (true) {
			BOOL bStatus = ReadEventLog(hLog, EVENTLOG_FORWARDS_READ|EVENTLOG_SEQUENTIAL_READ,
				0, buffer.getBufferUnsafe(), buffer.getBufferSize(), &dwRead, &dwNeeded);
			if (bStatus == FALSE) {
				DWORD err = GetLastError();
				if (err == ERROR_INSUFFICIENT_BUFFER) {
					if (!buffer_error_reported) {
						NSC_LOG_ERROR_STD(_T("EvenlogBuffer is too small change the value of ") + EVENTLOG_BUFFER + _T("=") + strEx::itos(dwNeeded+1) + _T(" under [EventLog] in nsc.ini : ") + error::lookup::last_error(err));
						buffer_error_reported = true;
					}
				} else if (err == ERROR_HANDLE_EOF) {
					break;
				} else {
					NSC_LOG_ERROR_STD(_T("Failed to read from eventlog: ") + error::lookup::last_error(err));
					message = _T("Failed to read from eventlog: ") + error::lookup::last_error(err);
					CloseEventLog(hLog);
					return NSCAPI::returnUNKNOWN;
				}
			}
			EVENTLOGRECORD *pevlr = buffer.getBufferUnsafe(); 
			while (dwRead > 0) { 
				EventLogRecord record((*cit2), pevlr, ltime);
				bool match = filter_impl->match(record);
				if (match&&unique) {
					match = false;
					uniq_eventlog_record uniq_record = pevlr;
					uniq_eventlog_map::iterator it = uniq_records.find(uniq_record);
					if (it != uniq_records.end()) {
						(*it).second ++;
						//match = false;
					}
					else {
						if (!data.syntax.empty()) {
							uniq_record.message = record.render(data.bShowDescriptions, data.syntax);
						} else if (!data.bShowDescriptions) {
							uniq_record.message = record.eventSource();
						} else {
							uniq_record.message = record.eventSource();
							uniq_record.message += _T("(") + EventLogRecord::translateType(record.eventType()) + _T(", ") + 
								strEx::itos(record.eventID()) + _T(", ") + EventLogRecord::translateSeverity(record.severity()) + _T(")");
							uniq_record.message += _T("[") + record.enumStrings() + _T("]");
							uniq_record.message += _T("{%count%}");
						}
						uniq_records[uniq_record] = 1;
					}
					hit_count++;
				} else if (match) {
					if (!data.syntax.empty()) {
						strEx::append_list(message, record.render(data.bShowDescriptions, data.syntax));
					} else if (!data.bShowDescriptions) {
						strEx::append_list(message, record.eventSource());
					} else {
						strEx::append_list(message, record.eventSource());
						message += _T("(") + EventLogRecord::translateType(record.eventType()) + _T(", ") + 
							strEx::itos(record.eventID()) + _T(", ") + EventLogRecord::translateSeverity(record.severity()) + _T(")");
						message += _T("[") + record.enumStrings() + _T("]");
					}
					hit_count++;
				}
				dwRead -= pevlr->Length; 
				pevlr = reinterpret_cast<EVENTLOGRECORD*>((LPBYTE)pevlr + pevlr->Length); 
			} 
		}
		CloseEventLog(hLog);
		for (uniq_eventlog_map::const_iterator cit = uniq_records.begin(); cit != uniq_records.end(); ++cit) {
			std::wstring msg = (*cit).first.message;
			strEx::replace(msg, _T("%count%"), strEx::itos((*cit).second));
			strEx::append_list(message, msg);
		}
	}
	NSC_DEBUG_MSG_STD(_T("Evaluation time: ") + strEx::itos(time.stop()));

	if (!bPerfData) {
		query1.perfData = false;
		query2.perfData = false;
	}
	if (query1.alias.empty())
		query1.alias = _T("eventlog");
	if (query2.alias.empty())
		query2.alias = _T("eventlog");
	if (query1.hasBounds())
		query1.runCheck(hit_count, returnCode, message, perf);
	else if (query2.hasBounds())
		query2.runCheck(hit_count, returnCode, message, perf);
	else {
		message = _T("No bounds specified!");
		return NSCAPI::returnUNKNOWN;
	}
	if ((truncate > 0) && (message.length() > (truncate-4)))
		message = message.substr(0, truncate-4) + _T("...");
	if (message.empty())
		message = _T("Eventlog check ok");
	return returnCode;
}


NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(gCheckEventLog);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gCheckEventLog);
