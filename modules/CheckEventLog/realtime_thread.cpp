#include <boost/foreach.hpp>

#include <format.hpp>

#include "realtime_thread.hpp"
#include "realtime_data.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <parsers/filter/realtime_helper.hpp>

typedef parsers::where::realtime_filter_helper<runtime_data, eventlog_filter::filter_config_object> filter_helper;

void real_time_thread::thread_proc() {
	filter_helper helper(core, plugin_id);
	std::list<std::string> logs;

	BOOST_FOREACH(const std::string &s, strEx::s::splitEx(logs_, std::string(","))) {
		logs.push_back(s);
	}

	BOOST_FOREACH(eventlog_filter::filter_config_object object, filters_.get_object_list()) {
		runtime_data data;
		BOOST_FOREACH(const std::string &f, object.files) {
			if (f != "any" && f != "all") {
				logs.push_back(f);
				data.add_file(f);
			}
		}
		helper.add_item(object, data);
	}
	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD("Scanning logs: " + utf8::cvt<std::string>(format::join(logs, ", ")));

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

	helper.touch_all();

	unsigned int errors = 0;
	while (true) {
		filter_helper::op_duration dur = helper.find_minimum_timeout();

		_time64(&ltime);

		DWORD dwWaitTime = INFINITE;
		if (dur && dur->total_milliseconds() < 0)
			dwWaitTime = 0;
		else if (dur)
			dwWaitTime = dur->total_milliseconds();

		NSC_DEBUG_MSG("Sleeping for: " + strEx::s::xtos(dwWaitTime) + "ms");
		DWORD dwWaitReason = WaitForMultipleObjects(static_cast<DWORD>(evlog_list.size()+1), handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			helper.process_no_items();
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + evlog_list.size())) {

			int index = dwWaitReason-WAIT_OBJECT_0-1;
			eventlog_type el = evlog_list[index];
			NSC_DEBUG_MSG_STD("Reading eventlog messages...");
			DWORD status = el->read_record(0, EVENTLOG_SEQUENTIAL_READ|EVENTLOG_FORWARDS_READ);
			if (status != ERROR_SUCCESS  && status != ERROR_HANDLE_EOF) {
				NSC_LOG_MESSAGE("Assuming eventlog reset (re-reading from start)");
				el->un_notify(handles[index+1]);
				el->reopen();
				el->notify(handles[index+1]);
				el->seek_start();
			}

			_time64(&ltime);

			EVENTLOGRECORD *pevlr = el->read_record_with_buffer();
			while (pevlr != NULL) {
				NSC_DEBUG_MSG_STD("Processing: " + strEx::s::xtos(pevlr));
				EventLogRecord elr(el->get_name(), pevlr, ltime);
				helper.process_items(elr);
				pevlr = el->read_record_with_buffer();
			}
		} else {
			NSC_LOG_ERROR("Error failed to wait for eventlog message: " + error::lookup::last_error());
			if (errors++ > 10) {
				NSC_LOG_ERROR("To many errors giving up");
				delete [] handles;
				return;
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
