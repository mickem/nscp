/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SamplePluginSimple.h"

// Empty constructor is called by the infrastructure when our plugin is loaded (once for each instance).
SamplePluginSimple::SamplePluginSimple() {}
SamplePluginSimple::~SamplePluginSimple() {}

// This is the load operation called when your plugin is officially loaded.
// The alias is mainly used to differentiate if the plugin is loaded more then once.
// A good approach is to use this (if it is not empty) when loading settings values to allow multiple instance to co-exist nicely.
// The mode is a bit undefined the idea is to allow the plugin to be loaded in off line mode.
// It is generally used to "not start servers" now.
// The load call should be fairly quick as this is done (currently) in serial.
// The main goal is to load your settings, register your self and start any servers
bool SamplePluginSimple::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	return true;
}
// Called when the plugin is unloaded
// kill off any active server instance here, unregister your self and free up all memory.
// Notice reload is currently implemented as unload/load so it is very possible that your plugin will be loaded and unloaded multiple times.
// Do not assume it is safe to forget freeing up memory and destroying resources here
bool SamplePluginSimple::unloadModule() {
	return true;
}
void SamplePluginSimple::sample_raw_command(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response) {
	response->set_command(request.command());
	if (request.arguments_size() > 0) {
		response->add_lines()->set_message("");
		response->set_result(Plugin::Common_ResultCode_OK);
	} else {
		response->add_lines()->set_message("Yaaay it works");
		response->set_result(Plugin::Common_ResultCode_OK);
	}
}