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


#include <nsca/server/protocol.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

class NSCAServer : public nscapi::impl::simple_plugin, nsca::server::handler {
private:
	nscapi::core_wrapper* core_;
	unsigned int payload_length_;
	int plugin_id_;
	bool noPerfData_;
	bool allowNasty_;
	bool allowArgs_;

	std::string channel_;
	int encryption_;
	std::string password_;

	void set_encryption(std::string enc) {
		encryption_ = nscp::encryption::helpers::encryption_to_int(enc);
	}
	void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug("nsca", __FILE__, __LINE__, "Performance data disabled!");
	}

public:
	NSCAServer() {}
	virtual ~NSCAServer() {}
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	// handler
	void handle(nsca::packet packet);
	void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (get_core()->should_log(NSCAPI::log_level::debug)) {
			get_core()->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}
	void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (get_core()->should_log(NSCAPI::log_level::error)) {
			get_core()->log(NSCAPI::log_level::error, file, line, msg);
		}
	}
	unsigned int get_payload_length() {
		return payload_length_;
	}
	int get_encryption() {
		return encryption_;
	}
	std::string get_password() {
		return password_;
	}

private:
	socket_helpers::connection_info info_;
	boost::shared_ptr<nsca::server::server> server_;
};
