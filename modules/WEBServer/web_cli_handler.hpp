#pragma once

#include "error_handler_interface.hpp"

#include <client/simple_client.hpp>

class web_cli_handler : public client::cli_handler {
	const nscapi::core_wrapper* core;
	boost::shared_ptr<error_handler_interface> error_handler;
	int plugin_id;
public:
	web_cli_handler(boost::shared_ptr<error_handler_interface> error_handler, const nscapi::core_wrapper* core, int plugin_id) 
		: error_handler(error_handler)
		, core(core)
		, plugin_id(plugin_id) 
	{}

	void output_message(const std::string &msg) {
		error_handler_interface::log_entry e;
		e.date = "";
		e.file = "";
		e.line = 0;
		e.message = msg;
		e.type = "out";
		error_handler->add_message(false, e);
	}
	virtual void log_debug(std::string module, std::string file, int line, std::string msg) const {
		if (core->should_log(NSCAPI::log_level::debug)) {
			core->log(NSCAPI::log_level::debug, file, line, msg);
		}
	}

	virtual void log_error(std::string module, std::string file, int line, std::string msg) const {
		if (core->should_log(NSCAPI::log_level::debug)) {
			core->log(NSCAPI::log_level::error, file, line, msg);
		}
	}

	virtual int get_plugin_id() const { return plugin_id; }
	virtual const nscapi::core_wrapper* get_core() const { return core; }
};
