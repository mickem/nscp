#include <boost/foreach.hpp>

#include "real_time_thread.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <nscapi/macros.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

void real_time_thread::process_no_events(const filters::filter_config_object &object) {
	std::wstring response;
	std::wstring command = object.alias;
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.ok_msg, object.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit evenhtlog result: ") + response);
	}
}

void real_time_thread::process_record(const filters::filter_config_object &object, const EventLogRecord &record) {
	std::wstring response;
	int severity = object.severity;
	std::wstring command = object.alias;
	if (severity == -1) {
		NSC_LOG_ERROR(_T("Severity not defined for: ") + object.alias);
		severity = NSCAPI::returnUNKNOWN;
	}
	if (!object.command.empty())
		command = object.command;
	std::wstring message = record.render(true, object.syntax, object.date_format, object.dwLang);
	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.severity, message, object.perf_msg, response)) {
		NSC_LOG_ERROR(_T("Failed to submit eventlog result ") + object.alias + _T(": ") + response);
	}
}

void real_time_thread::debug_miss(const EventLogRecord &record) {
	std::wstring message = record.render(true, _T("%id% %level% %source%: %message%"), DATE_FORMAT, LANG_NEUTRAL);
	NSC_DEBUG_MSG_STD(_T("No filter matched: ") + message);
}

void real_time_thread::thread_proc() {

	std::list<filters::filter_config_object> filters;
	std::list<std::wstring> logs;
	std::list<std::wstring> filter_list;

	BOOST_FOREACH(std::wstring s, strEx::splitEx(logs_, _T(","))) {
		logs.push_back(s);
	}

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(object.syntax, object.date_format);
		fargs->filter = object.filter;
		fargs->debug = object.debug;
		fargs->alias = object.alias;
		fargs->bShowDescriptions = true;
		if (object.log_ != _T("any") && object.log_ != _T("all"))
			logs.push_back(object.log_);
		// eventlog_filter::filter_engine 
		object.engine = eventlog_filter::factories::create_engine(fargs);

		if (!object.engine) {
			NSC_LOG_ERROR_STD(_T("Invalid filter: ") + object.filter);
			continue;
		}

		if (!object.engine->boot()) {
			NSC_LOG_ERROR_STD(_T("Error booting filter: ") + object.filter);
			continue;
		}

		std::wstring message;
		if (!object.engine->validate(message)) {
			NSC_LOG_ERROR_STD(_T("Error validating filter: ") + message);
			continue;
		}
		filters.push_back(object);
		filter_list.push_back(object.alias);
	}
	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD(_T("Scanning logs: ") + strEx::joinEx(logs, _T(", ")));
	NSC_DEBUG_MSG_STD(_T("Scanning filters: ") + strEx::joinEx(filter_list, _T(", ")));

	typedef boost::shared_ptr<eventlog_wrapper> eventlog_type;
	typedef std::vector<eventlog_type> eventlog_list;
	eventlog_list evlog_list;

	BOOST_FOREACH(std::wstring l, logs) {
		eventlog_type el = eventlog_type(new eventlog_wrapper(l));
		if (!el->seek_end()) {
			NSC_LOG_ERROR_STD(_T("Failed to find the end of eventlog: ") + l);
		} else {
			evlog_list.push_back(el);
		}
	}

	// TODO: add support for scanning "missed messages" at startup

	HANDLE *handles = new HANDLE[1+evlog_list.size()];
	handles[0] = stop_event_;
	for (int i=0;i<evlog_list.size();i++) {
		evlog_list[i]->notify(handles[i+1]);
	}
	__time64_t ltime;
	_time64(&ltime);

	BOOST_FOREACH(filters::filter_config_object &object, filters) {
		object.touch(ltime);
	}

	unsigned int errors = 0;
	while (true) {

		DWORD minNext = INFINITE;
		BOOST_FOREACH(const filters::filter_config_object &object, filters) {
			NSC_DEBUG_MSG_STD(_T("Getting next from: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			if (object.next_ok_ > 0 && object.next_ok_ < minNext)
				minNext = object.next_ok_;
		}

		_time64(&ltime);

		if (ltime > minNext) {
			NSC_LOG_ERROR(_T("Strange seems we expect to send ok now?"));
			continue;
		}
		DWORD dwWaitTime = (minNext - ltime)*1000;
		if (minNext == INFINITE || dwWaitTime < 0)
			dwWaitTime = INFINITE;
		NSC_DEBUG_MSG(_T("Next miss time is in: ") + strEx::itos(dwWaitTime) + _T("s"));

		DWORD dwWaitReason = WaitForMultipleObjects(evlog_list.size()+1, handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + evlog_list.size())) {

			eventlog_type el = evlog_list[dwWaitReason-WAIT_OBJECT_0-1];
			DWORD status = el->read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ);
			if (ERROR_SUCCESS != status && ERROR_HANDLE_EOF != status) {
				delete [] handles;
				return;
			}

			_time64(&ltime);

			EVENTLOGRECORD *pevlr = el->read_record_with_buffer();
			while (pevlr != NULL) {
				EventLogRecord elr(el->get_name(), pevlr, ltime);
				boost::shared_ptr<eventlog_filter::filter_obj> arg = boost::shared_ptr<eventlog_filter::filter_obj>(new eventlog_filter::filter_obj(elr));
				bool matched = false;

				BOOST_FOREACH(filters::filter_config_object &object, filters) {
					if (object.log_ != _T("any") && object.log_ != _T("all") && object.log_ != el->get_name()) {
						NSC_DEBUG_MSG_STD(_T("Skipping filter: ") + object.alias);
						continue;
					}
					if (object.engine->match(arg)) {
						process_record(object, elr);
						object.touch(ltime);
						matched = true;
					}
				}
				if (debug_ && !matched)
					debug_miss(elr);

				pevlr = el->read_record_with_buffer();
			}
		} else {
			NSC_LOG_ERROR(_T("Error failed to wait for eventlog message: ") + error::lookup::last_error());
			if (errors++ > 10) {
				NSC_LOG_ERROR(_T("To many errors giving up"));
				delete [] handles;
				return;
			}
		}
		_time64(&ltime);
		BOOST_FOREACH(filters::filter_config_object &object, filters) {
			if (object.next_ok_ != 0 && object.next_ok_ <= (ltime+1)) {
				process_no_events(object);
				object.touch(ltime);
			} else {
				NSC_DEBUG_MSG_STD(_T("missing: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			}
		}
	}
	delete [] handles;
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;

	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));

	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
	SetEvent(stop_event_);
	if (thread_)
		thread_->join();
	return true;
}

void real_time_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring key, std::wstring query) {
	try {
		filters_.add(proxy, filters_path_, key, query, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key);
	}
}
