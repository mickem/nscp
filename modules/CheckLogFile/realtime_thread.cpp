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

#include "realtime_thread.hpp"
#include "filter.hpp"
#include "realtime_data.hpp"

#include <parsers/filter/realtime_helper.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

#include <str/utils.hpp>
#include <error/error.hpp>
#include <simple_timer.hpp>

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <map>
#include <vector>
#include <time.h>

#ifndef WIN32
#include <poll.h>
#include <sys/inotify.h>
#endif


typedef parsers::where::realtime_filter_helper<runtime_data, filters::filter_config_object> filter_helper;

void real_time_thread::thread_proc() {
	filter_helper helper(core, plugin_id);
	std::list<std::string> logs;

	BOOST_FOREACH(boost::shared_ptr<filters::filter_config_object> object, filters_.get_object_list()) {
		runtime_data data;
		data.set_split(object->line_split, object->column_split);
		data.set_read_from_start(object->read_from_start);
		BOOST_FOREACH(const std::string &file, object->files) {
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
					NSC_LOG_ERROR("Failed to find folder for " + object->get_alias() + ": " + path.string());
					continue;
				}
			}
#else
			if (boost::filesystem::is_regular(path)) {
				logs.push_back(path.string());
			} else {
				NSC_LOG_ERROR("Failed to find folder for " + object->get_alias() + ": " + path.string());
				continue;
			}
#endif
		}
		helper.add_item(object, data, "logfile");
	}

	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD("Subscribing to folders: " + str::utils::joinEx(logs, ", "));
	std::vector<std::string> files_list(logs.begin(), logs.end());
#ifdef WIN32
	HANDLE *handles = new HANDLE[1 + files_list.size()];
	handles[0] = stop_event_;
	for (int i = 0; i < files_list.size(); i++) {
		handles[i + 1] = FindFirstChangeNotification(utf8::cvt<std::wstring>(files_list[i]).c_str(), TRUE, FILE_NOTIFY_CHANGE_SIZE);
	}
#else

	struct pollfd pollfds[2] = { { inotify_init(), POLLIN | POLLPRI, 0}, { stop_event_[0], POLLIN, 0} };

	int *wds = new int[logs.size()];
	for (std::size_t i = 0; i < files_list.size(); i++) {
		wds[i] = inotify_add_watch(pollfds[0].fd, files_list[i].c_str(), IN_MODIFY);
	}

#endif

	helper.touch_all();

	while (true) {
		filter_helper::op_duration dur = helper.find_minimum_timeout();
		std::string trigger_folder;
#ifdef WIN32
		DWORD dwWaitTime = INFINITE;
		if (dur && dur->total_milliseconds() < 0)
			dwWaitTime = 0;
		else if (dur)
			dwWaitTime = dur->total_milliseconds();
		DWORD dwWaitReason = WaitForMultipleObjects(logs.size() + 1, handles, FALSE, dwWaitTime);
		if (dwWaitReason == WAIT_TIMEOUT) {
			// we take care of this below...
		} else if (dwWaitReason == WAIT_OBJECT_0) {
			delete[] handles;
			return;
		} else if (dwWaitReason > WAIT_OBJECT_0 && dwWaitReason <= (WAIT_OBJECT_0 + files_list.size())) {
			int id = dwWaitReason - WAIT_OBJECT_0;
			FindNextChangeNotification(handles[id]);
			trigger_folder = files_list[id-1];
		}
#else

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

		int timeout = 1000 * 60;
		if (dur)
			timeout = dur->total_milliseconds();
		char buffer[BUF_LEN];
		int length = poll(pollfds, 2, timeout);
		if (!length) {
			continue;
		} else if (length < 0) {
			NSC_LOG_ERROR("read failed!");
			continue;
		} else if (pollfds[1].revents != 0) {
			return;
		} else if (pollfds[0].revents != 0) {
			length = read(pollfds[0].fd, buffer, BUF_LEN);
			for (int j = 0; j < length;) {
				struct inotify_event * event = (struct inotify_event *) &buffer[j];
				trigger_folder = event->name;
				j += EVENT_SIZE + event->len;
			}
		} else {
			NSC_LOG_ERROR("Strange, please report this...");
		}
#endif
		helper.process_items(boost::shared_ptr<runtime_data::transient_data_impl>(new runtime_data::transient_data_impl(trigger_folder)));

	}

#ifdef WIN32
	delete[] handles;
#else
	for (std::size_t i = 0; i < files_list.size(); i++) {
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
	stop_event_ = CreateEvent(NULL, TRUE, FALSE, L"EventLogShutdown");
#else
	if (pipe(stop_event_) == -1) {
		NSC_LOG_ERROR("Failed to create pipe");
	}
#endif
	thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&real_time_thread::thread_proc, this)));
	return true;
}
bool real_time_thread::stop() {
	if (!enabled_)
		return true;
#ifdef WIN32
	SetEvent(stop_event_);
#else
	if (write(stop_event_[1], " ", 4) == -1) {
		NSC_LOG_ERROR("Failed to signal a stop");
	}
#endif
	if (thread_)
		thread_->join();
	return true;
}

void real_time_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::string key, std::string query) {
	try {
		filters_.add(proxy, key, query);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + utf8::cvt<std::string>(key), e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + utf8::cvt<std::string>(key));
	}
}
