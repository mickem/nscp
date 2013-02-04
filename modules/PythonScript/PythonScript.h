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

#include <config.h>
#include <strEx.h>
#include <utils.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/python.hpp>

#include <protobuf/plugin.pb.h>

#include <scripts/functions.hpp>

struct python_script : public boost::noncopyable {
	std::string alias;
	unsigned int plugin_id;
	boost::python::dict localDict;
	python_script(unsigned int plugin_id, const std::string alias, const script_container& script);
	~python_script();
	bool callFunction(const std::string& functionName);
	bool callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2);
	bool callFunction(const std::string& functionName, const std::list<std::string> &args);
	void _exec(const std::string &scriptfile);
};


class PythonScript : public nscapi::impl::simple_plugin {
private:
	boost::filesystem::path root_;
	typedef script_container::list_type script_type;
	script_type scripts_;
	typedef std::list<boost::shared_ptr<python_script> > instance_list_type;
	instance_list_type instances_;
	std::wstring alias_;

public:
	PythonScript() {}
	virtual ~PythonScript() {}
	// Module calls
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	std::wstring get_alias() {
		return alias_;
	}
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
	void handleNotification(const std::string &channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);

private:
	bool loadScript(std::wstring alias, std::wstring script);
	NSCAPI::nagiosReturn execute_and_load_python(std::list<std::wstring> args, std::wstring &message);
	boost::optional<boost::filesystem::path> find_file(std::string file);

};
