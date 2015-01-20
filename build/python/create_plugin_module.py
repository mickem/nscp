import json
import json
import string
from optparse import OptionParser
from jinja2 import Template, Environment
import hashlib
import sys
import os
DEF_TEMPLATE = """
LIBRARY	{{module.name}}

EXPORTS
	NSModuleHelperInit
	NSLoadModuleEx
	NSUnloadModule
	NSGetModuleName
	NSGetModuleDescription
	NSGetModuleVersion
	NSHasCommandHandler
	NSHasMessageHandler
	NSHandleMessage
	NSHandleCommand
	NSDeleteBuffer
{% if module.channels %}
	NSHasNotificationHandler
	NSHandleNotification
{% endif %}
{% if module.cli %}
	NSCommandLineExec
{% endif %}
"""

RC_TEMPLATE = """
#include <version.hpp>

/////////////////////////////////////////////////////////////////////// 
// 
// Version
// 

1 VERSIONINFO
 FILEVERSION PRODUCTVER
 PRODUCTVERSION PRODUCTVER
 FILEFLAGSMASK 0x3fL
#ifdef _DEBUG
 FILEFLAGS 0x1L
#else
 FILEFLAGS 0x0L
#endif
 FILEOS 0x40004L
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
            VALUE "CompanyName", "MySolutions Nordic (Michael Medin)\\0"
            VALUE "FileDescription", "{{module.description|rcstring}}\\0"
            VALUE "FileVersion", STRPRODUCTVER "\\0"
            VALUE "InternalName", "{{module.name}}\\0"
            VALUE "LegalCopyright", "Copyright (C) 2014 - Michael Medin\\0"
            VALUE "OriginalFilename", "{{module.name|upper}}.DLL\\0"
            VALUE "ProductName", "NSClient++ Module: {{module.name}}\\0"
            VALUE "ProductVersion", STRPRODUCTVER "\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
"""

CPP_TEMPLATE = """#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>

#include "module.hpp"
#include <nscapi/command_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

namespace ch = nscapi::command_helper;

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool {{module.name}}Module::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {
		if (mode == NSCAPI::reloadStart) {
{% if module.reload %}
			return impl_->loadModuleEx(alias, mode);
{% else %}
{% if module.loaders == "both" or module.loaders == "unload" %}
			if (impl_) {
				impl_->unloadModule();
			}
{% endif %}
			mode = NSCAPI::normalStart;
{% endif %}
		} else {
			if (impl_) {
				unloadModule();
			}
			impl_.reset(new {{module.name}});
			impl_->set_id(get_id());
			registerCommands(get_command_proxy());
		}
{% if module.loaders == "both" or module.loaders == "load" %}
		return impl_->loadModuleEx(alias, mode);
{% else %}
		return true;
{% endif %}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to load {{module.name}}: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to load {{module.name}}: ");
		return false;
	}
}

bool {{module.name}}Module::unloadModule() {
	bool ret = false;
	if (impl_) {
{% if module.loaders == "both" or module.loaders == "unload" %}
		ret = impl_->unloadModule();
{% else %}
		ret = true;
{% endif %}
	}
	impl_.reset();
	return ret;
}

{% if module.commands or module.command_fallback%}
/**
 * Main command parser and delegator.
 *
 * @param char_command The command name (string)
 * @param request The request packet
 * @param response The response packet
 * @return status code
 */
NSCAPI::nagiosReturn {{module.name}}Module::handleRAWCommand(const std::string &request, std::string &response) {
	try {
		Plugin::QueryRequestMessage request_message;
		Plugin::QueryResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());

		if (!impl_) {
			return NSCAPI::returnIgnored;
		}
		for (int i=0;i<request_message.payload_size();i++) {
			Plugin::QueryRequestMessage::Request request_payload = request_message.payload(i);
			if (!impl_) {
				return NSCAPI::returnIgnored;
{% for cmd in module.commands %}
{% set cmd_name = cmd.name +"_" if cmd.name == module.name else cmd.name %}
{% if cmd.no_mapping %}
{% elif cmd.raw_mapping %}
			} else if (request_payload.command() == "{{cmd.name|lower}}") {
				impl_->{{cmd_name}}("{{cmd.name|lower}}", request_message, &response_message);
				response_message.SerializeToString(&response);
				return NSCAPI::isSuccess;
{% elif cmd.nagios %}
			} else if (request_payload.command() == "{{cmd.name|lower}}") {
				std::string msg, perf;
				std::list<std::string> args;
				for (int i=0;i<request_payload.arguments_size();i++) {
					args.push_back(request_payload.arguments(i));
				}
				NSCAPI::nagiosReturn ret = impl_->{{cmd_name}}(request_payload.target(), boost::algorithm::to_lower_copy(request_payload.command()), args, msg, perf);
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				response_payload->set_message(msg);
				response_payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
				if (!perf.empty())
					nscapi::protobuf::functions::parse_performance_data(response_payload, perf);
{% elif cmd.request %}
			} else if (request_payload.command() == "{{cmd.name|lower}}") {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->{{cmd_name}}(request_payload, response_payload, request_message);
{% elif cmd.legacy %}
			} else if (request_payload.command() == "{{cmd.name|lower}}") {
				std::string msg, perf;
				std::list<std::string> args;
				for (int i=0;i<request_payload.arguments_size();i++) {
					args.push_back(request_payload.arguments(i));
				}
				NSCAPI::nagiosReturn ret = impl_->{{cmd_name}}(request_payload.target(), boost::algorithm::to_lower_copy(request_payload.command()), args, msg, perf);
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				response_payload->set_message(msg);
				response_payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
				if (!perf.empty())
					nscapi::protobuf::functions::parse_performance_data(response_payload, perf);
{% else %}
			} else if (request_payload.command() == "{{cmd.name|lower}}") {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->{{cmd_name}}(request_payload, response_payload);
{% endif %}
{% endfor %}
{% if module.command_fallback %}
			} else {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->query_fallback(request_payload, response_payload, request_message);
{% endif %}
			}
		}
		response_message.SerializeToString(&response);
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::create_simple_query_response_unknown("", std::string("Failed to process command : ") + e.what(), response);
		return NSCAPI::isSuccess;
	} catch (...) {
		nscapi::protobuf::functions::create_simple_query_response_unknown("", "Failed to process command", response);
		return NSCAPI::isSuccess;
	}
}

void {{module.name}}Module::registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy) {
	ch::command_registry registry(proxy);
	registry.command()
{% for cmd in module.commands %}
{% if cmd.alias and cmd.alias|length == 1 %}
		("{{cmd.name}}", "{{cmd.alias[0]}}",
		"{{cmd.description}}")
{% else %}
		("{{cmd.name}}",
		"{{cmd.description}}")
{% endif %}
{% endfor %}
		;
/*
	registry.add_metadata(_T("check_cpu"))
		(_T("guide"), _T("http://nsclient.org/nscp/wiki/doc/usage/nagios/nsca"))
		;
*/
	registry.register_all();
}
{% else %}
void {{module.name}}Module::registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy) {}
{% endif %}

{%if module.log_handler %}
void {{module.name}}Module::handleMessageRAW(std::string data) {
	try {
		Plugin::LogEntry message;
		message.ParseFromString(data);
		if (!impl_) {
			return;
		} else {
			for (int i=0;i<message.entry_size();i++) {
				impl_->handleLogMessage(message.entry(i));
			}
		}
	} catch (std::exception &e) {
		// Ignored since loggers cant log
	} catch (...) {
		// Ignored since loggers cant log
	}
}
{% endif %}

{% if module.channels == "raw" %}
NSCAPI::nagiosReturn {{module.name}}Module::handleRAWNotification(const char* char_channel, const std::string &request, std::string &response) {
	const std::string channel = char_channel;
	try {
		if (!impl_) {
			return NSCAPI::returnIgnored;
		}
		Plugin::SubmitRequestMessage request_message;
		Plugin::SubmitResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());
		impl_->handleNotification(channel, request_message, &response_message);
		response_message.SerializeToString(&response);
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::create_simple_submit_response(channel, "", NSCAPI::returnUNKNOWN, std::string("Failed to process submission on ") + channel + ": " + e.what(), response);
		return NSCAPI::isSuccess;
	} catch (...) {
		nscapi::protobuf::functions::create_simple_submit_response(channel, "", NSCAPI::returnUNKNOWN, "Failed to process submission on: " + channel, response);
		return NSCAPI::isSuccess;
	}
}
{% elif module.channels %}
NSCAPI::nagiosReturn {{module.name}}Module::handleRAWNotification(const char* char_channel, const std::string &request, std::string &response) {
	const std::string channel = char_channel;
	try {
		Plugin::SubmitRequestMessage request_message;
		Plugin::SubmitResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());

		for (int i=0;i<request_message.payload_size();i++) {
			Plugin::QueryResponseMessage::Response request_payload = request_message.payload(i);
			if (!impl_) {
				return NSCAPI::returnIgnored;
			} else {
				Plugin::SubmitResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->handleNotification(channel, request_payload, response_payload, request_message);
			}
		}
		response_message.SerializeToString(&response);
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::create_simple_submit_response(channel, "", NSCAPI::returnUNKNOWN, std::string("Failed to process submission on ") + channel + ": " + e.what(), response);
		return NSCAPI::isSuccess;
	} catch (...) {
		nscapi::protobuf::functions::create_simple_submit_response(channel, "", NSCAPI::returnUNKNOWN, "Failed to process submission on: " + channel, response);
		return NSCAPI::isSuccess;
	}
}
{% endif %}

{%if module.cli == "legacy" %}
NSCAPI::nagiosReturn {{module.name}}Module::commandRAWLineExec(const std::string &request, std::string &response) {
	try {
		Plugin::ExecuteRequestMessage request_message;
		Plugin::ExecuteResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());

		bool found = false;
		for (int i=0;i<request_message.payload_size();i++) {
			const Plugin::ExecuteRequestMessage::Request &request_payload = request_message.payload(i);
			if (!impl_) {
				nscapi::protobuf::functions::create_simple_exec_response_unknown("", std::string("Internal error"), response);
				return NSCAPI::isSuccess;
			} else {
				Plugin::ExecuteResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				std::string output;
				std::list<std::string> args;
				for (int j=0;j<request_payload.arguments_size();++j)
					args.push_back(request_payload.arguments(j));
				int ret = impl_->commandLineExec(request_payload.command(), args, output);
				if (ret != NSCAPI::returnIgnored) {
					found = true;
					response_payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
					response_payload->set_message(output);
				}
			}
		}
		if (found) {
			response_message.SerializeToString(&response);
			return NSCAPI::isSuccess;
		}
		return NSCAPI::returnIgnored;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::create_simple_exec_response_unknown("", std::string("Failed to process command: ") + utf8::utf8_from_native(e.what()), response);
		return NSCAPI::isSuccess;
	} catch (...) {
		nscapi::protobuf::functions::create_simple_exec_response_unknown("", "Failed to process command", response);
		return NSCAPI::isSuccess;
	}
}
{% elif module.cli == "pass-through" %}
NSCAPI::nagiosReturn {{module.name}}Module::commandRAWLineExec(const std::string &request, std::string &response) {
	return impl_->commandLineExec(request, response);
}
{% elif module.cli %}
NSCAPI::nagiosReturn {{module.name}}Module::commandRAWLineExec(const std::string &request, std::string &response) {
	try {
		Plugin::ExecuteRequestMessage request_message;
		Plugin::ExecuteResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());

		bool found = false;
		for (int i=0;i<request_message.payload_size();i++) {
			Plugin::ExecuteRequestMessage::Request request_payload = request_message.payload(i);
			if (!impl_) {
				return NSCAPI::returnIgnored;
			} else {
				Plugin::ExecuteResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				if (!impl_->commandLineExec(request_payload, response_payload, request_message)) {
					// TODO: remove payloads here!
				} else {
					found = true;
				}
			}
		}
		if (found) {
			response_message.SerializeToString(&response);
			return NSCAPI::isSuccess;
		}
		return NSCAPI::returnIgnored;
	} catch (const std::exception &e) {
		nscapi::protobuf::functions::create_simple_exec_response_unknown("", std::string("Failed to process command: ") + utf8::utf8_from_native(e.what()), response);
		return NSCAPI::isSuccess;
	} catch (...) {
		nscapi::protobuf::functions::create_simple_exec_response_unknown("", "Failed to process command", response);
		return NSCAPI::isSuccess;
	}
}
{% endif %}

#ifdef _WIN32
	BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }
#endif
	nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();
	typedef {{module.name}}Module plugin_impl_class;
	static nscapi::plugin_instance_data<plugin_impl_class> plugin_instance;
	extern int NSModuleHelperInit(unsigned int, nscapi::core_api::lpNSAPILoader f) {
		return nscapi::basic_wrapper_static<plugin_impl_class>::NSModuleHelperInit(f); 
	}
	extern int NSLoadModuleEx(unsigned int id, char* alias, int mode) {
		if (mode == NSCAPI::normalStart || mode == NSCAPI::dontStart) {
			nscapi::basic_wrapper_static<plugin_impl_class>::set_alias("{{module.alias}}", alias);
		}
		nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
		return wrapper.NSLoadModuleEx(id, alias, mode); 
	}
	extern int NSLoadModule() {
		return nscapi::basic_wrapper_static<plugin_impl_class>::NSLoadModule();
	}
	extern int NSGetModuleName(char* buf, int buflen) {
		return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleName(buf, buflen);
	}
	extern int NSGetModuleDescription(char* buf, int buflen) {
		return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleDescription(buf, buflen);
	}
	extern int NSGetModuleVersion(int *major, int *minor, int *revision) {
		return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleVersion(major, minor, revision);
	}
	extern int NSUnloadModule(unsigned int id) {
		int ret;
		{
			nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
			ret = wrapper.NSUnloadModule();
		}
		plugin_instance.erase(id);
		return ret;
	}
	extern void NSDeleteBuffer(char**buffer) {
		nscapi::basic_wrapper_static<plugin_impl_class>::NSDeleteBuffer(buffer); 
	}
{%if module.log_handler %}
extern void NSHandleMessage(unsigned int id, const char* request_buffer, unsigned int request_buffer_len) {
	nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleMessage(request_buffer, request_buffer_len);
}
extern NSCAPI::boolReturn NSHasMessageHandler(unsigned int id) {
	nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasMessageHandler();
}
{% else %}
extern void NSHandleMessage(unsigned int, const char*, unsigned int) {}
extern NSCAPI::boolReturn NSHasMessageHandler(unsigned int) { return NSCAPI::isfalse; }
{% endif %}
{% if module.commands or module.command_fallback%}
extern NSCAPI::nagiosReturn NSHandleCommand(unsigned int id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) {
	nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleCommand(request_buffer, request_buffer_len, reply_buffer, reply_buffer_len); 
}
extern NSCAPI::boolReturn NSHasCommandHandler(unsigned int id) {
	nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasCommandHandler(); 
}
{% else %}
extern NSCAPI::nagiosReturn NSHandleCommand(unsigned int, const char*, const unsigned int, char**, unsigned int*) {  return NSCAPI::returnIgnored; }
extern NSCAPI::boolReturn NSHasCommandHandler(unsigned int) { return NSCAPI::isfalse; }
{% endif %}
{% if module.cli %}
extern int NSCommandLineExec(unsigned int id, char *request_buffer, unsigned int request_len, char **response_buffer, unsigned int *response_len) {
	nscapi::cliexec_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSCommandLineExec(request_buffer, request_len, response_buffer, response_len); 
}
{% endif %}
{% if module.channels %}
extern int NSHandleNotification(unsigned int id, const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len) {
	nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHandleNotification(channel, buffer, buffer_len, response_buffer, response_buffer_len); 
}
extern NSCAPI::boolReturn NSHasNotificationHandler(unsigned int id) {
	nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id));
	return wrapper.NSHasNotificationHandler(); 
}
{% endif %}
"""

HPP_TEMPLATE = """#pragma once
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_plugin_wrapper.hpp>

extern "C" int NSModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f);
extern "C" int NSLoadModule();
extern "C" int NSLoadModuleEx(unsigned int plugin_id, char* alias, int mode);
extern "C" void NSDeleteBuffer(char**buffer);
extern "C" int NSGetModuleName(char* buf, int buflen);
extern "C" int NSGetModuleDescription(char* buf, int buflen);
extern "C" int NSGetModuleVersion(int *major, int *minor, int *revision);
extern "C" NSCAPI::boolReturn NSHasCommandHandler(unsigned int plugin_id);
extern "C" NSCAPI::boolReturn NSHasMessageHandler(unsigned int plugin_id);
extern "C" void NSHandleMessage(unsigned int plugin_id, const char* data, unsigned int len);
extern "C" NSCAPI::nagiosReturn NSHandleCommand(unsigned int plugin_id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len);
extern "C" int NSUnloadModule(unsigned int plugin_id);
{%if module.cli %}
extern "C" int NSCommandLineExec(unsigned int plugin_id, char *request_buffer, unsigned int request_len, char **response_buffer, unsigned int *response_len);
{% endif %}
{% if module.channels %}
extern "C" int NSHasNotificationHandler(unsigned int plugin_id);
extern "C" int NSHandleNotification(unsigned int plugin_id, const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len);
{% endif %}


#include "{{options.source}}/{{module.name}}.h"

class {{module.name}}Module : public nscapi::impl::simple_plugin {

public:
	boost::shared_ptr<{{module.name}}> impl_;

	{{module.name}}Module() {}
	~{{module.name}}Module() {}

	// Module calls
	/**
	 * Load module
	 * @return True if we loaded successfully.
	 */
	bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	 * Return the module name.
	 * @return The module name
	 */
	static std::string getModuleName() {
		return "{{module.name}}";
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::module_version getModuleVersion() {
		nscapi::module_version version = {0, 3, 0 };
		return version;
	}
	static std::string getModuleDescription() {
		return "{{module.description|cstring}}";
	}

{% if module.commands or module.command_fallback%}
	bool hasCommandHandler() { return true; }
{% else %}
	bool hasCommandHandler() { return false; }
{% endif %}
	NSCAPI::nagiosReturn handleRAWCommand(const std::string &request, std::string &response);

/* Add the following to {{module.name}}

{% if module.commands or module.command_fallback%}
{% for cmd in module.commands %}
{% set cmd_name = cmd.name +"_" if cmd.name == module.name else cmd.name %}
{% if cmd.raw_mapping %}
	void {{cmd_name}}(const std::string &command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response);
{% elif cmd.nagios %}
	NSCAPI::nagiosReturn {{cmd_name}}(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
{% elif cmd.no_mapping %}
{% else %}
	void {{cmd_name}}(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
{% endif %}{% endfor %}{% if module.command_fallback%}	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
{% endif %}
{% endif %}
*/
{%if module.log_handler %}
	bool hasMessageHandler() { return true; }
	void handleMessageRAW(std::string data);
	/*
	Add the following to {{module.name}}
	void handleLogMessage(const Plugin::LogEntry::Entry &message);
	*/
{% endif %}
{% if module.channels %}
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const char* char_command, const std::string &request, std::string &response);
	/*
	Add the following to {{module.name}}
{% if module.channels == "raw" %}
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);
{% else %}
	void handleNotification(const std::wstring channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
{% endif %}
	*/
{% endif %}

{%if module.cli %}
	NSCAPI::nagiosReturn commandRAWLineExec(const std::string &request, std::string &response);
	/*
	Add the following to {{module.name}}
{%if module.cli == "legacy" %}
	NSCAPI::nagiosReturn commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result);
{% else %}
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
{% endif %}
	*/
{% endif %}
	// exposed functions
	void registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy);
};
"""

commands = []
command_fallback = False
module = None
cli = False
log_handler = False
channels = False

class Module:
	name = ''
	title = ''
	description = ''
	alias = ''
	version = None
	loaders = "both"
	
	def __init__(self, data):
		if data['name']:
			self.name = data['name']
		if data['alias']:
			self.alias = data['alias']
		if data['description']:
			self.description = data['description']
		if data['title']:
			self.title = data['title']
		if data['version']:
			if data['version'] == 'auto':
				self.version = None
			else:
				self.version = data['version']
		else:
			self.version = None
		if 'load' in data:
			self.loaders = data['load']
		else:
			self.loaders = "both"
		if 'reload' in data:
			self.reload = data['reload']
		else:
			self.reload = False

	def __repr__(self):
		return self.name

class Command:
	name = ''
	description = ''
	alias = []
	legacy = False
	request = False
	no_mapping = False
	raw_mapping = False
	nagios = False

	def __init__(self, name, description, alias = []):
		self.name = name
		self.description = description
		self.alias = alias
		self.legacy = False
		self.request = False
		self.no_mapping = False
		self.raw_mapping = False
		self.nagios = False

	def __repr__(self):
		if self.alias:
			return '%s (%s)'%(self.name, self.alias)
		return '%s'%self.name

def parse_commands(data):
	global commands, command_fallback
	if data:
		for key, value in data.iteritems():
			desc = ''
			alias = []
			legacy = False
			request = False
			no_mapping = False
			raw_mapping = False
			nagios = False
			if key == "fallback" and value:
				command_fallback = True
			if type(value) is dict:
				if 'desc' in value:
					desc = value['desc']
				elif 'description' in value:
					desc = value['description']
				if 'legacy' in value and value['legacy']:
					legacy = True
				if 'request' in value and value['request']:
					request = True
				if 'nagios' in value and value['nagios']:
					nagios = True
				if 'mapping' in value:
					if value['mapping'] == 'nagios':
						nagios = True
					elif value['mapping'] == 'raw':
						raw_mapping = True
					elif not value['mapping']:
						no_mapping = True
				if 'alias' in value:
					if type(value['alias']) is list:
						alias = value['alias']
					else:
						alias = [ value['alias'] ]
			else:
				desc = value
			if not key == "fallback":
				cmd = Command(key, desc, alias)
				if legacy:
					cmd.legacy = True
				if nagios:
					cmd.nagios = True
				if request:
					cmd.request = True
				if no_mapping:
					cmd.no_mapping = True
				if raw_mapping:
					cmd.raw_mapping = True
				commands.append(cmd)

def parse_module(data):
	global module
	if data:
		module = Module(data)

parser = OptionParser()
parser.add_option("-s", "--source", help="source FILE to read json data from", metavar="FILE")
parser.add_option("-t", "--target", help="target FOLDER folder to write output to", metavar="FOLDER")
(options, args) = parser.parse_args()

data = json.loads(open('%s/module.json'%options.source).read())
for key, value in data.iteritems():
	if key == "module":
		parse_module(value)
	elif key == "commands":
		parse_commands(value)
	elif key == "command line exec":
		if value == "legacy":
			cli = "legacy"
		elif value:
			cli = True
	elif key == "channels" and ( value == 'raw' or value == 'pass-through' ):
		channels = value
	elif key == "channels":
		channels = True
	elif key == "log messages":
		if value:
			log_handler = True
	else:
		print '* TODO: %s'%key

def render_template(hash, template, filename):
	data = template.render(hash).encode('utf8')
	
	path = os.path.dirname(filename)
	if not os.path.exists(path):
		os.makedirs(path)

	if os.path.exists(filename):
		m1 = hashlib.sha256()
		m1.update(data)
		sha1 = m1.digest()
		with open(filename) as f:
			m2 = hashlib.sha256()
			m2.update(f.read())
			sha2 = m2.digest()
		if sha1 == sha2:
			print "no changes detected in: %s"%filename
			return

	print 'Writing file: %s'%filename
	f = open(filename,"w")
	f.write(data)
	f.close()

def escape_cstring(str):
	return str.replace('"', '\\"')

def escape_rcstring(str):
	return str.replace('"', '')

module.commands = commands
module.cli = cli
module.channels = channels
module.log_handler = log_handler
module.command_fallback = command_fallback

env = Environment(extensions=["jinja2.ext.do",])
env.filters['cstring'] = escape_cstring
env.filters['rcstring'] = escape_rcstring

data = {'module': module, 'options': options}
render_template(data, env.from_string(HPP_TEMPLATE), '%s/module.hpp'%options.target)
render_template(data, env.from_string(CPP_TEMPLATE), '%s/module.cpp'%options.target)
render_template(data, env.from_string(DEF_TEMPLATE), '%s/module.def'%options.target)
render_template(data, env.from_string(RC_TEMPLATE), '%s/module.rc'%options.target)

