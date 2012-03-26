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
#include "GraphiteClient.h"

#include <utils.h>
#include <strEx.h>

#include <settings/client/settings_client.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

namespace sh = nscapi::settings_helper;

const std::wstring GraphiteClient::command_prefix = _T("graphite");
/**
 * Default c-tor
 * @return 
 */
GraphiteClient::GraphiteClient() {}

/**
 * Default d-tor
 * @return 
 */
GraphiteClient::~GraphiteClient() {}

/**
 * Load (initiate) module.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool GraphiteClient::loadModule() {
	return false;
}

bool GraphiteClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {

	try {

		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("graphite"), alias, _T("client"));
		target_path = settings.alias().get_settings_path(_T("targets"));

		settings.alias().add_path_to_settings()
			(_T("GRAPHITE CLIENT SECTION"), _T("Section for graphite passive check module."))

			(_T("handlers"), sh::fun_values_path(boost::bind(&GraphiteClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("targets"), sh::fun_values_path(boost::bind(&GraphiteClient::add_target, this, _1, _2)), 
			_T("REMOTE TARGET DEFINITIONS"), _T(""))
			;

		settings.alias().add_key_to_settings()
			(_T("hostname"), sh::string_key(&hostname_, "auto"),
			_T("HOSTNAME"), _T("The host name of this host if set to blank (default) the windows name of the computer will be used."))

			(_T("channel"), sh::wstring_key(&channel_, _T("GRAPHITE")),
			_T("CHANNEL"), _T("The channel to listen to."))
			;

		settings.register_all();
		settings.notify();

		targets.add_missing(get_settings_proxy(), target_path, _T("default"), _T(""), true);


		get_core()->registerSubmissionListener(get_id(), channel_);

		register_command(command_prefix + _T("_query"), _T("QUery remote host"));
		register_command(command_prefix + _T("_submit"), _T("Submit to remote host"));
		register_command(command_prefix + _T("_forward"), _T("Forward query to remote host"));
		register_command(command_prefix + _T("_exec"), _T("Execute command on remote host"));
		register_command(command_prefix + _T("_help"), _T("Help"));

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
			} catch (const std::exception& e) {
				NSC_LOG_ERROR_STD(_T("Failed to resolve: ") + utf8::to_unicode(e.what()));
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

void GraphiteClient::add_target(std::wstring key, std::wstring arg) {
	try {
		targets.add(get_settings_proxy(), target_path , key, arg);
	} catch (const std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key + _T(", ") + utf8::to_unicode(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to add target: ") + key);
	}
}

void GraphiteClient::add_command(std::wstring name, std::wstring args) {
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
bool GraphiteClient::unloadModule() {
	return true;
}

NSCAPI::nagiosReturn GraphiteClient::handleRAWCommand(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::QueryRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_query(cmd, config, message, result);
}

NSCAPI::nagiosReturn GraphiteClient::commandRAWLineExec(const wchar_t* char_command, const std::string &request, std::string &result) {
	std::wstring cmd = client::command_line_parser::parse_command(char_command, command_prefix);

	Plugin::ExecuteRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return commands.process_exec(cmd, config, message, result);
}

NSCAPI::nagiosReturn GraphiteClient::handleRAWNotification(const wchar_t* channel, std::string request, std::string &result) {
	Plugin::SubmitRequestMessage message;
	message.ParseFromString(request);

	client::configuration config;
	setup(config, message.header());

	return client::command_line_parser::do_relay_submit(config, message, result);
}

//////////////////////////////////////////////////////////////////////////
// Parser setup/Helpers
//

void GraphiteClient::add_local_options(po::options_description &desc, client::configuration::data_type data) {
	desc.add_options()
		("path", po::value<std::string>()->notifier(boost::bind(&nscapi::functions::destination_container::set_string_data, &data->recipient, "path", _1)), 
		"")

		("timeout", po::value<unsigned int>()->notifier(boost::bind(&nscapi::functions::destination_container::set_int_data, &data->recipient, "timeout", _1)), 
		"")

		;
}

void GraphiteClient::setup(client::configuration &config, const ::Plugin::Common_Header& header) {
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

GraphiteClient::connection_data GraphiteClient::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
	nscapi::functions::destination_container recipient, sender;
	nscapi::functions::parse_destination(header, header.recipient_id(), recipient, true);
	nscapi::functions::parse_destination(header, header.sender_id(), sender, true);
	return connection_data(recipient, data->recipient, sender);
}

//////////////////////////////////////////////////////////////////////////
// Parser implementations
//

int GraphiteClient::clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("GRAPHITE does not support query patterns"));
	nscapi::functions::create_simple_query_response_unknown(_T("UNKNOWN"), _T("GRAPHITE does not support query patterns"), reply);
	return NSCAPI::hasFailed;
}

int GraphiteClient::clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, std::string &reply) {
	const ::Plugin::Common_Header& request_header = request_message.header();
	connection_data con = parse_header(request_header, data);
	std::string path = con.path;

	strEx::replace(path, "${hostname}", con.sender_hostname);

	Plugin::SubmitResponseMessage response_message;
	nscapi::functions::make_return_header(response_message.mutable_header(), request_header);

	std::list<g_data> list;
	for (int i=0;i < request_message.payload_size(); ++i) {
		Plugin::QueryResponseMessage::Response r =request_message.payload(i);
		std::string tmp_path = path;
		strEx::replace(tmp_path, "${check_alias}", r.alias());

		for (int j=0;j<r.perf_size();j++) {
			g_data d;
			::Plugin::Common::PerformanceData perf = r.perf(j);
			double value;
			d.path = tmp_path;
			strEx::replace(d.path, "${perf_alias}", perf.alias());
			if (perf.has_float_value()) {
				if (perf.float_value().has_value())
					value = perf.float_value().value();
				else
					NSC_LOG_ERROR(_T("Unsopported performance data (no value)"));
			} else if (perf.has_int_value()) {
				if (perf.int_value().has_value())
					value = perf.int_value().value();
				else
					NSC_LOG_ERROR(_T("Unsopported performance data (no value)"));
			} else {
				NSC_LOG_ERROR(_T("Unsopported performance data type: ") + utf8::cvt<std::wstring>(perf.alias()));
				continue;
			}
			strEx::replace(d.path, " ", "_");
			d.value = strEx::s::itos(value);
			list.push_back(d);
		}
	}

	boost::tuple<int,std::wstring> ret = instance->send(con, list);
	nscapi::functions::append_simple_submit_response_payload(response_message.add_payload(), "TODO", ret.get<0>(), utf8::cvt<std::string>(ret.get<1>()));
	response_message.SerializeToString(&reply);
	return NSCAPI::isSuccess;
}

int GraphiteClient::clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, std::string &reply) {
	NSC_LOG_ERROR_STD(_T("GRAPHITE does not support exec patterns"));
	nscapi::functions::create_simple_exec_response_unknown("UNKNOWN", "GRAPHITE does not support exec patterns", reply);
	return NSCAPI::hasFailed;
}

//////////////////////////////////////////////////////////////////////////
// Protocol implementations
//

boost::tuple<int,std::wstring> GraphiteClient::send(connection_data data, const std::list<g_data> payload) {
	try {
		NSC_DEBUG_MSG_STD(_T("Connection details: ") + data.to_wstring());
		
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver(io_service);
		boost::asio::ip::tcp::resolver::query query(data.host, data.port);
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		boost::asio::ip::tcp::resolver::iterator end;

		boost::asio::ip::tcp::socket socket(io_service);
		boost::system::error_code error = boost::asio::error::host_not_found;
		while(error && endpoint_iterator != end) {
			socket.close();
			socket.connect(*endpoint_iterator++, error);
		}
		if(error)
			throw boost::system::system_error(error);

		boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1)); 
		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration diff = now - time_t_epoch;
		int x = diff.total_milliseconds();

		BOOST_FOREACH(const g_data &d, payload) {
			std::string msg = d.path + " " +d.value + " " + boost::lexical_cast<std::string>(x) + "\n";
			NSC_DEBUG_MSG_STD(_T("Sending: ") + utf8::cvt<std::wstring>(msg));
			socket.send(boost::asio::buffer(msg));
		}
		//socket.shutdown();
		return boost::make_tuple(NSCAPI::returnUNKNOWN, _T(""));
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
NSC_WRAPPERS_MAIN_DEF(GraphiteClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();
NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF();

