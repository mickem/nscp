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

#include <nrpe/server/protocol.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nrpe/packet.hpp>
#include <nrpe/server/handler.hpp>

class NRPEServer : public nscapi::impl::simple_plugin, nrpe::server::handler {
private:
	unsigned int payload_length_;
	bool noPerfData_;
	bool allowNasty_;
	bool allowArgs_;
	bool multiple_packets_;
	std::string encoding_;

	void set_perf_data(bool v) {
		noPerfData_ = !v;
		if (noPerfData_)
			log_debug("nrpe", __FILE__, __LINE__, "Performance data disabled!");
	}

public:
	NRPEServer();
	virtual ~NRPEServer();
	// Module calls
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	// Handler
	std::list<nrpe::packet> handle(nrpe::packet packet);

	nrpe::packet create_error(std::string msg) {
		return nrpe::packet::create_response(3, msg, payload_length_);
	}

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

private:
	socket_helpers::connection_info info_;
	boost::shared_ptr<nrpe::server::server> server_;
};
