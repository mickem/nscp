/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "CheckMKClient.h"

#include <time.h>
#include <strEx.h>

#include <protobuf/plugin.pb.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

namespace sh = nscapi::settings_helper;

const std::wstring CheckMKClient::command_prefix = _T("check_mk");
/**
 * Default c-tor
 * @return 
 */
CheckMKClient::CheckMKClient() {}

/**
 * Default d-tor
 * @return 
 */
CheckMKClient::~CheckMKClient() {}

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckMKClient::loadModule() {
	return false;
}

NSCAPI::nagiosReturn CheckMKClient::parse_data(lua::script_information *information, lua::lua_traits::function_type c, const check_mk::packet &packet)
{
	lua::lua_wrapper instance(lua::lua_runtime::prep_function(information, c));
	int args = 1;
	if (c.object_ref != 0)
		args = 2;
	check_mk::check_mk_packet_wrapper* obj = Luna<check_mk::check_mk_packet_wrapper>::createNew(instance);
	obj->packet = packet;
	if (instance.pcall(args, LUA_MULTRET, 0) != 0) {
		NSC_LOG_ERROR_STD(_T("Failed to process check_mk result: ") + utf8::cvt<std::wstring>(instance.pop_string()));
		return NSCAPI::returnUNKNOWN;
	}
	instance.gc(LUA_GCCOLLECT, 0);
	return NSCAPI::returnUNKNOWN;
}



//////////////////////////////////////////////////////////////////////////
bool CheckMKClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {
		root_ = get_core()->getBasePath();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		lua_runtime_->register_plugin(boost::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));


		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("check_mk"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("CHECK MK CLIENT SECTION"), _T("Section for NSCP active/passive check module."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&CheckMKClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&CheckMKClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))

			(_T("scripts"), sh::fun_values_path(boost::bind(&CheckMKClient::add_script, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("channel"), sh::wstring_key(&channel_, _T("NSCP")),
			_T("CHANNEL"), _T("The channel to listen to."))

			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, _T("default"), _T(""), true);

		if (scripts_->empty()) {
			add_script(_T("default"),_T("default_check_mk.lua"));
		}

		get_core()->registerSubmissionListener(get_id(), channel_);
		register_command(command_prefix + _T("_query"), _T("Submit a query to a remote host via NSCP"));
		register_command(command_prefix + _T("_forward"), _T("Forward query to remote NSCP host"));
		register_command(command_prefix + _T("_submit"), _T("Submit a query to a remote host via NSCP"));
		register_command(command_prefix + _T("_exec"), _T("Execute remote command on a remote host via NSCP"));
		register_command(command_prefix + _T("_help"), _T("Help on using NSCP Client"));

		scripts_->load_all();

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_STD(_T("NSClient API exception: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + utf8::to_unicode(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	return true;
}
std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "_NRPE_CHECK";
}

bool CheckMKClient::add_script(std::wstring alias, std::wstring file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = _T("");
		}

		boost::optional<boost::filesystem::wpath> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		NSC_DEBUG_MSG_STD(_T("Adding script: ") + ofile->string() + _T(" as ") + alias + _T(")"));
		scripts_->add(utf8::cvt<std::string>(alias), utf8::cvt<std::string>(ofile->string()));
		return true;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not load script: (Unknown exception) ") + file);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CheckMKClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void CheckMKClient::add_command(std::wstring name, std::wstring args) {
	try {
		std::wstring key = commands.add_command(name, args);
		if (!key.empty())
			register_command(key.c_str(), _T("NSCP relay for: ") + name);
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not add command ") + name + _T(": ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not add command ") + name);
	}
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckMKClient::unloadModule() {
	scripts_.reset();
	lua_runtime_.reset();
	nscp_runtime_.reset();
	return true;
}

NSCAPI::nagiosReturn CheckMKClient::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::QueryRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_query(cmd, config, message, result);
}

NSCAPI::nagiosReturn CheckMKClient::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::ExecuteRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_exec(cmd, config, message, result);
}

NSCAPI::nagiosReturn CheckMKClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &result) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return client::command_line_parser::do_relay_submit(config, message, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void CheckMKClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("certificate,c", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")
		;
}

void CheckMKClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler = boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	std::wstring recipient = utf8::cvt<std::wstring>(config.data->recipient.id);
	if (!targets.has_object(recipient)) {
		recipient = _T("default");
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
	}
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

CheckMKClient::connection_data CheckMKClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::functions::destination_container recipient;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

std::string gather_and_log_errors(std::string  &payload) {
	NSCPIPC::ErrorMessage message;
	message.ParseFromString(payload);
	std::string ret;
	for (int i=0;i<message.error_size();i++) {
		ret += message.error(i).message();
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
	}
	return ret;
}
int CheckMKClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	int ret = NSCAPI::returnUNKNOWN;
	connection_data con = parse_header(request_header, data);

	Plugin::QueryResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	instance->send(con);
	response_message.SerializeToString(&reply);
	return ret;
}

int CheckMKClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("check_mk does not support submit patterns"));
	nscapi::functions::create_simple_submit_response(_T("N/A"), _T("UNKNOWN"), NSCAPI::returnUNKNOWN, _T("SYSLOG does not support query patterns"), reply);
	return NSCAPI::hasFailed;
}

int CheckMKClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("check_mk does not support submit patterns"));
	nscapi::functions::create_simple_exec_response(_T("UNKNOWN"), NSCAPI::returnUNKNOWN, _T("SYSLOG does not support query patterns"), reply);
	return NSCAPI::hasFailed;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//
struct client_handler : public socket_helpers::client::client_handler {
	client_handler(CheckMKClient::connection_data &con) 
		: socket_helpers::client::client_handler(con.host, con.port, con.timeout, con.use_ssl, con.cert)
	{

	}
	void log_debug(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, utf8::to_unicode(msg));
		}
	}
	void log_error(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, utf8::to_unicode(msg));
		}
	}
};



void CheckMKClient::send(connection_data con) {
	try {
		NSC_DEBUG_MSG_STD(_T("check_mk Connection details: ") + con.to_wstring());
		if (con.use_ssl) {
#ifndef USE_SSL
			NSC_LOG_ERROR_STD(_T("SSL not avalible (compiled without USE_SSL)"));
			return response;
#endif
		}
		socket_helpers::client::client<check_mk::client::protocol> client(boost::shared_ptr<client_handler>(new client_handler(con)));
		client.connect();
		std::string dummy;
		check_mk::packet packet = client.process_request(dummy);
		boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command("check_mk", "c_callback");
		if (cmd) {
			parse_data(cmd->information, cmd->function, packet);
		} else {
			NSC_LOG_ERROR_STD(_T("No check_mk callback found!"));
		}
		//lua_runtime_->on_query()
		client.shutdown();
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Error: ..."));
	}
}

NSC_WRAP_DLL()
NSC_WRAPPERS_MAIN_DEF(CheckMKClient)
NSC_WRAPPERS_IGNORE_MSG_DEF()
NSC_WRAPPERS_HANDLE_CMD_DEF()
NSC_WRAPPERS_CLI_DEF()
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF()

