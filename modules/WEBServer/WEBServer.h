/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>

//#define MONGOOSE_NO_FILESYSTEM
#define MONGOOSE_NO_AUTH
#define MONGOOSE_NO_CGI
#define MONGOOSE_NO_SSI

#include <mongoose/Server.h>
#include <mongoose/WebController.h>
#include <mongoose/StreamResponse.h>

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/plugin.hpp>

struct error_handler {
	struct status {
		status() : error_count(0) {}
		std::string last_error;
		unsigned int error_count;
	};
	struct log_entry {
		int line;
		std::string type;
		std::string file;
		std::string message;
		std::string date;
	};
	typedef std::vector<log_entry> log_list;
	error_handler() : error_count_(0) {}
	void add_message(bool is_error, const log_entry &message);
	void reset();
	status get_status();
	log_list get_errors(std::size_t &position);
private:
	boost::timed_mutex mutex_;
	log_list log_entries;
	std::string last_error_;
	unsigned int error_count_;
};

struct metrics_handler {
	void set(const std::string &metrics);
	std::string get();
private:
	std::string metrics_;
	boost::timed_mutex mutex_;
};

class WEBServer : public nscapi::impl::simple_plugin {
public:
	WEBServer();
	virtual ~WEBServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void handleLogMessage(const Plugin::LogEntry::Entry &message);
	bool commandLineExec(const int target_mode, const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	void submitMetrics(const Plugin::MetricsMessage &response);
	bool install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool password(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
private:

	boost::shared_ptr<Mongoose::Server> server;
};
