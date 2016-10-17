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