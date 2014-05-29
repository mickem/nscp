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
	error_handler() : error_count_(0) {}
	void add_message(bool is_error, std::string message);
	void reset();
	status get_status();
	std::string get_errors(int &position);
private:
	boost::timed_mutex mutex_;
	std::list<std::string> log_entries;
	std::string last_error_;
	unsigned int error_count_;

};
class WEBServer : public nscapi::impl::simple_plugin {
public:
	WEBServer();
	virtual ~WEBServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void handleLogMessage(const Plugin::LogEntry::Entry &message);

private:

	boost::shared_ptr<Mongoose::Server> server;
};

