#pragma once

#include "Vcclr.h"

#include <string>

std::string to_nstring(System::String^ s);

class plugin_manager;
class instance_information;
class plugin_instance {
private:
	std::wstring dll;
	std::wstring type;
	int plugin_id;
	gcroot<System::Type^> typeInstance;
	gcroot<NSCP::IPluginFactory^> factory;
	gcroot<NSCP::IPlugin^> plugin;
	gcroot<NSCP::ICore^> core;
	instance_information *info;

public:

	typedef boost::shared_ptr<plugin_instance> plugin_type;

	plugin_instance(plugin_manager *manager, std::wstring dll, std::wstring type, int plutin_id);
	~plugin_instance();

	void create();
	bool load(std::wstring alias, int mode);
	bool unload();

	void set_self(plugin_type self);

	static plugin_type create(plugin_manager* manager, std::wstring factory, std::wstring plugin, std::wstring dll, int plugin_id);

	int onCommand(std::wstring command, std::string request, std::string &response);
	int onSubmit(std::wstring channel, std::string request, std::string &response);

	int get_id() const {
		return plugin_id;
	}
};

class plugin_manager {
public:
// 	virtual bool settings_register_key(std::wstring path, std::wstring key, NSCAPI::settings_type type, std::wstring title, std::wstring description, std::wstring defaultValue, bool advanced) = 0;
// 	virtual bool settings_register_path(std::wstring path, std::wstring title, std::wstring description, bool advanced) = 0;
// 	virtual bool register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description) = 0;
// 	virtual bool register_channel(std::wstring channel, plugin_instance::plugin_type plugin) = 0;

	virtual std::list<std::string> settings_get_list(const std::string path) = 0;
	virtual void settings_reg_path(const std::string path, const std::string title, const std::string desc) = 0;
	virtual void settings_reg_key(const std::string path, const std::string key, const std::string title, const std::string desc) = 0;
	virtual std::string settings_get_string(const std::string path, const std::string key, const std::string value) = 0;
	virtual int settings_get_int(const std::string path, const std::string key, const int value) = 0;
	virtual void registry_reg_command(const std::string command, const std::string description, int plugin_id) = 0;


	virtual nscapi::core_wrapper* get_core() = 0;
};

class instance_information {
public:
	instance_information(plugin_manager *manager) : manager(manager) {}
	plugin_manager *manager;
	plugin_instance::plugin_type self;
	int get_id() const { return self->get_id(); }
};