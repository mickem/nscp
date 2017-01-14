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
