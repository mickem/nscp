import json
import string
from optparse import OptionParser

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

	def __repr__(self):
		return self.name
		
	def tpl_data(self, commands, command_fallback):
		global cli, log_handler, options
		module_info = {
			'CLASS': self.name,
			'MODULE_NAME': self.name,
			'MODULE_ALIAS': self.alias,
			'SOURCE' : options.source,
			'MODULE_DESCRIPTION': self.description.replace('\n', '\\n'),
			'MODULE_DESCRIPTION_RC': self.description.replace('\n', '').replace('\"', '')
		}
		command_hpp = ''
		command_registrations_cpp = ''
		raw_command_cpp = ''
		command_instances_hpp = ''
		if commands or command_fallback:
			command_instances_cpp = ''
			for c in commands:
				cmd_tpl = c.tpl_data(module_info)
				if c.legacy:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP_LEGACY).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP_LEGACY).substitute(cmd_tpl)
				elif c.request:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP_REQUEST_MESSAGE).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP_REQUEST_MESSAGE).substitute(cmd_tpl)
				elif c.nagios:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP_NAGIOS).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP_NAGIOS).substitute(cmd_tpl)
				elif c.no_mapping:
					# Dont add anything sinxe it is not mapped
					None
				elif c.raw_mapping:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP_RAW).substitute(cmd_tpl)
					command_instances_cpp += string.Template(RAW_COMMAND_INSTANCE_CPP).substitute(cmd_tpl)
				else:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP).substitute(cmd_tpl)
				if c.alias and len(c.alias) == 1:
					command_registrations_cpp += string.Template(COMMAND_REGISTRATION_ALIAS_CPP).substitute(cmd_tpl)
				else:
					command_registrations_cpp += string.Template(COMMAND_REGISTRATION_CPP).substitute(cmd_tpl)
			if command_fallback:
				command_instances_hpp += COMMAND_INSTANCE_HPP_FALLBACK
				command_instances_cpp += COMMAND_INSTANCE_CPP_FALLBACK

			commands_hpp = COMMAND_DELEGATOR_HPP_TRUE
			commands_cpp = string.Template(COMMAND_DELEGATOR_CPP_TRUE).substitute(dict(module_info.items() + 
				{
					'COMMAND_REGISTRATIONS_CPP': command_registrations_cpp, 
					'COMMAND_INSTANCES_CPP' : command_instances_cpp
				}.items()))
			commands_hpp_def = COMMAND_DELEGATOR_DEF_HPP_TRUE
			commands_cpp_def = COMMAND_DELEGATOR_DEF_CPP_TRUE
		else:
			commands_hpp = COMMAND_DELEGATOR_HPP_FALSE
			commands_cpp = string.Template(COMMAND_DELEGATOR_CPP_FALSE).substitute(dict(module_info.items()))
			commands_hpp_def = COMMAND_DELEGATOR_DEF_HPP_FALSE
			commands_cpp_def = COMMAND_DELEGATOR_DEF_CPP_FALSE
			
		if log_handler:
			log_delegator_hpp = string.Template(LOG_DELEGATOR_HPP_TRUE).substitute(module_info)
			log_delegator_cpp = string.Template(LOG_DELEGATOR_CPP_TRUE).substitute(module_info)
			log_delegator_hpp_def = string.Template(LOG_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			log_delegator_cpp_def = string.Template(LOG_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
		else:
			log_delegator_hpp = string.Template(LOG_DELEGATOR_HPP_FALSE).substitute(module_info)
			log_delegator_cpp = string.Template(LOG_DELEGATOR_CPP_FALSE).substitute(module_info)
			log_delegator_hpp_def = string.Template(LOG_DELEGATOR_DEF_HPP_FALSE).substitute(module_info)
			log_delegator_cpp_def = string.Template(LOG_DELEGATOR_DEF_CPP_FALSE).substitute(module_info)
		if cli == "legacy":
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_LEGACY).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_LEGACY).substitute(module_info)
			cli_delegator_hpp_def = string.Template(CLI_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			cli_delegator_cpp_def = string.Template(CLI_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
			cli_delegator_def_def = string.Template(CLI_DELEGATOR_DEF_DEF_TRUE).substitute(module_info)
		elif cli == "pass-through":
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_PASS_THROUGH).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_PASS_THROUGH).substitute(module_info)
			cli_delegator_hpp_def = string.Template(CLI_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			cli_delegator_cpp_def = string.Template(CLI_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
			cli_delegator_def_def = string.Template(CLI_DELEGATOR_DEF_DEF_TRUE).substitute(module_info)
		elif cli:
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_TRUE).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_TRUE).substitute(module_info)
			cli_delegator_hpp_def = string.Template(CLI_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			cli_delegator_cpp_def = string.Template(CLI_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
			cli_delegator_def_def = string.Template(CLI_DELEGATOR_DEF_DEF_TRUE).substitute(module_info)
		else:
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_FALSE).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_FALSE).substitute(module_info)
			cli_delegator_hpp_def = string.Template(CLI_DELEGATOR_DEF_HPP_FALSE).substitute(module_info)
			cli_delegator_cpp_def = string.Template(CLI_DELEGATOR_DEF_CPP_FALSE).substitute(module_info)
			cli_delegator_def_def = string.Template(CLI_DELEGATOR_DEF_DEF_FALSE).substitute(module_info)
			
		if channels == "pass-through" or channels == "raw":
			channel_delegator_hpp = string.Template(CHANNEL_DELEGATOR_HPP_PASS_THROUGH).substitute(module_info)
			channel_delegator_cpp = string.Template(CHANNEL_DELEGATOR_CPP_PASS_THROUGH).substitute(module_info)
			channel_delegator_hpp_def = string.Template(CHANNEL_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			channel_delegator_cpp_def = string.Template(CHANNEL_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
			channel_delegator_def_def = string.Template(CHANNEL_DELEGATOR_DEF_DEF_TRUE).substitute(module_info)
		elif channels:
			channel_delegator_hpp = string.Template(CHANNEL_DELEGATOR_HPP_TRUE).substitute(module_info)
			channel_delegator_cpp = string.Template(CHANNEL_DELEGATOR_CPP_TRUE).substitute(module_info)
			channel_delegator_hpp_def = string.Template(CHANNEL_DELEGATOR_DEF_HPP_TRUE).substitute(module_info)
			channel_delegator_cpp_def = string.Template(CHANNEL_DELEGATOR_DEF_CPP_TRUE).substitute(module_info)
			channel_delegator_def_def = string.Template(CHANNEL_DELEGATOR_DEF_DEF_TRUE).substitute(module_info)
		else:
			channel_delegator_hpp = string.Template(CHANNEL_DELEGATOR_HPP_FALSE).substitute(module_info)
			channel_delegator_cpp = string.Template(CHANNEL_DELEGATOR_CPP_FALSE).substitute(module_info)
			channel_delegator_hpp_def = string.Template(CHANNEL_DELEGATOR_DEF_HPP_FALSE).substitute(module_info)
			channel_delegator_cpp_def = string.Template(CHANNEL_DELEGATOR_DEF_CPP_FALSE).substitute(module_info)
			channel_delegator_def_def = string.Template(CHANNEL_DELEGATOR_DEF_DEF_FALSE).substitute(module_info)

		if self.loaders == "both":
			load_delegator = string.Template(LOAD_DELEGATOR_TRUE).substitute(module_info)
			unload_delegator = string.Template(UNLOAD_DELEGATOR_TRUE).substitute(module_info)
		elif self.loaders == "load":
			load_delegator = string.Template(LOAD_DELEGATOR_TRUE).substitute(module_info)
			unload_delegator = string.Template(UNLOAD_DELEGATOR_FALSE).substitute(module_info)
		elif self.loaders == "unload":
			load_delegator = string.Template(LOAD_DELEGATOR_FALSE).substitute(module_info)
			unload_delegator = string.Template(UNLOAD_DELEGATOR_TRUE).substitute(module_info)
		else:
			load_delegator = string.Template(LOAD_DELEGATOR_FALSE).substitute(module_info)
			unload_delegator = string.Template(UNLOAD_DELEGATOR_FALSE).substitute(module_info)
		extra_keys = {
			'COMMAND_DELEGATOR_HPP': commands_hpp,
			'COMMAND_DELEGATOR_CPP': commands_cpp,
			'COMMAND_DELEGATOR_HPP_DEF': commands_hpp_def,
			'COMMAND_DELEGATOR_CPP_DEF': commands_cpp_def,
			'COMMAND_INSTANCES_HPP': command_instances_hpp,
			'CLI_DELEGATOR_HPP' : cli_delegator_hpp,
			'CLI_DELEGATOR_CPP' : cli_delegator_cpp,
			'CLI_DELEGATOR_HPP_DEF' : cli_delegator_hpp_def,
			'CLI_DELEGATOR_CPP_DEF' : cli_delegator_cpp_def,
			'CLI_DELEGATOR_DEF' : cli_delegator_def_def,
			'CHANNEL_DELEGATOR_HPP' : channel_delegator_hpp,
			'CHANNEL_DELEGATOR_CPP' : channel_delegator_cpp,
			'CHANNEL_DELEGATOR_HPP_DEF' : channel_delegator_hpp_def,
			'CHANNEL_DELEGATOR_CPP_DEF' : channel_delegator_cpp_def,
			'CHANNEL_DELEGATOR_DEF' : channel_delegator_def_def,
			'LOG_DELEGATOR_HPP' : log_delegator_hpp,
			'LOG_DELEGATOR_CPP' : log_delegator_cpp,
			'LOG_DELEGATOR_HPP_DEF' : log_delegator_hpp_def,
			'LOG_DELEGATOR_CPP_DEF' : log_delegator_cpp_def,
			'LOAD_DELEGATOR'	: load_delegator,
			'UNLOAD_DELEGATOR'	: unload_delegator,
		}
		if commands:
			return dict(cmd_tpl.items() + extra_keys.items())
		else:
			return dict(module_info.items() + extra_keys.items())

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

	def tpl_data(self, module_data):
		if self.alias:
			alias = self.alias[0]
			alias_lst = 'TODO'
		else:
			alias = ''
			alias_lst = ''
		cmd_name = self.name
		if cmd_name == module_data['CLASS']:
			cmd_name += "_"
		return dict(module_data.items() + {
			'COMMAND_NAME': cmd_name,
			'COMMAND_NAME_ORG': self.name,
			'COMMAND_NAME_LC': self.name.lower(),
			'ALIAS': alias,
			'ALIAS_LST': alias_lst,
			'COMMAND_DESCRIPTION': self.description
		}.items())

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

COMMAND_INSTANCE_HPP = """	void ${COMMAND_NAME}(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
"""
COMMAND_INSTANCE_HPP_LEGACY = """	NSCAPI::nagiosReturn ${COMMAND_NAME}(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
"""
COMMAND_INSTANCE_HPP_REQUEST_MESSAGE = """	void ${COMMAND_NAME}(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
"""
COMMAND_INSTANCE_HPP_FALLBACK = """	void query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message);
"""
COMMAND_INSTANCE_HPP_RAW = """	void ${COMMAND_NAME}(const std::string &command, const Plugin::QueryRequestMessage &request, Plugin::QueryResponseMessage *response);
"""
COMMAND_INSTANCE_HPP_NAGIOS = """	NSCAPI::nagiosReturn ${COMMAND_NAME}(const std::string &target, const std::string &command, std::list<std::string> &arguments, std::string &msg, std::string &perf);
"""

COMMAND_DELEGATOR_HPP_TRUE = """
	bool hasCommandHandler() { return true; }
	NSCAPI::nagiosReturn handleRAWCommand(const std::string &request, std::string &response);
"""
COMMAND_DELEGATOR_HPP_FALSE = """
	bool hasCommandHandler() { return false; }
	NSCAPI::nagiosReturn handleRAWCommand(const std::string &request, std::string &response);
"""

CLI_DELEGATOR_HPP_TRUE = """
	NSCAPI::nagiosReturn commandRAWLineExec(const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	bool commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message);
	*/
"""
CLI_DELEGATOR_HPP_PASS_THROUGH = """
	NSCAPI::nagiosReturn commandRAWLineExec(const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	NSCAPI::nagiosReturn commandLineExec(const char* char_command, const std::string &request, std::string &response);
	*/
"""
CLI_DELEGATOR_HPP_LEGACY = """
	NSCAPI::nagiosReturn commandRAWLineExec(const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	NSCAPI::nagiosReturn commandLineExec(const std::string &command, const std::list<std::string> &arguments, std::string &result);
	*/
"""
CLI_DELEGATOR_HPP_FALSE = ""

CHANNEL_DELEGATOR_HPP_PASS_THROUGH = """
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const char* char_command, const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	void handleNotification(const std::string &channel, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message);
	*/
"""
CHANNEL_DELEGATOR_HPP_TRUE = """
	bool hasNotificationHandler() { return true; };
	NSCAPI::nagiosReturn handleRAWNotification(const char* char_command, const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	void handleNotification(const std::wstring channel, const Plugin::QueryResponseMessage::Response &request, Plugin::SubmitResponseMessage::Response *response, const Plugin::SubmitRequestMessage &request_message);
	*/
"""
CHANNEL_DELEGATOR_HPP_FALSE = ""


LOG_DELEGATOR_HPP_TRUE = """
	bool hasMessageHandler() { return true; }
	void handleMessageRAW(std::string data);
	/*
	Add the following to ${CLASS}
	void handleLogMessage(const Plugin::LogEntry::Entry &message);
	*/
"""
LOG_DELEGATOR_HPP_FALSE = ""

MODULE_HPP = """#pragma once
#include <boost/shared_ptr.hpp>

#include <nscapi/nscapi_plugin_interface.hpp>

NSC_WRAPPERS_MAIN();
${LOG_DELEGATOR_HPP_DEF}
${CLI_DELEGATOR_HPP_DEF}
${CHANNEL_DELEGATOR_HPP_DEF}
${COMMAND_DELEGATOR_HPP_DEF}

#include "${SOURCE}/${CLASS}.h"

class ${CLASS}Module : public nscapi::impl::simple_plugin {

public:
	boost::shared_ptr<${CLASS}> impl_;

	${CLASS}Module() {}
	~${CLASS}Module() {}

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
		return "${MODULE_NAME}";
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	static std::string getModuleDescription() {
		return "${MODULE_DESCRIPTION}";
	}

${COMMAND_DELEGATOR_HPP}
/* Add the following to ${CLASS}

${COMMAND_INSTANCES_HPP}
*/

${LOG_DELEGATOR_HPP}

${CHANNEL_DELEGATOR_HPP}
${CLI_DELEGATOR_HPP}

	// exposed functions
	void registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy);

};
"""
MODULE_RC = """
#include <config.h>

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
            VALUE "FileDescription", "${MODULE_DESCRIPTION_RC}\\0"
            VALUE "FileVersion", STRPRODUCTVER "\\0"
            VALUE "InternalName", "${CLASS}\\0"
            VALUE "LegalCopyright", "Copyright (C) 2014 - Michael Medin\\0"
            VALUE "OriginalFilename", "${CLASS}.DLL\\0"
            VALUE "ProductName", "NSClient++ Module: ${CLASS}\\0"
            VALUE "ProductVersion", STRPRODUCTVER "\\0"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
"""
CHANNEL_DELEGATOR_CPP = """
/**
 * Check if we have a message handler.
 * @return false as we have no message handler
 */
bool ${CLASS}Module::hasMessageHandler() {
	return true;
}

"""
CLI_DELEGATOR_DEF_HPP_TRUE = "NSC_WRAPPERS_CLI()"
CLI_DELEGATOR_DEF_HPP_FALSE = ""
CLI_DELEGATOR_DEF_CPP_TRUE = "NSC_WRAPPERS_CLI_DEF()"
CLI_DELEGATOR_DEF_CPP_FALSE = ""
CLI_DELEGATOR_DEF_DEF_TRUE = "	NSCommandLineExec"
CLI_DELEGATOR_DEF_DEF_FALSE = ""

COMMAND_DELEGATOR_DEF_HPP_TRUE = ""
COMMAND_DELEGATOR_DEF_HPP_FALSE = ""
COMMAND_DELEGATOR_DEF_CPP_TRUE = "NSC_WRAPPERS_HANDLE_CMD_DEF()"
COMMAND_DELEGATOR_DEF_CPP_FALSE = "NSC_WRAPPERS_IGNORE_CMD_DEF()"

CHANNEL_DELEGATOR_DEF_HPP_TRUE = "NSC_WRAPPERS_CHANNELS()"
CHANNEL_DELEGATOR_DEF_HPP_FALSE = ""
CHANNEL_DELEGATOR_DEF_CPP_TRUE = "NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()"
CHANNEL_DELEGATOR_DEF_CPP_FALSE = ""
CHANNEL_DELEGATOR_DEF_DEF_TRUE = "	NSHasNotificationHandler\n	NSHandleNotification"
CHANNEL_DELEGATOR_DEF_DEF_FALSE = ""

LOG_DELEGATOR_DEF_HPP_TRUE = ""
LOG_DELEGATOR_DEF_HPP_FALSE = ""
LOG_DELEGATOR_DEF_CPP_TRUE = "NSC_WRAPPERS_HANDLE_MSG_DEF()"
LOG_DELEGATOR_DEF_CPP_FALSE = "NSC_WRAPPERS_IGNORE_MSG_DEF()"

CLI_DELEGATOR_CPP_LEGACY = """
NSCAPI::nagiosReturn ${CLASS}Module::commandRAWLineExec(const std::string &request, std::string &response) {
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
"""
CLI_DELEGATOR_CPP_PASS_THROUGH = """
NSCAPI::nagiosReturn ${CLASS}Module::commandRAWLineExec(const std::string &request, std::string &response) {
	return impl_->commandLineExec(request, response);
}
"""
CLI_DELEGATOR_CPP_TRUE = """
NSCAPI::nagiosReturn ${CLASS}Module::commandRAWLineExec(const std::string &request, std::string &response) {
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
"""

CLI_DELEGATOR_CPP_FALSE = ""

CHANNEL_DELEGATOR_CPP_PASS_THROUGH = """
NSCAPI::nagiosReturn ${CLASS}Module::handleRAWNotification(const char* char_channel, const std::string &request, std::string &response) {
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
"""
CHANNEL_DELEGATOR_CPP_TRUE = """
NSCAPI::nagiosReturn ${CLASS}Module::handleRAWNotification(const char* char_channel, const std::string &request, std::string &response) {
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
"""
CHANNEL_DELEGATOR_CPP_FALSE = ""


LOG_DELEGATOR_CPP_FALSE = ""
LOG_DELEGATOR_CPP_TRUE = """
void ${CLASS}Module::handleMessageRAW(std::string data) {
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
"""

COMMAND_INSTANCE_CPP = """			} else if (request_payload.command() == "${COMMAND_NAME_LC}") {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->${COMMAND_NAME}(request_payload, response_payload);
"""
COMMAND_INSTANCE_CPP_REQUEST_MESSAGE = """			} else if (request_payload.command() == "${COMMAND_NAME}") {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->${COMMAND_NAME}(request_payload, response_payload, request_message);
"""
COMMAND_INSTANCE_CPP_FALLBACK = """			} else {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->query_fallback(request_payload, response_payload, request_message);
"""
COMMAND_INSTANCE_CPP_LEGACY = """			} else if (request_payload.command() == "${COMMAND_NAME}") {
				std::string msg, perf;
				std::list<std::string> args;
				for (int i=0;i<request_payload.arguments_size();i++) {
					args.push_back(request_payload.arguments(i));
				}
				NSCAPI::nagiosReturn ret = impl_->${COMMAND_NAME}(request_payload.target(), boost::algorithm::to_lower_copy(request_payload.command()), args, msg, perf);
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				response_payload->set_message(msg);
				response_payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
				if (!perf.empty())
					nscapi::protobuf::functions::parse_performance_data(response_payload, perf);
"""
COMMAND_REGISTRATION_ALIAS_CPP = """		("${COMMAND_NAME}", "${ALIAS}",
		"${COMMAND_DESCRIPTION}")
"""
COMMAND_REGISTRATION_CPP = """		("${COMMAND_NAME_ORG}",
		"${COMMAND_DESCRIPTION}")
"""
RAW_COMMAND_INSTANCE_CPP = """			} else if (request_payload.command() == "${COMMAND_NAME}") {
				impl_->${COMMAND_NAME}("${COMMAND_NAME}", request_message, &response_message);
				response_message.SerializeToString(&response);
				return NSCAPI::isSuccess;
"""
COMMAND_INSTANCE_CPP_NAGIOS = """			} else if (request_payload.command() == "${COMMAND_NAME}") {
				std::string msg, perf;
				std::list<std::string> args;
				for (int i=0;i<request_payload.arguments_size();i++) {
					args.push_back(request_payload.arguments(i));
				}
				NSCAPI::nagiosReturn ret = impl_->${COMMAND_NAME}(request_payload.target(), boost::algorithm::to_lower_copy(request_payload.command()), args, msg, perf);
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				response_payload->set_message(msg);
				response_payload->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(ret));
				if (!perf.empty())
					nscapi::protobuf::functions::parse_performance_data(response_payload, perf);
"""


COMMAND_DELEGATOR_CPP_TRUE = """
/**
 * Main command parser and delegator.
 *
 * @param char_command The command name (string)
 * @param request The request packet
 * @param response THe response packet
 * @return status code
 */
NSCAPI::nagiosReturn ${CLASS}Module::handleRAWCommand(const std::string &request, std::string &response) {
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
${COMMAND_INSTANCES_CPP}
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

void ${CLASS}Module::registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy) {
	ch::command_registry registry(proxy);
	registry.command()
${COMMAND_REGISTRATIONS_CPP}
		;
/*
	registry.add_metadata(_T("check_cpu"))
		(_T("guide"), _T("http://nsclient.org/nscp/wiki/doc/usage/nagios/nsca"))
		;
*/
	registry.register_all();
}
"""
COMMAND_DELEGATOR_CPP_FALSE = """
void ${CLASS}Module::registerCommands(boost::shared_ptr<nscapi::command_proxy> proxy) {}
"""


LOAD_DELEGATOR_TRUE = "		return impl_->loadModuleEx(alias, mode);"
LOAD_DELEGATOR_FALSE = "		return true;"
UNLOAD_DELEGATOR_TRUE = "		ret = impl_->unloadModule();"
UNLOAD_DELEGATOR_FALSE = "		ret = true;"

MODULE_CPP = """
#include <nscapi/nscapi_plugin_interface.hpp>
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
bool ${CLASS}Module::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
	try {
		if (impl_) {
			unloadModule();
		}
		impl_.reset(new ${CLASS});
		impl_->set_id(get_id());
		registerCommands(get_command_proxy());
${LOAD_DELEGATOR}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to load ${CLASS}: ", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to load ${CLASS}: ");
		return false;
	}
}

bool ${CLASS}Module::unloadModule() {
	bool ret = false;
	if (impl_) {
${UNLOAD_DELEGATOR}
	}
	impl_.reset();
	return ret;
}

${COMMAND_DELEGATOR_CPP}

${LOG_DELEGATOR_CPP}

${CHANNEL_DELEGATOR_CPP}

${CLI_DELEGATOR_CPP}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(${CLASS}Module, "${MODULE_ALIAS}")
${LOG_DELEGATOR_CPP_DEF}
${COMMAND_DELEGATOR_CPP_DEF}
${CLI_DELEGATOR_CPP_DEF}
${CHANNEL_DELEGATOR_CPP_DEF}
"""

MODULE_DEF = """
LIBRARY	${CLASS}

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
${CHANNEL_DELEGATOR_DEF}
${CLI_DELEGATOR_DEF}
"""


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


tpl_data = module.tpl_data(commands, command_fallback)
hpp=open('%s/module.hpp'%options.target, 'w+')
hpp.write(string.Template(MODULE_HPP).substitute(tpl_data))
print "Updated module.hpp"
cpp=open('%s/module.cpp'%options.target, 'w+')
cpp.write(string.Template(MODULE_CPP).substitute(tpl_data))
print "Updated module.cpp"
cpp=open('%s/module.def'%options.target, 'w+')
cpp.write(string.Template(MODULE_DEF).substitute(tpl_data))
print "Updated module.def"
hpp=open('%s/module.rc'%options.target, 'w+')
hpp.write(string.Template(MODULE_RC).substitute(tpl_data))
print "Updated module.rc"
