#include <boost/foreach.hpp>

#include "real_time_thread.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include <nscapi/macros.hpp>

void real_time_thread::process_no_events(const eventlog_filter::filter_config_object &object) {
	std::string response;
	std::string command = object.alias;
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.empty_msg, "", response)) {
		NSC_LOG_ERROR("Failed to submit result: " + response);
	}
}

void real_time_thread::process_record(eventlog_filter::filter_config_object &object, const EventLogRecord &record) {

	std::string response;
	int severity = object.severity;
	std::string command = object.alias;
	if (severity != -1) {
		object.filter.returnCode = severity;
	} else {
		object.filter.returnCode = NSCAPI::returnOK;
	}
	object.filter.start_match();
	bool matched = false;

	boost::tuple<bool,bool> ret = object.filter.match(boost::make_shared<eventlog_filter::filter_obj>(record));
	if (!ret.get<0>()) {
		if (ret.get<1>()) {
			return;
		}
	}
	std::string message = object.filter.get_message();
	if (message.empty())
		message = "Nothing matched";
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.filter.returnCode, message, "", response)) {
		NSC_LOG_ERROR("Failed to submit '" + message);
	}
}

void real_time_thread::debug_miss(const EventLogRecord &record) {
	//std::string message = record.render(true, "%id% %level% %source%: %message%", DATE_FORMAT_S, LANG_NEUTRAL);
	//NSC_DEBUG_MSG_STD("No filter matched: " + message);
}

void real_time_thread::thread_proc() {

	std::list<eventlog_filter::filter_config_object> filters;
	std::list<std::string> logs;
	std::list<std::string> filter_list;

	BOOST_FOREACH(const std::string &s, strEx::s::splitEx(logs_, std::string(","))) {
		logs.push_back(s);
	}

	BOOST_FOREACH(eventlog_filter::filter_config_object object, filters_.get_object_list()) {
		eventlog_filter::filter filter;
		std::string message;
		if (!object.boot(message)) {
			NSC_LOG_ERROR("Failed to load " + object.alias + ": " + message);
			continue;
		}
// 		eventlog_filter::filter_argument fargs = eventlog_filter::factories::create_argument(object.syntax, object.date_format);
// 		fargs->filter = object.filter;
// 		fargs->debug = object.debug;
// 		fargs->alias = object.alias;
// 		fargs->bShowDescriptions = true;
		BOOST_FOREACH(const eventlog_filter::file_container &f, object.files) {
			if (f.file != "any" && f.file != "all")
			logs.push_back(f.file);
		}
		filters.push_back(object);
		filter_list.push_back(object.alias);
	}
	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD("Scanning logs: " + utf8::cvt<std::string>(format::join(logs, ", ")));
	NSC_DEBUG_MSG_STD("Scanning filters: " + utf8::cvt<std::string>(format::join(filter_list, ", ")));

	typedef boost::shared_ptr<eventlog_wrapper> eventlog_type;
	typedef std::vector<eventlog_type> eventlog_list;
	eventlog_list evlog_list;

	BOOST_FOREACH(const std::string &l, logs) {
		eventlog_type el = eventlog_type(new eventlog_wrapper(l));
		if (!el->seek_end()) {
			NSC_LOG_ERROR_WA("Failed to find the end of eventlog: ", l);
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

	boost::posix_time::ptime current_time = boost::posix_time::ptime();

	BOOST_FOREACH(eventlog_filter::filter_config_object &object, filters) {
		object.touch(current_time);
	}

	unsigned int errors = 0;
	while (true) {
		bool first = true;
		boost::posix_time::ptime minNext;
		BOOST_FOREACH(const eventlog_filter::filter_config_object &object, filters) {
			if (object.max_age && (first || object.next_ok_ < minNext) ) {
				first = false;
				minNext = object.next_ok_;
			}
		}

		boost::posix_time::time_duration dur;
		if (first) {
			NSC_DEBUG_MSG("Next miss time is in: no timeout specified");
		} else {
			dur = minNext - boost::posix_time::ptime();
			NSC_DEBUG_MSG("Next miss time is in: " + strEx::s::xtos(dur.total_seconds()) + "s");
		}

		_time64(&ltime);

		DWORD dwWaitTime = INFINITE;
		if (!first)
			dwWaitTime = static_cast<DWORD>(dur.total_milliseconds());
		NSC_DEBUG_MSG("Next miss time is in: " + strEx::s::xtos(dwWaitTime) + "s");

		DWORD dwWaitReason = WaitForMultipleObjects(static_cast<DWORD>(evlog_list.size()+1), handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + evlog_list.size())) {

			int index = dwWaitReason-WAIT_OBJECT_0-1;
			eventlog_type el = evlog_list[index];
			DWORD status = el->read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ);
			if (ERROR_SUCCESS != status && ERROR_HANDLE_EOF != status) {
				NSC_LOG_ERROR("Assuming eventlog reset (re-reading from start)");
				el->un_notify(handles[index+1]);
				el->reopen();
				el->notify(handles[index+1]);
				el->seek_start();
			}

			_time64(&ltime);

			EVENTLOGRECORD *pevlr = el->read_record_with_buffer();
			while (pevlr != NULL) {
				EventLogRecord elr(el->get_name(), pevlr, ltime);
//				boost::shared_ptr<eventlog_filter::filter_obj> arg = boost::shared_ptr<eventlog_filter::filter_obj>(new eventlog_filter::filter_obj(elr));
				bool matched = false;

				BOOST_FOREACH(eventlog_filter::filter_config_object &object, filters) {
					bool found = false;
					BOOST_FOREACH(const eventlog_filter::file_container &f, object.files) {
						if (f.file == "any" || f.file == "all" || f.file == utf8::cvt<std::string>(el->get_name()))
							found = true;
					}
					if (!found) {
						NSC_DEBUG_MSG_STD("Skipping filter: " + utf8::cvt<std::string>(object.alias));
						continue;
					}
					current_time = boost::posix_time::ptime();

					bool matched = false;
					boost::tuple<bool,bool> ret = object.filter.match(boost::make_shared<eventlog_filter::filter_obj>(elr));
					if (!ret.get<0>()) {
						matched = true;
						if (ret.get<1>()) {
							break;
						}
					}

					if (!matched) {
						continue;
					}

					std::string command = object.alias;
					if (!object.command.empty())
						command = object.command;
					std::string message = object.filter.get_message();
					if (message.empty())
						message = "Nothing matched";
					if (!nscapi::core_helper::submit_simple_message(object.target, command, object.filter.returnCode, message, "", message)) {
						NSC_LOG_ERROR("Failed to submit '" + message);
					}
				}
				if (debug_ && !matched)
					debug_miss(elr);

				pevlr = el->read_record_with_buffer();
			}
		} else {
			NSC_LOG_ERROR_WA("Error failed to wait for eventlog message: ", error::lookup::last_error());
			if (errors++ > 10) {
				NSC_LOG_ERROR("To many errors giving up");
				delete [] handles;
				return;
			}
		}

		//current_time = boost::posix_time::ptime() + boost::posix_time::seconds(1);
		BOOST_FOREACH(eventlog_filter::filter_config_object &object, filters) {
			if (object.max_age && object.next_ok_ < current_time) {
				process_no_events(object);
				object.touch(current_time);
			} else {
				NSC_DEBUG_MSG_STD("missing: " + strEx::s::xtos(object.next_ok_));
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

void real_time_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		filters_.add(proxy, filters_path_, key, query, key == "default");
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
