#pragma once

#include "script_interface.hpp"
#include "python_script.hpp"

#include <boost/thread/shared_mutex.hpp>

#include <map>
#include <string>

struct script_provider : public script_provider_interface {

private:
	int id_;
	nscapi::core_wrapper *core_;
	boost::filesystem::path root_;
	boost::shared_mutex mutex_;

	typedef std::list<boost::shared_ptr<python_script> > instance_list_type;
	instance_list_type instances_;

public:
	script_provider(int id, nscapi::core_wrapper *core, std::string settings_path, boost::filesystem::path root);

	unsigned int get_id();
	nscapi::core_wrapper* get_core();
	boost::shared_ptr<nscapi::settings_proxy> get_settings_proxy();

	boost::filesystem::path get_root();
	boost::optional<boost::filesystem::path> find_file(std::string file);

	void add_command(std::string script_alias, std::string script, std::string plugin_alias);
	//commands::command_object_instance find_command(std::string alias);
	void remove_command(std::string alias);
	void clear();

};
