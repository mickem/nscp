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
#include "NSCAClient.h"

#include <utils.h>
#include <strEx.h>

#include <nsca/nsca_enrypt.hpp>
#include <nsca/nsca_packet.hpp>
#include <nsca/nsca_socket.hpp>

#include <settings/client/settings_client.hpp>

namespace sh = nscapi::settings_helper;

const std::wstring NSCAAgent::command_prefix = _T("nsca");
/**
 * Default c-tor
 * @return 
 */
NSCAAgent::NSCAAgent() {}

/**
 * Default d-tor
 * @return 
 */
NSCAAgent::~NSCAAgent() {}

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool NSCAAgent::loadModule() {
	return false;
}

bool NSCAAgent::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCA"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("NSCA CLIENT SECTION"), _T("Section for NSCA passive check module."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&NSCAAgent::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&NSCAAgent::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("hostname"), sh::string_key(&hostname_, "auto"),
			_T("HOSTNAME"), _T("The host name of this host if set to blank (default) the windows name of the computer will be used."))

			(_T("channel"), sh::wstring_key(&channel_, _T("NSCA")),
			_T("CHANNEL"), _T("The channel to listen to."))

			(_T("delay"), sh::string_fun_key<std::wstring>(boost::bind(&NSCAAgent::set_delay, this, _1), _T("0")),
			_T("DELAY"), _T(""))
			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, _T("default"), _T(""), true);


		get_core()->registerSubmissionListener(get_id(), channel_);

		register_command(_T("nsca_query"), _T("Check remote NRPE host"));
		register_command(_T("nsca_submit"), _T("Submit (via query) remote NRPE host"));
		register_command(_T("nsca_forward"), _T("Forward query to remote NRPE host"));
		register_command(_T("nsca_exec"), _T("Execute (via query) remote NRPE host"));
		register_command(_T("nsca_help"), _T("Help on using NRPE Client"));

		if (hostname_ == "auto") {
			hostname_ = boost::asio::ip::host_name();
		} else {
			std::pair<std::string,std::string> dn = strEx::split<std::string>(boost::asio::ip::host_name(), ".");

			try {
				boost::asio::io_service svc;
				boost::asio::ip::tcp::resolver resolver (svc);
				boost::asio::ip::tcp::resolver::query query (boost::asio::ip::host_name(), "");
				boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve (query), end;

				std::string s;
				while (iter != end) {
					s += iter->host_name();
					s += " - ";
					s += iter->endpoint().address().to_string();
					iter++;
				}
			} catch (exception& e) {
				cerr << "Error: " << e.what() << endl;
			}


			strEx::replace(hostname_, "${host}", dn.first);
			strEx::replace(hostname_, "${domain}", dn.second);
		}
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

std::wstring NSCAAgent::getCryptos() {
	std::wstring ret = _T("{");
	for (int i=0;i<LAST_ENCRYPTION_ID;i++) {
		if (nsca::nsca_encrypt::hasEncryption(i)) {
			std::wstring name;
			try {
				boost::shared_ptr<nsca::nsca_encrypt::any_encryption> core(nsca::nsca_encrypt::get_encryption_core(i));
				if (core == NULL)
					name = _T("Broken<NULL>");
				else
					name = utf8::to_unicode(core->getName());
			} catch (nsca::nsca_encrypt::encryption_exception &e) {
				name = utf8::to_unicode(e.what());
			}
			if (ret.size() > 1)
				ret += _T(", ");
			ret += strEx::itos(i) + _T("=") + name;
		}
	}
	return ret + _T("}");
}

std::string get_command(std::string alias, std::string command = "") {
	if (!alias.empty())
		return alias; 
	if (!command.empty())
		return command; 
	return "host_check";
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void NSCAAgent::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void NSCAAgent::add_command(std::wstring name, std::wstring args) {
	try {
		std::wstring key = commands.add_command(name, args);
		if (!key.empty())
			register_command(key.c_str(), _T("NSCA relay for: ") + name);
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
bool NSCAAgent::unloadModule() {
	return true;
}

NSCAPI::nagiosReturn NSCAAgent::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::QueryRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_query(cmd, config, message, result);
}

NSCAPI::nagiosReturn NSCAAgent::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::ExecuteRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_exec(cmd, config, message, result);
}

NSCAPI::nagiosReturn NSCAAgent::handleRAWNotification(const wchar_t* channel, std::string request, std::string &result) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return client::command_line_parser::do_relay_submit(config, message, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void NSCAAgent::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("encryption,e", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "encryption", _1)), 
		"Length of payload (has to be same as on the server)")

		("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&nscapi::functions::destination_container::set_int_data, &data->recipient, "payload length", _1)), 
		"Length of payload (has to be same as on the server)")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")

		("password", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "password", _1)), 
		"Password")

		("source-host", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("sender-host", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->host_self, "host", _1)), 
		"Source/sender host name (default is auto which means use the name of the actual host)")

		("time-offset", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "time offset", _1)), 
		"")
		;
}

void NSCAAgent::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
	boost::shared_ptr<clp_handler_impl> handler(new clp_handler_impl(this));
	add_local_options(config.local, config.data);

	config.data->recipient.id = header.recipient_id();
	std::wstring recipient = utf8::cvt<std::wstring>(config.data->recipient.id);
	if (!targets.has_object(recipient)) {
		NSC_LOG_ERROR(_T("Target not found (using default): ") + recipient);
		recipient = _T("default");
	}
	nscapi::targets::optional_target_object opt = targets.find_object(recipient);

	if (opt) {
		nscapi::targets::target_object t = *opt;
		nscapi::functions::destination_container def = t.to_destination_container();
		config.data->recipient.apply(def);
	}
	config.data->host_self.id = "self";
	config.data->host_self.address.host = hostname_;

	config.target_lookup = handler;
	config.handler = handler;
}

NSCAAgent::connection_data NSCAAgent::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::functions::destination_container recipient, sender;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	nscapi::functions::parse_destination(header, header.sender_id(), sender, true);
	return connection_data(recipient, data->recipient, sender);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int NSCAAgent::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	Plugin::QueryResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_query_request(request_message.payload(i));
		packet.code = 0;
		packet.result = utf8::cvt<std::string>(data.command);
		list.push_back(packet);
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);

	nscapi::functions::append_simple_query_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()), "");
	response_message.SerializeToString(&reply);
	return NSCAPI::isSuccess;
}

int NSCAAgent::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::wstring channel = utf8::cvt<std::wstring>(request_message.channel());

	Plugin::SubmitResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;

	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		std::wstring alias, msg;
		packet.code = nscapi::functions::parse_simple_submit_request_payload(request_message.payload(i), alias, msg);
		if (alias != _T("host_check"))
			packet.service = utf8::cvt<std::string>(alias);
		packet.result = utf8::cvt<std::string>(msg);
		list.push_back(packet);
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	response_message.SerializeToString(&reply);
	return NSCAPI::isSuccess;
}

int NSCAAgent::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);

	Plugin::ExecuteResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<nsca::packet> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		nsca::packet packet(con.sender_hostname, con.buffer_length, con.time_delta);
		nscapi::functions::decoded_simple_command_data data = nscapi::functions::parse_simple_exec_request_payload(request_message.payload(i));
		packet.code = 0;
		if (data.command != _T("host_check"))
			packet.service = utf8::cvt<std::string>(data.command);
		//packet.result = data.;
		list.push_back(packet);
	}
	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::functions::append_simple_exec_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	response_message.SerializeToString(&reply);
	return NSCAPI::isSuccess;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> NSCAAgent::send(connection_data data, const std::list<nsca::packet> packets) {
	try {
		NSC_DEBUG_MSG_STD(_T("Connection details: ") + data.to_wstring());
		boost::asio::io_service io_service;
		nsca::socket socket(io_service);
		socket.connect(data.host, data.port);
		if (!socket.recv_iv(data.password, data.get_encryption(), boost::posix_time::seconds(data.timeout<5?30:data.timeout))) {
			NSC_LOG_ERROR_STD(_T("Failed to read iv"));
			return NSCAPI::hasFailed;
		}
		NSC_DEBUG_MSG_STD(_T("Got IV sending packets: ") + strEx::itos(packets.size()));
		BOOST_FOREACH(const nsca::packet &packet, packets) {
			NSC_DEBUG_MSG_STD(_T("Sending (data): ") + utf8::cvt<std::wstring>(packet.to_string()));
			socket.send_nsca(packet, boost::posix_time::seconds(data.timeout));
		}
		socket.shutdown();
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T(""));
	} catch (const nsca::nsca_encrypt::encryption_exception &e) {
		NSC_LOG_ERROR_STD(_T("NSCA Error: ") + utf8::to_unicode(e.what()));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("NSCA error: ") + utf8::to_unicode(e.what()));
	} catch (const std::runtime_error &e) {
		NSC_LOG_ERROR_STD(_T("Socket error: ") + utf8::to_unicode(e.what()));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Socket error: ") + utf8::to_unicode(e.what()));
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Error: ") + utf8::to_unicode(e.what()));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Error: ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown exception when sending NSCA data: "));
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCAAgent);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();

