#include "stdafx.h"

#ifndef WIN32
#include <poll.h>
#include <sys/inotify.h>
#endif

#include <map>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include <parsers/expression/expression.hpp>

#include <time.h>
#include <utils.h>
#include <error.hpp>

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>

#include <parsers/where/unary_fun.hpp>
#include <parsers/where/list_value.hpp>
#include <parsers/where/binary_op.hpp>
#include <parsers/where/unary_op.hpp>
#include <parsers/where/variable.hpp>

#include <simple_timer.hpp>
#include <settings/client/settings_client.hpp>

#include "real_time_thread.hpp"
#include "filter.hpp"

void real_time_thread::process_timeout(const filters::filter_config_object &object) {
	std::wstring response;
	std::wstring command = object.alias;
	NSC_LOG_ERROR(_T("Processing timeout: ") + object.alias);
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, NSCAPI::returnOK, object.empty_msg, _T(""), response)) {
		NSC_LOG_ERROR(_T("Failed to submit result: ") + response);
	}
}

void real_time_thread::process_object(filters::filter_config_object &object) {
	std::wstring response;
	int severity = object.severity;
	std::wstring command = object.alias;
	if (severity != -1) {
		object.filter.returnCode = severity;
	} else {
		object.filter.returnCode = NSCAPI::returnOK;
	}
	object.filter.reset();

	bool matched = false;
	NSC_LOG_ERROR(_T("Processing object: ") + object.alias);
	BOOST_FOREACH(filters::file_container &c, object.files) {
		boost::uintmax_t sz = boost::filesystem::file_size(c.file);
		std::string fname = utf8::cvt<std::string>(c.file);
		std::ifstream file(fname.c_str());
		if (file.is_open()) {
			std::string line;
			object.filter.summary.filename = fname;
			if (sz == c.size) {
				continue;
			} else if (sz > c.size) {
				file.seekg(c.size);
			}
			while (file.good()) {
				std::getline(file,line, '\n');
				if (!object.column_split.empty()) {
					std::list<std::string> chunks = strEx::s::splitEx(line, utf8::cvt<std::string>(object.column_split));
					boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(object.filter.summary.filename, line, chunks, object.filter.summary.match_count));
					boost::tuple<bool,bool> ret = object.filter.match(record);
					if (ret.get<0>()) {
						matched = true;
						if (ret.get<1>()) {
							break;
						}
					}
				}
			}
			file.close();
		} else {
			NSC_LOG_ERROR(_T("Failed to open file: ") + c.file);
		}
	}
	if (!matched) {
		return;
	}

	std::string message;
	if (object.filter.message.empty())
		message = "Nothing matched";
	else
		message = object.filter.message;
	if (!object.command.empty())
		command = object.command;
	if (!nscapi::core_helper::submit_simple_message(object.target, command, object.filter.returnCode, utf8::cvt<std::wstring>(message), _T(""), response)) {
		NSC_LOG_ERROR(_T("Failed to submit '") + utf8::cvt<std::wstring>(message) + _T("' ") + object.alias + _T(": ") + response);
	}
}

void real_time_thread::thread_proc() {

	std::list<filters::filter_config_object> filters;
	std::list<std::wstring> logs;

	BOOST_FOREACH(filters::filter_config_object object, filters_.get_object_list()) {
		logfile_filter::filter filter;
		std::string message;
		if (!object.boot(message)) {
			NSC_LOG_ERROR(_T("Failed to load ") + object.alias + _T(": ") + utf8::cvt<std::wstring>(message));
			continue;
		}
		BOOST_FOREACH(const filters::file_container &fc, object.files) {
			boost::filesystem::wpath path = fc.file;
#ifdef WIN32
			if (boost::filesystem::is_directory(path)) {
				logs.push_back(path.string());
			} else {
				path = path.remove_filename();
				if (boost::filesystem::is_directory(path)) {
					logs.push_back(path.string());
				} else {
					NSC_LOG_ERROR(_T("Failed to find folder for ") + object.alias + _T(": ") + fc.file);
					continue;
				}
			}
#else
			if (boost::filesystem::is_regular(path)) {
				logs.push_back(path.string());
			} else {
				NSC_LOG_ERROR(_T("Failed to find folder for ") + object.alias + _T(": ") + fc.file);
				continue;
			}
#endif
		}
		filters.push_back(object);
	}

	logs.sort();
	logs.unique();
	NSC_DEBUG_MSG_STD(_T("Scanning folders: ") + strEx::joinEx(logs, _T(", ")));
	std::vector<std::wstring> files_list(logs.begin(), logs.end());
#ifdef WIN32
	HANDLE *handles = new HANDLE[1+logs.size()];
	handles[0] = stop_event_;
	for (int i=0;i<files_list.size();i++) {
		NSC_DEBUG_MSG_STD(_T("Adding folder: ") + files_list[i]);
		handles[i+1] = FindFirstChangeNotification(files_list[i].c_str(), TRUE, FILE_NOTIFY_CHANGE_SIZE);
	}
#else

	struct pollfd pollfds[2] = { { inotify_init(), POLLIN|POLLPRI, 0}, { stop_event_[0], POLLIN, 0}};

	int *wds = new int[logs.size()];
	for (int i=0;i<files_list.size();i++) {
		NSC_DEBUG_MSG_STD(_T("Adding folder: ") + files_list[i]);
		wds[i] = inotify_add_watch(pollfds[0].fd, utf8::cvt<std::string>(files_list[i]).c_str(), IN_MODIFY);
	}

#endif

	boost::posix_time::ptime current_time;
	BOOST_FOREACH(filters::filter_config_object &object, filters) {
		object.touch(current_time);
	}
	while (true) {
		bool first = true;
		boost::posix_time::ptime minNext;
		BOOST_FOREACH(const filters::filter_config_object &object, filters) {
			NSC_DEBUG_MSG_STD(_T("Getting next from: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			if (object.max_age && (first || object.next_ok_ < minNext) ) {
				first = false;
				minNext = object.next_ok_;
			}
		}

		boost::posix_time::time_duration dur;
		if (first) {
			NSC_DEBUG_MSG(_T("Next miss time is in: no timeout specified"));
		} else {
			dur = minNext - boost::posix_time::ptime();
			NSC_DEBUG_MSG(_T("Next miss time is in: ") + strEx::itos(dur.total_seconds()) + _T("s"));
		}

#ifdef WIN32
		DWORD dwWaitTime = INFINITE;
		if (!first)
			dwWaitTime = dur.total_milliseconds();
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
		if (!first)
			timeout = dur.total_milliseconds();
		char buffer[BUF_LEN];
		int length = poll(pollfds, 2, timeout);
		if( !length )
		{
			continue;
		}
		else if( length < 0 )
		{
			NSC_LOG_ERROR(_T("read failed!"));
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
			NSC_LOG_ERROR(_T("Strange, please report this..."));
		}
#endif

		current_time = boost::posix_time::ptime();

		BOOST_FOREACH(filters::filter_config_object &object, filters) {
 			if (object.has_changed()) {
 				process_object(object);
				object.touch(current_time);
 			}
		}

		//current_time = boost::posix_time::ptime() + boost::posix_time::seconds(1);
		BOOST_FOREACH(filters::filter_config_object &object, filters) {
			if (object.max_age && object.next_ok_ < current_time) {
				process_timeout(object);
				object.touch(current_time);
			} else {
				NSC_DEBUG_MSG_STD(_T("missing: ") + object.alias + _T(": ") + strEx::itos(object.next_ok_));
			}
		}

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

void real_time_thread::add_realtime_filter(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring key, std::wstring query) {
	try {
		filters_.add(proxy, filters_path_, key, query, key == _T("default"));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add command: ") + key);
	}
}


