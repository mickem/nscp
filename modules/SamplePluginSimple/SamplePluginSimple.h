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
#pragma once

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

// Your implementation class can derive from various helper implementations
// simple_plugin			- Hides ID handling in your plugin and allows you to register and access the various cores.
// simple_command_handler	- Provides a "nagios plugin" like command handler interface (so you wont have to deal with google protocol buffers)
// There is a bunch of others as well for wrapping the other APIs
class SamplePluginSimple : public nscapi::impl::simple_plugin {
private:

public:
	SamplePluginSimple();
	virtual ~SamplePluginSimple();

	// Declare exposed API methods (C++ versions)
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	void sample_raw_command(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
};