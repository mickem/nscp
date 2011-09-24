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
NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();
NSC_WRAPPERS_CHANNELS();

#include <config.h>
#include <strEx.h>
#include <utils.h>
//#include <checkHelpers.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/python.hpp>

#include <scripts/functions.hpp>

struct python_script : public boost::noncopyable {
	unsigned int plugin_id;
	std::string alias;
	boost::python::dict localDict;
	python_script(unsigned int plugin_id, const std::string alias, const script_container& script);
	~python_script();
	void callFunction(const std::string& functionName);
	void callFunction(const std::string& functionName, unsigned int i1, const std::string &s1, const std::string &s2);
	void _exec(const std::string &scriptfile);
};


class PythonScript : public nscapi::impl::simple_plugin {
private:
	boost::filesystem::wpath root_;
	typedef script_container::list_type script_type;
	script_type scripts_;
	typedef std::list<boost::shared_ptr<python_script> > instance_list_type;
	instance_list_type instances_;
	std::wstring alias_;

public:
	PythonScript();
	virtual ~PythonScript();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);

	bool unloadModule();
	bool reload(std::wstring &msg);
	std::wstring get_alias() {
		return alias_;
	}

	static std::wstring getModuleName() {
		return _T("PythonScript");
	}
	static std::wstring getModuleDescription() {
		return _T("PythonScript...");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	bool hasNotificationHandler();
	bool loadScript(std::wstring alias, std::wstring script);
	//NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);

	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
	NSCAPI::nagiosReturn handleRAWNotification(const std::wstring &channel, std::string &request, std::string &response);

	NSCAPI::nagiosReturn execute_and_load_python(std::list<std::wstring> args);
	//NSCAPI::nagiosReturn RunLUA(const unsigned int argLen, wchar_t **char_args, std::wstring &message, std::wstring &perf);
	//NSCAPI::nagiosReturn extract_return(Lua_State &L, int arg_count,  std::wstring &message, std::wstring &perf);

	//script_wrapper::lua_handler
	//void register_command(script_wrapper::lua_script* script, std::wstring command, std::wstring function);

private:

	boost::optional<boost::filesystem::wpath> find_file(std::wstring file);

};
