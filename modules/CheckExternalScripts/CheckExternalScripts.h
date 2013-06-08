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

#include <map>
#include <error.hpp>
#include <execute_process.hpp>
#include "commands.hpp"
#include "alias.hpp"

class CheckExternalScripts : public nscapi::impl::simple_plugin {
private:
	commands::command_handler commands_;
	alias::command_handler aliases_;
	unsigned int timeout;
	std::string commands_path;
	std::string aliases_path;
	std::wstring scriptDirectory_;
	std::string root_;
	bool allowArgs_;
	bool allowNasty_;
	std::map<std::string,std::string> wrappings_;

public:
	CheckExternalScripts();
	virtual ~CheckExternalScripts();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();
	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);



private:

	void handle_command(const commands::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage::Response *response);
	void handle_alias(const alias::command_object &cd, const std::list<std::string> &args, Plugin::QueryResponseMessage::Response *response);
	void addAllScriptsFrom(std::wstring path);
	void add_command(std::string key, std::string arg);
	void add_alias(std::string key, std::string command);
	void add_wrapping(std::string key, std::string command) {
		strEx::s::token tok = strEx::s::getToken(command, ' ');
		std::string::size_type pos = tok.first.find_last_of(".");
		std::string type;
		if (pos != std::wstring::npos)
			type = tok.first.substr(pos+1);

		std::string tpl = wrappings_[type];
		if (tpl.empty()) {
			NSC_LOG_ERROR("Failed to find wrapping for type: " + type);
		} else {
			strEx::replace(tpl, "%SCRIPT%", tok.first);
			strEx::replace(tpl, "%ARGS%", tok.second);
			add_command(key,tpl);
		}
	}
};

