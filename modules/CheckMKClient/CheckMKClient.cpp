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
#include "CheckMKClient.h"

#include <time.h>
#include <strEx.h>

#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

namespace sh = nscapi::settings_helper;

const std::string command_prefix("check_mk");
const std::string default_command("query");
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

NSCAPI::nagiosReturn CheckMKClient::parse_data(lua::script_information *information, lua::lua_traits::function_type c, const check_mk::packet &packet)
{
	lua::lua_wrapper instance(lua::lua_runtime::prep_function(information, c));
	int args = 1;
	if (c.object_ref != 0)
		args = 2;
	check_mk::check_mk_packet_wrapper* obj = Luna<check_mk::check_mk_packet_wrapper>::createNew(instance);
	obj->packet = packet;
	if (instance.pcall(args, LUA_MULTRET, 0) != 0) {
		NSC_LOG_ERROR_STD("Failed to process check_mk result: " + instance.pop_string());
		return NSCAPI::returnUNKNOWN;
	}
	instance.gc(LUA_GCCOLLECT, 0);
	return NSCAPI::returnUNKNOWN;
}



//////////////////////////////////////////////////////////////////////////
bool CheckMKClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
	std::map<std::wstring,std::wstring> commands;

	try {
		root_ = get_base_path();
		nscp_runtime_.reset(new scripts::nscp::nscp_runtime_impl(get_id(), get_core()));
		lua_runtime_.reset(new lua::lua_runtime(utf8::cvt<std::string>(root_.string())));
		lua_runtime_->register_plugin(boost::shared_ptr<check_mk::check_mk_plugin>(new check_mk::check_mk_plugin()));
		scripts_.reset(new scripts::script_manager<lua::lua_traits>(lua_runtime_, nscp_runtime_, get_id(), utf8::cvt<std::string>(alias)));


		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias("check_mk", alias, "client");
		target_path = settings.alias().get_settings_path("targets");

		settings.alias().add_path_to_settings()
			("CHECK MK CLIENT SECTION", "Section for NSCP active/passive check module.")

			("handlers", sh::fun_values_path(boost::bind(&CheckMKClient::add_command, this, _1, _2)), 
			"CLIENT HANDLER SECTION", "",
			"CLIENT", "For more configuration options add a dedicated section")

			("targets", sh::fun_values_path(boost::bind(&CheckMKClient::add_target, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "",
			"TARGET", "For more configuration options add a dedicated section")

			("scripts", sh::fun_values_path(boost::bind(&CheckMKClient::add_script, this, _1, _2)), 
			"REMOTE TARGET DEFINITIONS", "",
			"SCRIPT", "For more configuration options add a dedicated section")
			;

		settings.alias().add_key_to_settings()
			("channel", sh::string_key(&channel_, "CheckMK"),
			"CHANNEL", "The channel to listen to.")

			;


		settings.register_all();
		settings.notify();

		targets.add_samples(get_settings_proxy(), target_path);
		targets.add_missing(get_settings_proxy(), target_path, "default", "", true);

		if (scripts_->empty()) {
			add_script("default", "default_check_mk.lua");
		}

		nscapi::core_helper core(get_core(), get_id());
		core.register_channel(channel_);

		scripts_->load_all();

	} catch (nscapi::nscapi_exception &e) {
		NSC_LOG_ERROR_EXR("Load", e);
		return false;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Load", e);
		return false;
	} catch (...) {
		NSC_LOG_ERROR_EX("Load");
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

bool CheckMKClient::add_script(std::string alias, std::string file) {
	try {
		if (file.empty()) {
			file = alias;
			alias = "";
		}

		boost::optional<boost::filesystem::path> ofile = lua::lua_script::find_script(root_, file);
		if (!ofile)
			return false;
		scripts_->add(alias, ofile->string());
		return true;
	} catch (...) {
		NSC_LOG_ERROR("Could not load script: " + file);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CheckMKClient::add_target(std::string key, std::string arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add target: " + key);
	}
}

void CheckMKClient::add_command(std::string name, std::string args) {
	try {
		nscapi::core_helper core(get_core(), get_id());
		std::string key = commands.add_command(name, args);
		if (!key.empty())
			core.register_command(key.c_str(), "check_mk relay for: " + name);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to add command: " + name, e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to add command: " + name);
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

void CheckMKClient::query_fallback(const Plugin::QueryRequestMessage::Request &request, Plugin::QueryResponseMessage::Response *response, const Plugin::QueryRequestMessage &request_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this)), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	commands.parse_query(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

bool CheckMKClient::commandLineExec(const Plugin::ExecuteRequestMessage::Request &request, Plugin::ExecuteResponseMessage::Response *response, const Plugin::ExecuteRequestMessage &request_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this)), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	return commands.parse_exec(command_prefix, default_command, request.command(), config, request, *response, request_message);
}

void CheckMKClient::handleNotification(const std::string &, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage *response_message) {
	client::configuration config(command_prefix, boost::shared_ptr<clp_handler_impl>(new clp_handler_impl(this)), boost::shared_ptr<target_handler>(new target_handler(targets)));
	setup(config, request_message.header());
	commands.forward_submit(config, request_message, *response_message);
}

nscapi::protobuf::types::destination_container CheckMKClient::target_handler::lookup_target(std::string &id) const {
	nscapi::targets::optional_target_object opt = targets_.find_object(id);
	if (opt)
		return opt->to_destination_container();
	nscapi::protobuf::types::destination_container ret;
	return ret;
}

bool CheckMKClient::target_handler::has_object(std::string alias) const {
	return targets_.has_object(alias);
}
bool CheckMKClient::target_handler::apply(nscapi::protobuf::types::destination_container &dst, const std::string key) {
	nscapi::targets::optional_target_object opt = targets_.find_object(key);
	if (opt)
		dst.apply(opt->to_destination_container());
	return static_cast<bool>(opt);
}
//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void CheckMKClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("certificate,c", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate", _1)), 
		"Length of payload (has to be same as on the server)")

		("dh", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "dh", _1)), 
		"Length of payload (has to be same as on the server)")

		("certificate-key,k", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate key", _1)), 
		"Client certificate to use")

		("certificate-format", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "certificate format", _1)), 
		"Client certificate format")

		("ca", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "ca", _1)), 
		"Certificate authority")

		("verify", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "verify mode", _1)), 
		"Client certificate format")

		("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_string_data, &data->recipient, "allowed ciphers", _1)), 
		"Client certificate format")

		("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
		"Length of payload (has to be same as on the server)")

		("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
			"Length of payload (has to be same as on the server)")

 		("ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_bool_data, &data->recipient, "ssl", _1)), 
			"Initial an ssl handshake with the server.")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::protobuf::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")


		;
}

void CheckMKClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	config.default_command = default_command;
	std::string recipient = config.data->recipient.id;
	if (!targets.has_object(recipient))
		recipient = "default";
	config.target_lookup->apply(config.data->recipient, recipient);
	config.data->host_self.id = "self";
	//config.data->host_self.host = hostname_;
}

CheckMKClient::connection_data parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::protobuf::functions::destination_container recipient;
	nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
	return CheckMKClient::connection_data(recipient, data->recipient);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int CheckMKClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	int ret = NSCAPI::returnUNKNOWN;
	connection_data con = parse_header(request_header, data);

	nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

	instance->send(con);
	return ret;
}

int CheckMKClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &, Plugin::SubmitResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("check_mk does not support submit patterns");
	nscapi::protobuf::functions::set_response_good(*response_message.add_payload(), "check_mk does not support query pattern");
	return NSCAPI::isSuccess;
}

int CheckMKClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &, Plugin::ExecuteResponseMessage &response_message) {
	NSC_LOG_ERROR_STD("check_mk does not support submit patterns");
	nscapi::protobuf::functions::set_response_good(*response_message.add_payload(), "check_mk does not support exec pattern");
	return NSCAPI::isSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//
struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::debug)) {
			GET_CORE()->log(NSCAPI::log_level::debug, file, line, utf8::utf8_from_native(msg));
		}
	}
	void log_error(std::string file, int line, std::string msg) const {
		if (GET_CORE()->should_log(NSCAPI::log_level::error)) {
			GET_CORE()->log(NSCAPI::log_level::error, file, line, utf8::utf8_from_native(msg));
		}
	}
	std::string expand_path(std::string path) {
		return GET_CORE()->expand_path(path);
	}

};

void CheckMKClient::send(connection_data con) {
	try {
		NSC_DEBUG_MSG_STD("Connection details: " + con.to_string());
		if (con.ssl.enabled) {
#ifndef USE_SSL
			NSC_LOG_ERROR_STD(_T("SSL not available (compiled without USE_SSL)"));
			return response;
#endif
		}
		socket_helpers::client::client<check_mk::client::protocol> client(con, boost::shared_ptr<client_handler>(new client_handler()));
		client.connect();
		std::string dummy;
		check_mk::packet packet = client.process_request(dummy);
		boost::optional<scripts::command_definition<lua::lua_traits> > cmd = scripts_->find_command("check_mk", "c_callback");
		if (cmd) {
			parse_data(cmd->information, cmd->function, packet);
		} else {
			NSC_LOG_ERROR_STD("No check_mk callback found!");
		}
		//lua_runtime_->on_query()
		client.shutdown();
	} catch (std::runtime_error &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
	} catch (std::exception &e) {
		NSC_LOG_ERROR_EXR("Failed to send", e);
	} catch (...) {
		NSC_LOG_ERROR_EX("Failed to send");
	}
}
