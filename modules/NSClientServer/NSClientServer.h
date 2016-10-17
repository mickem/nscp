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

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

#include <check_nt/server/protocol.hpp>

class NSClientServer : public nscapi::impl::simple_plugin, public check_nt::server::handler {
public:
	NSClientServer();
	virtual ~NSClientServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	check_nt::packet handle(check_nt::packet packet);

	check_nt::packet create_error(std::string msg) {
		return check_nt::packet("ERROR: Failed to parse");
	}

	void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}

	std::string get_password() const {
		return password_;
	}

private:
	void set_password(std::string password) {
		password_ = password;
	}
	virtual void set_allow_arguments(bool v) {
		allowArgs_ = v;
	}
	virtual void set_allow_nasty_arguments(bool v) {
		allowNasty_ = v;
	}
	virtual void set_perf_data(bool v) {
		noPerfData_ = !v;
	}
	bool isPasswordOk(std::string remotePassword);
	std::string list_instance(std::string counter);

private:
	bool noPerfData_;
	bool allowNasty_;
	bool allowArgs_;

	socket_helpers::connection_info info_;
	boost::shared_ptr<check_nt::server::server> server_;
	std::string password_;
};