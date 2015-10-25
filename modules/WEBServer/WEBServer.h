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
	log_list get_errors(int &position);
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
	void submitMetrics(const Plugin::MetricsMessage::Response &response);
	bool install_server(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
	bool password(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response);
private:

	boost::shared_ptr<Mongoose::Server> server;
};

