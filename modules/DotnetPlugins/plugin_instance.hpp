#pragma once

#include "Vcclr.h"

#include <string>

std::wstring to_nstring(System::String^ s);

class plugin_manager;
class instance_information;
class plugin_instance {
private:
	std::wstring dll;
	std::wstring type;
	gcroot<System::Type^> typeInstance;
	gcroot<NSCP::IPluginFactory^> factory;
	gcroot<NSCP::IPlugin^> plugin;
	gcroot<NSCP::ICore^> core;
	instance_information *info;

public:

	typedef boost::shared_ptr<plugin_instance> plugin_type;

	plugin_instance(plugin_manager *manager, std::wstring dll, std::wstring type);
	~plugin_instance();

	void create();
	bool load(std::wstring alias, int mode);
	bool unload();

	void set_self(plugin_type self);

	static plugin_type create(plugin_manager* manager, std::wstring factory, std::wstring plugin, std::wstring dll);

	int onCommand(std::wstring command, std::string request, std::string &response);
	int onSubmit(std::wstring channel, std::string request, std::string &response);

};

class plugin_manager {
public:
	virtual bool register_command(std::wstring command, plugin_instance::plugin_type plugin, std::wstring description) = 0;
	virtual bool register_channel(std::wstring channel, plugin_instance::plugin_type plugin) = 0;
	virtual nscapi::core_wrapper* get_core() = 0;
};

class instance_information {
public:
	instance_information(plugin_manager *manager) : manager(manager) {}
	plugin_manager *manager;
	plugin_instance::plugin_type self;
};