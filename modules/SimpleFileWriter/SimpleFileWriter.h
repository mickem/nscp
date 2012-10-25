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

class FileWriter : public nscapi::impl::simple_plugin {
public:
private:
	typedef boost::function<std::string(const std::string channel, const Plugin::Common::Header &hdr, const Plugin::QueryResponseMessage::Response &payload)> index_lookup_function;
	typedef std::list<index_lookup_function> index_lookup_type;
	index_lookup_type index_lookup_;
	std::string filename_;
	//boost::shared_mutex cache_mutex_;


public:
	FileWriter();
	virtual ~FileWriter();
	// Module calls
	bool loadModule();
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	static std::wstring getModuleName() {
		return _T("SimpleFileWriter module");
	}
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 0, 1 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("FileWriters results for later checking.");
	}

	bool hasNotificationHandler() { return true; }
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &reply);

};
