#pragma once

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
	virtual boost::optional<boost::filesystem::path> find_file(std::string file) = 0;

	virtual void add_command(std::string script_alias, std::string script, std::string plugin_alias) = 0;
	//virtual commands::command_object_instance find_command(std::string alias) = 0;
	virtual void remove_command(std::string alias) = 0;
	virtual void clear() = 0;
};
