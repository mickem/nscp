#include "stdafx.h"

#include <list>

#include "plugin_instance.hpp"

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

std::wstring to_nstring(System::String^ s) {
	pin_ptr<const wchar_t> pinString = PtrToStringChars(s);
	return std::wstring(pinString);
}

typedef cli::array<Byte> protobuf_data;


std::string to_nstring(protobuf_data^  request) {
	char *buffer = new char[request->Length+1];
	memset(buffer, 0, request->Length+1);
	Marshal::Copy(request, 0, IntPtr(buffer), (int)request->Length);
	std::string ret(buffer, (std::string::size_type)request->Length);
	delete [] buffer;
	return ret;
}
/*
std::string to_nstring(cli::array<unsigned char>^ buffer) {
	cli::pin_ptr<unsigned char> native_buffer = &buffer[0];
	return std::string(native_buffer, buffer->Length);
}
*/
protobuf_data^ to_mstring(std::string &buffer) {
	protobuf_data^ arr = gcnew protobuf_data(buffer.size());
	Marshal::Copy(IntPtr(const_cast<char*>(buffer.c_str())), arr, 0, arr->Length);
	return arr;
}

System::String^ to_mstring(const std::wstring &s) {
	return gcnew System::String(s.c_str());
}

std::list<std::wstring> to_nlist(List<String^>^ list) {
	std::list<std::wstring> ret;
	// TODO: Implement this!
	return ret;
};
array<String^>^ to_mlist(std::list<std::wstring> list) {
	array<System::String^>^ ret = gcnew array<System::String^>(list.size());
	typedef std::list<std::wstring>::const_iterator iter_t;
	int j = 0;

	for (iter_t i = list.begin(); i != list.end(); ++i)
		ret[j++] = gcnew System::String(i->c_str());
	return ret;
};

//////////////////////////////////////////////////////////////////////////


ref class LoggerImpl : public NSCP::ILogger {
public:
	virtual void debug(String^ message) {
		NSC_DEBUG_MSG_STD(to_nstring(message));
	}
	virtual void info(String^ message) {
		NSC_LOG_MESSAGE(to_nstring(message));
	}
	virtual void warn(String^ message) {
		NSC_LOG_ERROR(to_nstring(message));
	}
	virtual void error(String^ message) {
		NSC_LOG_ERROR(to_nstring(message));
	}
	virtual void fatal(String^ message) {
		NSC_LOG_CRITICAL(to_nstring(message));
	}
};

ref class BasicPlugin {
	instance_information *info;
public:

	BasicPlugin(instance_information *info) : info(info) {}
	void set_info(instance_information *info) {
		this->info = info;
	}

	nscapi::core_wrapper* get_core() {
		if (info == NULL)
			throw gcnew System::Exception("Uninitialized core");
		if (info->manager == NULL)
			throw gcnew System::Exception("Uninitialized core");
		return info->manager->get_core();
	}
	plugin_manager* get_manager() {
		if (info == NULL)
			throw gcnew System::Exception("Uninitialized core");
		return info->manager;
	}
	plugin_instance::plugin_type get_self() {
		if (info == NULL)
			throw gcnew System::Exception("Uninitialized core");
		return info->self;
	}
	instance_information* get_info() {
		if (info == NULL)
			throw gcnew System::Exception("Uninitialized core");
		return info;
	}

};

ref class RegistryImpl : public NSCP::IRegistry, public BasicPlugin {
public:
	RegistryImpl(instance_information *info) : BasicPlugin(info) {}
	virtual bool registerCommand(String^ command, String^ description) {
		return get_manager()->register_command(to_nstring(command), get_self(), to_nstring(description));
	}
	virtual bool subscribeChannel(String^ channel) {
		return get_manager()->register_channel(to_nstring(channel), get_self());
	}
};

ref class SettingsImpl : public NSCP::ISettings, public BasicPlugin {
public:
	SettingsImpl(instance_information *info) : BasicPlugin(info) {}

	virtual array<String^>^ getSection(String^ path) {
		return to_mlist(get_core()->getSettingsSection(to_nstring(path)));
	}
	virtual String^ getString(String^ path, String ^key, String ^defaultValue) {
		return to_mstring(get_core()->getSettingsString(to_nstring(path), to_nstring(key), to_nstring(defaultValue)));
	}
	virtual void setString(String^ path, String ^key, String ^theValue) {
		get_core()->SetSettingsString(to_nstring(path), to_nstring(key), to_nstring(theValue));
	}
	virtual bool getBool(String^ path, String ^key, bool defaultValue) {
		return get_core()->getSettingsBool(to_nstring(path), to_nstring(key), defaultValue);
	}
	virtual void setBool(String^ path, String ^key, bool theValue) {
		get_core()->SetSettingsInt(to_nstring(path), to_nstring(key), theValue?1:0);
	}
	virtual int getInt(String^ path, String ^key, int defaultValue) {
		return get_core()->getSettingsInt(to_nstring(path), to_nstring(key), defaultValue);
	}
	virtual void setInt(String^ path, String ^key, int theValue) {
		get_core()->SetSettingsInt(to_nstring(path), to_nstring(key), theValue);
	}
	virtual bool save() {
		get_core()->settings_save();
		return true;
	}
	virtual bool registerPath(String^ path, String^ title, String^ description, bool advanced) {
		get_manager()->settings_register_path(to_nstring(path), to_nstring(title), to_nstring(description), advanced);
		return true;
	}
	virtual bool register_key(String^ path, String^ key, int type, String^ title, String^ description, String^ defaultValue, bool advanced) {
		get_manager()->settings_register_key(to_nstring(path), to_nstring(key), type, to_nstring(title), to_nstring(description), to_nstring(defaultValue), advanced);
		return true;
	}

};

ref class CoreImpl : public NSCP::ICore, public BasicPlugin {
public:
	CoreImpl(instance_information *info) : BasicPlugin(info) {}

	virtual NSCP::Result^ query(String^ command, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->query(to_nstring(command), to_nstring(request), response);
		ret->data = to_mstring(response);
		return ret;
	}
	virtual NSCP::Result^ exec(String^ target, String^ command, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->exec_command(to_nstring(target), to_nstring(command), utf8::cvt<std::string>(to_nstring(request)), response);
		ret->data = to_mstring(response);
		return ret;
	}
	virtual NSCP::Result^ exec(String^ command, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->exec_command(_T("*"), to_nstring(command), utf8::cvt<std::string>(to_nstring(request)), response);
		ret->data = to_mstring(response);
		return ret;
	}
	virtual NSCP::Result^ submit(String^ channel, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->submit_message(to_nstring(channel), utf8::cvt<std::string>(to_nstring(request)), response);
		ret->data = to_mstring(response);
		return ret;
	}
	virtual bool reload(String^ module) {
		return get_core()->reload(to_nstring(module)) == NSCAPI::isSuccess;
	}

	virtual NSCP::ISettings^ getSettings() {
		return gcnew SettingsImpl(get_info());
	}
	virtual NSCP::IRegistry^ getRegistry() {
		return gcnew RegistryImpl(get_info());
	}
	virtual NSCP::ILogger^ getLogger() {
		return gcnew LoggerImpl();
	}
};

//////////////////////////////////////////////////////////////////////////


plugin_instance::plugin_instance(plugin_manager *manager, std::wstring dll, std::wstring type) 
: info(new instance_information(manager))
, dll(dll)
, type(type)
{
	core = gcnew CoreImpl(info);
}
plugin_instance::~plugin_instance()
{
	delete info;
}

void plugin_instance::set_self(plugin_type self) {
	info->self = self;
}

plugin_instance::plugin_type plugin_instance::create(plugin_manager *manager, std::wstring factory, std::wstring plugin, std::wstring dll) {
	NSC_DEBUG_MSG(_T("Using factory: ") + factory + _T(" for ") + plugin);
	NSC_DEBUG_MSG(_T("About to load dotnet plugin: ") + dll);

	plugin_type instance = plugin_type(new plugin_instance(manager, dll, factory));
	if (!instance) {
		NSC_LOG_ERROR_STD(_T("Failed to load plugin '") + factory + _T("' from ") + dll);
		return instance;

	}
	instance->set_self(instance);
	NSC_DEBUG_MSG(_T("Plugin loaded: ") + plugin);
	instance->create();
	return instance;
}

void plugin_instance::create() {	
	try {
		System::Reflection::Assembly^ dllAssembly = System::Reflection::Assembly::LoadFrom(to_mstring(dll));
		typeInstance = dllAssembly->GetType(to_mstring(type));
		if (!typeInstance) {
			NSC_LOG_ERROR_STD(_T("Failed to load factory '") + type + _T("' from ") + dll);
			factory = nullptr;
			plugin = nullptr;
			return;
		}
		factory = (NSCP::IPluginFactory^)Activator::CreateInstance(typeInstance);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD(_T("Failed to create instance of '") + dll + _T(": ") + to_nstring(e->ToString()));
	}
}

bool plugin_instance::load(std::wstring alias, int mode) {
	if (!factory) {
		NSC_LOG_ERROR_STD(_T("No factory for: ") + dll);
		return false;
	}
	plugin = factory->create(core, to_mstring(alias));
	plugin->load(mode);
	return true;
}

bool plugin_instance::unload() {
	plugin->unload();
	return true;
}

int plugin_instance::onCommand(std::wstring command, std::string request, std::string &response) {
	NSCP::IQueryHandler^ handler = plugin->getQueryHandler();
	if (!handler->isActive())
		return NSCAPI::returnIgnored;
	NSCP::Result^ result = handler->onQuery(to_mstring(command), to_mstring(request));
	response = to_nstring(result->data);
	return result->result;
}

int plugin_instance::onSubmit(std::wstring channel, std::string request, std::string &response) {
	NSCP::ISubmissionHandler ^handler = plugin->getSubmissionHandler();
	if (!handler->isActive())
		return NSCAPI::returnIgnored;
	NSCP::Result^ result = handler->onSubmission(to_mstring(channel), to_mstring(request));
	response = to_nstring(result->data);
	return result->result;
}


