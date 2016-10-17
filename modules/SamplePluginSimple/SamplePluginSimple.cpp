/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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