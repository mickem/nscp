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
NSC_WRAPPERS_MAIN()
NSC_WRAPPERS_CHANNELS()

class Cache : public nscapi::impl::simple_plugin {
public:
	struct cache_query {
		std::string channel;
		std::string host;
		std::string alias;
		std::string command;
	};
private:
	typedef boost::function<std::string(const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload)> index_lookup_function;
	typedef boost::function<std::string(const cache_query &query)> command_lookup_function;
	typedef std::list<index_lookup_function> index_lookup_type;
	typedef std::list<command_lookup_function> command_lookup_type;
	index_lookup_type index_lookup_;
	command_lookup_type command_lookup_;

	std::map<std::string,std::string> cache_;
	boost::shared_mutex cache_mutex_;

public:
	Cache();
	virtual ~Cache();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	static std::wstring getModuleName() {
		return _T("SimpleCache module");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("Caches results for later checking.");
	}

	bool hasCommandHandler();
	bool hasMessageHandler();
	bool hasNotificationHandler() { return true; }
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t * command, const std::string &request, std::string &reply);
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &reply);

};
