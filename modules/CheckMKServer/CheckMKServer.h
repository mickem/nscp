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

#include <nscapi/nscapi_plugin_impl.hpp>
#include <check_mk/server/server_protocol.hpp>
#include "handler_impl.hpp"

class CheckMKServer : public nscapi::impl::simple_plugin {
public:
	CheckMKServer();
	virtual ~CheckMKServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

private:

	bool add_script(std::string alias, std::string file);

	socket_helpers::connection_info info_;
	boost::shared_ptr<check_mk::server::server> server_;
	boost::shared_ptr<handler_impl> handler_;
	boost::shared_ptr<scripts::script_manager<lua::lua_traits> > scripts_;
	boost::shared_ptr<lua::lua_runtime> lua_runtime_;
	boost::shared_ptr<scripts::nscp::nscp_runtime_impl> nscp_runtime_;
	boost::filesystem::path root_;
};
