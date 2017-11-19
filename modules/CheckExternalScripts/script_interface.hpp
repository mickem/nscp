#pragma once

#include "commands.hpp"

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>

#include <boost/shared_ptr.hpp>

#include <string>

struct script_provider_interface {
	virtual unsigned int get_id() = 0;
	virtual nscapi::core_wrapper* get_core() = 0;
	virtual boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy() = 0;

	virtual boost::filesystem::path get_root() = 0;
	virtual std::string generate_wrapped_command(std::string command) = 0;

	virtual void add_command(std::string alias, std::string script) = 0;
	virtual commands::command_object_instance find_command(std::string alias) = 0;
	virtual void remove_command(std::string alias) = 0;
	virtual std::list<std::string> get_commands() = 0;
};
