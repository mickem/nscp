import json
import string
from optparse import OptionParser

commands = []
module = None
cli = False
log_handler = False

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
		
	def tpl_data(self, commands):
		global cli, log_handler, options
		module_info = {
			'CLASS': self.name,
			'NAME': self.name,
			'SOURCE' : options.source,
			'DESCRIPTION': self.description
		}
		command_hpp = ''
		command_instances_hpp = ''
		command_instances_cpp = ''
		command_registrations_cpp = ''
		if commands:
			commands_hpp = COMMAND_DELEGATOR_HPP
			for c in commands:
				cmd_tpl = c.tpl_data(module_info)
				if c.legacy:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP_LEGACY).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP_LEGACY).substitute(cmd_tpl)
				else:
					command_instances_hpp += string.Template(COMMAND_INSTANCE_HPP).substitute(cmd_tpl)
					command_instances_cpp += string.Template(COMMAND_INSTANCE_CPP).substitute(cmd_tpl)
				if c.alias and len(c.alias) == 1:
					command_registrations_cpp += string.Template(COMMAND_REGISTRATION_ALIAS_CPP).substitute(cmd_tpl)
				else:
					command_registrations_cpp += string.Template(COMMAND_REGISTRATION_CPP).substitute(cmd_tpl)
			commands_cpp = string.Template(COMMAND_CPP).substitute(dict(module_info.items() + 
				{
					'COMMAND_REGISTRATIONS_CPP': command_registrations_cpp, 
					'COMMAND_INSTANCES_CPP' : command_instances_cpp
				}.items()))
		if log_handler:
			log_delegator_hpp = LOG_DELEGATOR_HPP_TRUE
		else:
			log_delegator_hpp = LOG_DELEGATOR_HPP_FALSE
		if cli == "legacy":
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_LEGACY).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_LEGACY).substitute(module_info)
			cli_delegator_def = string.Template(CLI_DELEGATOR_DEF_LEGACY).substitute(module_info)
		elif cli:
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_TRUE).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_TRUE).substitute(module_info)
			cli_delegator_def = string.Template(CLI_DELEGATOR_DEF_TRUE).substitute(module_info)
		else:
			cli_delegator_hpp = string.Template(CLI_DELEGATOR_HPP_FALSE).substitute(module_info)
			cli_delegator_cpp = string.Template(CLI_DELEGATOR_CPP_FALSE).substitute(module_info)
			cli_delegator_def = string.Template(CLI_DELEGATOR_DEF_FALSE).substitute(module_info)
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
		return dict(cmd_tpl.items() + {
			'COMMAND_DELEGATOR_HPP': commands_hpp,
			'COMMAND_CPP': commands_cpp,
			'COMMAND_INSTANCES_HPP': command_instances_hpp,
			'CLI_DELEGATOR_HPP' : cli_delegator_hpp,
			'CLI_DELEGATOR_CPP' : cli_delegator_cpp,
			'CLI_DELEGATOR_DEF' : cli_delegator_def,
			'LOG_DELEGATOR_HPP' : log_delegator_hpp,
			'LOG_DELEGATOR_CPP' : '/* TODO: Add support for LOG delegators */',
			'LOAD_DELEGATOR'	: load_delegator,
			'UNLOAD_DELEGATOR'	: unload_delegator,
			'CHANNEL_DELEGATOR_HPP' : '/* TODO: Add support for channel delegators */',
			'CHANNEL_DELEGATOR_CPP' : '/* TODO: Add support for channel delegators */'
		}.items())

class Command:
	name = ''
	description = ''
	alias = []
	legacy = False

	def __init__(self, name, description, alias = []):
		self.name = name
		self.description = description
		self.alias = alias
		self.legacy = False

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
		return dict(module_data.items() + {
			'NAME': self.name,
			'ALIAS': alias,
			'ALIAS_LST': alias_lst,
			'DESCRIPTION': self.description
		}.items())

def parse_commands(data):
	global commands
	if data:
		for key, value in data.iteritems():
			desc = ''
			alias = []
			legacy = False
			if type(value) is dict:
				if 'desc' in value:
					desc = value['desc']
				elif 'description' in value:
					desc = value['description']
				if 'legacy' in value and value['legacy']:
					legacy = True
				if 'alias' in value:
					if type(value['alias']) is list:
						alias = value['alias']
					else:
						alias = [ value['alias'] ]
			else:
				desc = value
			cmd = Command(key, desc, alias)
			if legacy:
				cmd.legacy = True
			commands.append(cmd)

def parse_module(data):
	global module
	if data:
		module = Module(data)

COMMAND_INSTANCE_HPP = """	void ${NAME}(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response);
"""
COMMAND_INSTANCE_HPP_LEGACY = """	NSCAPI::nagiosReturn ${NAME}(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &msg, std::wstring &perf);
"""

COMMAND_DELEGATOR_HPP = """
	bool hasCommandHandler();
	NSCAPI::nagiosReturn handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response);
"""

CLI_DELEGATOR_HPP_TRUE = """
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
	*/
"""
CLI_DELEGATOR_HPP_LEGACY = """
	NSCAPI::nagiosReturn commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response);
	/*
	Add the following to ${CLASS}
	NSCAPI::nagiosReturn commandRAWLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result);
	*/
"""
CLI_DELEGATOR_HPP_FALSE = ""

CHANNEL_DELEGATOR_HPP_TRUE = """
	bool hasMessageHandler();
	NSCAPI::nagiosReturn handleRAWNotification(const wchar_t* channel, std::string request, std::string &response);
"""
CHANNEL_DELEGATOR_HPP_FALSE = ""

LOG_DELEGATOR_HPP_TRUE = """
	bool hasMessageHandler();
	void handleMessageRAW(std::string data);
"""
LOG_DELEGATOR_HPP_FALSE = ""

MODULE_HPP = """
#pragma once

NSC_WRAPPERS_MAIN();
NSC_WRAPPERS_CLI();

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
	bool loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode);
	bool unloadModule();

	/**
	 * Return the module name.
	 * @return The module name
	 */
	static std::wstring getModuleName() {
		return _T("${NAME}");
	}
	/**
	* Module version
	* @return module version
	*/
	static nscapi::plugin_wrapper::module_version getModuleVersion() {
		nscapi::plugin_wrapper::module_version version = {0, 3, 0 };
		return version;
	}
	static std::wstring getModuleDescription() {
		return _T("${DESCRIPTION}");
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
CHANNEL_DELEGATOR_CPP = """
/**
 * Check if we have a message handler.
 * @return false as we have no message handler
 */
bool ${CLASS}Module::hasMessageHandler() {
	return true;
}

"""
CLI_DELEGATOR_DEF_TRUE = "NSC_WRAPPERS_CLI_DEF()"
CLI_DELEGATOR_DEF_FALSE = ""
CLI_DELEGATOR_DEF_LEGACY = CLI_DELEGATOR_DEF_TRUE

CLI_DELEGATOR_CPP_LEGACY = """
NSCAPI::nagiosReturn ${CLASS}Module::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	nscapi::protobuf::types::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request(char_command, request);
	std::wstring result;
	NSCAPI::nagiosReturn ret = impl_->commandLineExec(data.command, data.args, result);
	if (ret == NSCAPI::returnIgnored)
		return NSCAPI::returnIgnored;
	nscapi::functions::create_simple_exec_response(data.command, ret, result, response);
	return ret;
}
"""
CLI_DELEGATOR_CPP_TRUE = """
NSCAPI::nagiosReturn ${CLASS}Module::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &response) {
	return impl_->commandLineExec(char_command, request, response);
}
"""
CLI_DELEGATOR_CPP_FALSE = ""


COMMAND_INSTANCE_CPP = """			} else if (command == "${NAME}") {
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				impl_->${NAME}(request_payload, response_payload);
"""
COMMAND_INSTANCE_CPP_LEGACY = """			} else if (command == "${NAME}") {
				std::wstring msg, perf;
				std::list<std::wstring> args;
				for (int i=0;i<request_payload.arguments_size();i++) {
					args.push_back(utf8::cvt<std::wstring>(request_payload.arguments(i)));
				}
				NSCAPI::nagiosReturn ret = impl_->${NAME}(utf8::cvt<std::wstring>(request_payload.target()), boost::algorithm::to_lower_copy(utf8::cvt<std::wstring>(request_payload.command())), args, msg, perf);
				Plugin::QueryResponseMessage::Response *response_payload = response_message.add_payload();
				response_payload->set_command(request_payload.command());
				response_payload->set_message(utf8::cvt<std::string>(msg));
				response_payload->set_result(Plugin::Common_ResultCode_UNKNOWN);
				if (!perf.empty())
					nscapi::functions::parse_performance_data(response_payload, perf);
"""
COMMAND_REGISTRATION_ALIAS_CPP = """		(_T("${NAME}"), _T("${ALIAS}"),
		_T("${DESCRIPTION}"))

"""
COMMAND_REGISTRATION_CPP = """		(_T("${NAME}"),
		_T("${DESCRIPTION}"))

"""

COMMAND_CPP = """
/**
 * Check if we have a command handler.
 * @return true (as we have a command handler)
 */
bool ${CLASS}Module::hasCommandHandler() {
	return true;
}

/**
 * Main command parser and delegator.
 *
 * @param char_command The command name (string)
 * @param request The request packet
 * @param response THe response packet
 * @return status code
 */
NSCAPI::nagiosReturn ${CLASS}Module::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &response) {
	std::string command = utf8::cvt<std::string>(char_command);
	try {
		Plugin::QueryRequestMessage request_message;
		Plugin::QueryResponseMessage response_message;
		request_message.ParseFromString(request);
		nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_message.header());

		for (int i=0;i<request_message.payload_size();i++) {
			Plugin::QueryRequestMessage::Request request_payload = request_message.payload(i);
			Plugin::QueryResponseMessage::Response response_payload;
			if (!impl_) {
				return NSCAPI::returnIgnored;
${COMMAND_INSTANCES_CPP}
			}
		}
		response_message.SerializeToString(&response);
		return NSCAPI::isSuccess;
	} catch (const std::exception &e) {
		return nscapi::functions::create_simple_query_response_unknown(command, std::string("Failed to process command ") + command + ": " + e.what(), response);
	} catch (...) {
		return nscapi::functions::create_simple_query_response_unknown(command, "Failed to process command: " + command, response);
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

LOAD_DELEGATOR_TRUE = "		return impl_->loadModuleEx(alias, mode);"
LOAD_DELEGATOR_FALSE = "		return true;"
UNLOAD_DELEGATOR_TRUE = "		ret = impl_->unloadModule();"
UNLOAD_DELEGATOR_FALSE = "		ret = true;"

MODULE_CPP = """
#include <nscapi/macros.hpp>
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
bool ${CLASS}Module::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		if (impl_) {
			unloadModule();
		}
		impl_.reset(new ${CLASS});
		impl_->set_id(get_id());
		registerCommands(get_command_proxy());
${LOAD_DELEGATOR}
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to load ${CLASS}: ") + utf8::cvt<std::wstring>(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to load ${CLASS}: Unknwon exception"));
		return false;
	}
	return true;
}

bool ${CLASS}Module::unloadModule() {
	bool ret = false;
	if (impl_) {
${UNLOAD_DELEGATOR}
	}
	impl_.reset();
	return ret;
}

${COMMAND_CPP}

${LOG_DELEGATOR_CPP}

${CHANNEL_DELEGATOR_CPP}

${CLI_DELEGATOR_CPP}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(${CLASS}Module, _T("w32system"))
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
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
	elif key == "log messages":
		if value:
			log_handler = True
	else:
		print '* TODO: %s'%key


tpl_data = module.tpl_data(commands)
hpp=open('%s/module.hpp'%options.target, 'w+')
hpp.write(string.Template(MODULE_HPP).substitute(tpl_data))
print "Updated module.hpp"
cpp=open('%s/module.cpp'%options.target, 'w+')
cpp.write(string.Template(MODULE_CPP).substitute(tpl_data))
print "Updated module.cpp"
