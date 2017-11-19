#pragma once

#include "script_interface.hpp"

#include <boost/thread/shared_mutex.hpp>

#include <map>
#include <string>

struct script_provider : public script_provider_interface {

private:
	int id_;
	nscapi::core_wrapper *core_;
	boost::filesystem::path root_;
	std::map<std::string, std::string> wrappings_;
	commands::command_handler commands_;
	boost::shared_mutex mutex_;

public:
	script_provider(int id, nscapi::core_wrapper *core, std::string settings_path, boost::filesystem::path root, std::map<std::string, std::string> wrappings);

	unsigned int get_id();
	nscapi::core_wrapper* get_core();
	boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

	boost::filesystem::path get_root();
	std::string generate_wrapped_command(std::string command);

	void setup_commands();
	void add_command(std::string alias, std::string script);
	commands::command_object_instance find_command(std::string alias);
	void remove_command(std::string alias);
	std::list<std::string> get_commands();

};
