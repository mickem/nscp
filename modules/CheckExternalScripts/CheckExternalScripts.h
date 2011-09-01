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
#include <map>
#include <error.hpp>
#include <execute_process.hpp>

class CheckExternalScripts : public nscapi::impl::simple_plugin {
private:
	struct command_data {
		command_data() {}
		command_data(std::wstring command_, std::wstring arguments_) : command(command_) {
			parser_arguments(arguments_);
		}
		void parser_arguments(std::wstring args) {
			boost::tokenizer<boost::escaped_list_separator<wchar_t>, std::wstring::const_iterator, std::wstring> 
				tok(args, boost::escaped_list_separator<wchar_t>(L'\\', L' ', L'\"'));
			BOOST_FOREACH(std::wstring s, tok) {
				arguments.push_back(s);
			}
		}
		std::wstring get_argument() {
			std::wstring args;
			BOOST_FOREACH(std::wstring s, arguments) {
				if (!args.empty())
					args += _T(" ");
				args += s;
			}
			return args;
		}

		std::wstring command;
		std::list<std::wstring> arguments;
		std::wstring to_string() {
			std::wstring args;
			BOOST_FOREACH(std::wstring s, arguments) {
				if (!args.empty())
					args += _T(" ");
				args += s;
			}
			return command + _T("(") + get_argument() + _T(")");
		}
	};
	typedef std::map<std::wstring, command_data> command_list;
	command_list commands;
	command_list alias;
	unsigned int timeout;
	std::wstring scriptDirectory_;
	std::wstring root_;
	bool allowArgs_;
	bool allowNasty_;
	std::map<std::wstring,std::wstring> wrappings_;

public:
	CheckExternalScripts();
	virtual ~CheckExternalScripts();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();


	static std::wstring getModuleName() {
		return _T("Check External Scripts");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("A simple wrapper to run external scripts and batch files.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
	//NSCAPI::nagiosReturn handleCommand(const std::wstring command, std::list<std::wstring> arguments, std::wstring &message, std::wstring &perf);
	std::wstring getConfigurationMeta();

private:
	class NRPEException {
		std::wstring error_;
	public:
		NRPEException(std::wstring s) {
			error_ = s;
		}
		std::wstring getMessage() {
			return error_;
		}
	};


private:
	void addAllScriptsFrom(std::wstring path);
	void add_command(std::wstring key, std::wstring command) {
		strEx::token tok = strEx::getToken(command, ' ', true);
		boost::to_lower(key);
		command_data cd = command_data(tok.first, tok.second);
		commands[key.c_str()] = cd;
		get_core()->registerCommand(key.c_str(), _T("Script: ") + cd.to_string());
	}
	void add_alias(std::wstring key, std::wstring command) {
		strEx::token tok = strEx::getToken(command, ' ', true);
		boost::to_lower(key);
		command_data cd = command_data(tok.first, tok.second);
		alias[key.c_str()] = cd;
		get_core()->registerCommand(key.c_str(), _T("Alias for: ") + cd.to_string());
	}
	void add_wrapping(std::wstring key, std::wstring command) {
		strEx::token tok = strEx::getToken(command, ' ', true);
		std::wstring::size_type pos = tok.first.find_last_of(_T("."));
		std::wstring type;
		if (pos != std::wstring::npos)
			type = tok.first.substr(pos+1);

		std::wstring tpl = wrappings_[type];

		strEx::replace(tpl, _T("%SCRIPT%"), tok.first);
		strEx::replace(tpl, _T("%ARGS%"), tok.second);

		add_command(key,tpl);
	}
};

