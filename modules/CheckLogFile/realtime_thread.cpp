#include "stdafx.h"

#ifndef WIN32
#include <poll.h>
#include <sys/inotify.h>
#endif

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <time.h>
#include <error.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

#include <simple_timer.hpp>
#include <settings/client/settings_client.hpp>

#include "realtime_thread.hpp"
#include "filter.hpp"

#include "realtime_data.hpp"
#include <parsers/filter/realtime_helper.hpp>

typedef parsers::where::realtime_filter_helper<runtime_data, filters::filter_config_object> filter_helper;

void real_time_thread::thread_proc() {

	filter_helper helper;
	std::list<std::string> logs;

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		runtime_data data;
		data.set_split(object.line_split, object.column_split);
		BOOST_FOREACH(const std::string &file, object.files) {
			boost::filesystem::path path = file;
			data.add_file(path);
#ifdef WIN32
			if (boost::filesystem::is_directory(path)) {
				logs.push_back(path.string());
			} else {
				path = path.remove_filename();
				if (boost::filesystem::is_directory(path)) {
					logs.push_back(path.string());
				} else {
					NSC_LOG_ERROR("Failed to find folder for " + utf8::cvt<std::string>(object.tpl.alias) + ": " + path.string());
					continue;
				}
			}
#else
			if (boost::filesystem::is_regular(path)) {
				logs.push_back(path.string());
			} else {
				NSC_LOG_ERROR("Failed to find folder for " + object.tpl.alias + ": " + path.string());
				continue;
			}
#endif
		}
		helper.add_item(object, data);
	}

	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD("Subscribing to folders: " + strEx::s::joinEx(logs, ", "));
	std::vector<std::string> files_list(logs.begin(), logs.end());
#ifdef WIN32
	HANDLE *handles = new HANDLE[1+logs.size()];
	handles[0] = stop_event_;
	for (int i=0;i<files_list.size();i++) {
		handles[i+1] = FindFirstChangeNotification(utf8::cvt<std::wstring>(files_list[i]).c_str(), TRUE, FILE_NOTIFY_CHANGE_SIZE);
	}
#else

	struct pollfd pollfds[2] = { { inotify_init(), POLLIN|POLLPRI, 0}, { stop_event_[0], POLLIN, 0}};

	int *wds = new int[logs.size()];
	for (int i=0;i<files_list.size();i++) {
		wds[i] = inotify_add_watch(pollfds[0].fd, files_list[i].c_str(), IN_MODIFY);
	}

#endif

	helper.touch_all();

	while (true) {
		filter_helper::op_duration dur = helper.find_minimum_timeout();
#ifdef WIN32
		DWORD dwWaitTime = INFINITE;
		if (dur && dur->total_milliseconds() < 0)
			dwWaitTime = 0;
		else if (dur)
			dwWaitTime = dur->total_milliseconds();
		DWORD dwWaitReason = WaitForMultipleObjects(logs.size()+1, handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete [] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + files_list.size())) {
			FindNextChangeNotification(handles[dwWaitReason-WAIT_OBJECT_0]);
		}
#else

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

		int timeout = 1000*60;
		if (dur)
			timeout = dur->total_milliseconds();
		char buffer[BUF_LEN];
		int length = poll(pollfds, 2, timeout);
		if(!length) {
			continue;
		} else if(length < 0) {
			NSC_LOG_ERROR("read failed!");
			continue;
		}
		else if (pollfds[1].revents != 0) {
			return;
		} else if (pollfds[0].revents != 0) {
			length = read(pollfds[0].fd, buffer, BUF_LEN);  
			for (int j=0;j<length;) {
				struct inotify_event * event = (struct inotify_event *) &buffer[j];
				std::wstring wstr = utf8::cvt<std::wstring>( event->name);
				j += EVENT_SIZE + event->len;
			}
		} else {
			NSC_LOG_ERROR("Strange, please report this...");
		}
#endif
		helper.process_items(0);
	}

#ifdef WIN32
	delete [] handles;
#else
	for (int i=0;i<files_list.size();i++) {
		inotify_rm_watch(pollfds[0].fd, wds[i]);
	}
	close(pollfds[0].fd);
	//close(pollfds[1].fd);
#endif
	return;
}


bool real_time_thread::start() {
	if (!enabled_)
		return true;
#ifdef WIN32
 	stop_event_ = CreateEvent(NULL, TRUE, FALSE, _T("EventLogShutdown"));
#else
	pipe(stop_event_);
#endif
	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
#ifdef WIN32
 	SetEvent(stop_event_);
#else
	write(stop_event_[1], " ", 4);
#endif
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
