#include "stdafx.h"

#include <list>

#include <utf8.hpp>

#include "plugin_instance.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_plugin_interface.hpp>

using namespace System;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

std::string to_nstring(System::String^ s) {
	pin_ptr<const wchar_t> pinString = PtrToStringChars(s);
	return utf8::cvt<std::string>(std::wstring(pinString));
}

typedef cli::array<Byte> protobuf_data;

std::string to_nstring(protobuf_data^ request) {
	char *buffer = new char[request->Length+1];
	memset(buffer, 0, request->Length+1);
	Marshal::Copy(request, 0, IntPtr(buffer), (int)request->Length);
	std::string ret(buffer, (std::string::size_type)request->Length);
	delete [] buffer;
	return ret;
}

protobuf_data^ to_pbd(std::string &buffer) {
	protobuf_data^ arr = gcnew protobuf_data(buffer.size());
	Marshal::Copy(IntPtr(const_cast<char*>(buffer.c_str())), arr, 0, arr->Length);
	return arr;
}

System::String^ to_mstring(const std::string &s) {
	return gcnew System::String(utf8::cvt<std::wstring>(s).c_str());
}
System::String^ to_mstring(const std::wstring &s) {
	return gcnew System::String(s.c_str());
}

std::list<std::wstring> to_nlist(List<String^>^ list) {
	std::list<std::wstring> ret;
	// TODO: Implement this!
	return ret;
};
array<String^>^ to_mlist(std::list<std::string> list) {
	array<System::String^>^ ret = gcnew array<System::String^>(list.size());
	typedef std::list<std::string>::const_iterator iter_t;
	int j = 0;

	for (iter_t i = list.begin(); i != list.end(); ++i)
		ret[j++] = gcnew System::String(utf8::cvt<std::wstring>(*i).c_str());
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
		get_manager()->registry_reg_command(to_nstring(command), to_nstring(description), get_info()->get_id());
		return true;
	}
	virtual bool subscribeChannel(String^ channel) {
		return false; //get_manager()->register_channel(to_nstring(channel), get_self());
	}
};

ref class SettingsImpl : public NSCP::ISettings, public BasicPlugin {
public:
	SettingsImpl(instance_information *info) : BasicPlugin(info) {}

	virtual array<String^>^ getSection(String^ path) 
	{
		return to_mlist(get_manager()->settings_get_list(to_nstring(path)));
	}

	virtual String^ getString(String^ path, String ^key, String ^defaultValue) 
	{
		return to_mstring(get_manager()->settings_get_string(to_nstring(path), to_nstring(key), to_nstring(defaultValue)));
	}

	virtual void setString(String^ path, String ^key, String ^theValue) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual bool getBool(String^ path, String ^key, bool defaultValue) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual void setBool(String^ path, String ^key, bool theValue) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual int getInt(String^ path, String ^key, int defaultValue) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual void setInt(String^ path, String ^key, int theValue) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual bool save() 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual bool registerPath(String^ path, String^ title, String^ description, bool advanced) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

	virtual bool register_key(String^ path, String^ key, int type, String^ title, String^ description, String^ defaultValue, bool advanced) 
	{
		throw gcnew System::Exception("The method or operation is not implemented.");
	}

};

ref class CoreImpl : public NSCP::ICore, public BasicPlugin {
public:
	CoreImpl(instance_information *info) : BasicPlugin(info) {}

	virtual NSCP::Result^ query(protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->query(to_nstring(request), response);
		ret->data = to_pbd(response);
		return ret;
	}
	virtual NSCP::Result^ exec(String^ target, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->exec_command(to_nstring(target), to_nstring(request), response);
		ret->data = to_pbd(response);
		return ret;
	}
	virtual NSCP::Result^ submit(String^ channel, protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->submit_message(to_nstring(channel), to_nstring(request), response);
		ret->data = to_pbd(response);
		return ret;
	}
	virtual bool reload(String^ module) {
		return get_core()->reload(to_nstring(module)) == NSCAPI::isSuccess;
	}
	virtual NSCP::Result^ settings(protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->settings_query(to_nstring(request), response);
		ret->data = to_pbd(response);
		return ret;
	}
	virtual NSCP::Result^ registry(protobuf_data^ request) {
		NSCP::Result^ ret= gcnew NSCP::Result();
		std::string response;
		ret->result = get_core()->registry_query(to_nstring(request), response);
		ret->data = to_pbd(response);
		return ret;
	}
	virtual void log(protobuf_data^ request) {
		// 		get_core()->log(to_nstring(request));
	}
	virtual NSCP::ILogger^ getLogger() {
		return gcnew LoggerImpl();
	}
	virtual NSCP::IRegistry^ getRegistry() {
		return gcnew RegistryImpl(get_info());
	}
	virtual NSCP::ISettings^ getSettings() {
		return gcnew SettingsImpl(get_info());
	}
};

//////////////////////////////////////////////////////////////////////////

plugin_instance::plugin_instance(plugin_manager *manager, std::wstring dll, std::wstring type, int plugin_id)
	: info(new instance_information(manager))
	, dll(dll)
	, type(type)
	, plugin_id(plugin_id)
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

plugin_instance::plugin_type plugin_instance::create(plugin_manager *manager, std::wstring factory, std::wstring plugin, std::wstring dll, int plugin_id) {
	//NSC_DEBUG_MSG("Using factory: " + factory + _T(" for ") + plugin);
	NSC_DEBUG_MSG("About to load dotnet plugin: " + utf8::cvt<std::string>(dll));

	plugin_type instance = plugin_type(new plugin_instance(manager, dll, factory, plugin_id));
	if (!instance) {
		NSC_LOG_ERROR_STD("Failed to load plugin factory from " + utf8::cvt<std::string>(dll));
		return instance;
	}
	instance->set_self(instance);
	NSC_DEBUG_MSG("Plugin loaded: " + utf8::cvt<std::string>(plugin));
	instance->create();
	return instance;
}

void plugin_instance::create() {
	try {
		System::Reflection::Assembly^ dllAssembly = System::Reflection::Assembly::LoadFrom(to_mstring(dll));
		typeInstance = dllAssembly->GetType(to_mstring(type));
		if (!typeInstance) {
			NSC_LOG_ERROR_STD("Failed to load plugin factory from " + utf8::cvt<std::string>(dll));
			factory = nullptr;
			plugin = nullptr;
			return;
		}
		factory = (NSCP::IPluginFactory^)Activator::CreateInstance(typeInstance);
	} catch(System::Exception ^e) {
		NSC_LOG_ERROR_STD("Failed to create instance of " + utf8::cvt<std::string>(dll) + ": " + to_nstring(e->ToString()));
	}
}

bool plugin_instance::load(std::wstring alias, int mode) {
	if (!factory) {
		NSC_LOG_ERROR_STD("No factory for: " + utf8::cvt<std::string>(dll));
		return false;
	}
	plugin = factory->create(core, plugin_id, to_mstring(alias));
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
	NSCP::Result^ result = handler->onQuery(to_mstring(command), to_pbd(request));
	response = to_nstring(result->data);
	return result->result;
}

int plugin_instance::onSubmit(std::wstring channel, std::string request, std::string &response) {
	NSCP::ISubmissionHandler ^handler = plugin->getSubmissionHandler();
	if (!handler->isActive())
		return NSCAPI::returnIgnored;
	NSCP::Result^ result = handler->onSubmission(to_mstring(channel), to_pbd(request));
	response = to_nstring(result->data);
	return result->result;
}