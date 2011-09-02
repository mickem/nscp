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
#include "NSCPClient.h"
#include <time.h>
#include <boost/filesystem.hpp>

#include <nscapi/functions.hpp>
#include <config.h>
#include <strEx.h>
#include <nscp/client/socket.hpp>

#include <protobuf/plugin.pb.h>

#include <settings/client/settings_client.hpp>


namespace sh = nscapi::settings_helper;

NSCPClient::NSCPClient() : buffer_length_(0) {
}

NSCPClient::~NSCPClient() {
}

bool NSCPClient::loadModule() {
	return false;
}

bool NSCPClient::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	std::map<std::wstring,std::wstring> commands;

	try {

		register_command(_T("query_nscp"), _T("Submit a query to a remote host via NSCP"));
		register_command(_T("submit_nscp"), _T("Submit a query to a remote host via NSCP"));
		//"/settings/NSCP/client/handlers"
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("NSCP"), alias, _T("client"));

		settings.alias().add_path_to_settings()

			(_T("handlers"), sh::fun_values_path(boost::bind(&NSCPClient::add_command, this, _1, _2)), 
			_T("CLIENT HANDLER SECTION"), _T(""))

			(_T("servers"), sh::fun_values_path(boost::bind(&NSCPClient::add_server, this, _1, _2)), 
			_T("REMOTE SERVER DEFINITIONS"), _T(""))

			;

		settings.alias().add_key_to_settings()

			(_T("payload length"),  sh::uint_key(&buffer_length_, 1024),
			_T("PAYLOAD LENGTH"), _T("Length of payload to/from the NSCP agent. This is a hard specific value so you have to \"configure\" (read recompile) your NSCP agent to use the same value for it to work."))

			;


		settings.register_all();
		settings.notify();

	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}

	boost::filesystem::wpath p = GET_CORE()->getBasePath() + std::wstring(_T("/security/nrpe_dh_512.pem"));
	cert_ = p.string();
	if (boost::filesystem::is_regular(p)) {
		NSC_DEBUG_MSG_STD(_T("Using certificate: ") + cert_);
	} else {
		NSC_LOG_ERROR_STD(_T("Certificate not found: ") + cert_);
	}

	return true;
}

void NSCPClient::add_common_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("host,H", po::wvalue<std::wstring>(&command_data.host), "The address of the host running the NSCP daemon")
		("port,p", po::value<int>(&command_data.port), "The port on which the daemon is running (default=5668)")
		("timeout,t", po::value<int>(&command_data.timeout), "Number of seconds before connection times out (default=10)")
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plain text.")
		("query,q", po::bool_switch(&command_data.query), "Force query mode (only useful when this is not obvious)")
		("submit,s", po::bool_switch(&command_data.submit), "Force submit mode (only useful when this is not obvious)")
		;
}
void NSCPClient::add_query_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the query that the remote daemon should run")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.arguments), "list of arguments")
		;
}
void NSCPClient::add_submit_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("message,m", po::wvalue<std::wstring>(&command_data.message), "Message")
		("result,r", po::value<unsigned int>(&command_data.result), "Result code")
		;
}
void NSCPClient::add_exec_options(po::options_description &desc, nscp_connection_data &command_data) {
	desc.add_options()
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.arguments), "list of arguments")
		;
}

void NSCPClient::add_server(std::wstring key, std::wstring args) {
}

void NSCPClient::add_command(std::wstring key, std::wstring args) {
	try {

		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description query("Query options");
		add_query_options(query, command_data);
		po::options_description submit("Submit options");
		add_submit_options(submit, command_data);

		po::positional_options_description p;
		p.add("arguments", -1);

		std::vector<std::wstring> list;
		//explicit escaped_list_separator(Char e = '\\', Char c = ',',Char q = '\"')
		boost::escaped_list_separator<wchar_t> sep(L'\\', L' ', L'\"');
		typedef boost::tokenizer<boost::escaped_list_separator<wchar_t>,std::wstring::const_iterator, std::wstring > tokenizer_t;
		tokenizer_t tok(args, sep);
		for(tokenizer_t::iterator beg=tok.begin(); beg!=tok.end();++beg){
			list.push_back(*beg);
		}

		po::options_description desc("Availible options");
		desc.add(common).add(query).add(submit);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(list).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		NSC_DEBUG_MSG_STD(_T("Added NSCP Client: ") + key.c_str() + _T(" = ") + command_data.toString());
		commands[key.c_str()] = command_data;

		register_command(key.c_str(), command_data.toString());

	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str());
	}
}

bool NSCPClient::unloadModule() {
	return true;
}

bool NSCPClient::hasCommandHandler() {
	return true;
}
bool NSCPClient::hasMessageHandler() {
	return false;
}


std::list<std::string> collect_result(std::list<std::string> payloads, int &ret) {
	std::list<std::string> result;
	ret = NSCAPI::returnOK;
	BOOST_FOREACH(std::string p, payloads) {
		Plugin::QueryResponseMessage message;
		message.ParseFromString(p);
		for (int i=0;i<message.payload_size();i++) {
			const Plugin::QueryResponseMessage::Response &payload = message.payload(i);
			ret = nscapi::plugin_helper::maxState(ret, nscapi::functions::gbp_to_nagios_status(payload.result()));
			std::string line = payload.message();

			// @todo: Add performance data parsing here!
			//nscapi::functions::parse_performance_data(payload, perf);
			//if (!payload.perf().empty())
			//	line += _T("|") + payload;
			result.push_back(line);
		}
	}
	return result;
}


NSCAPI::nagiosReturn NSCPClient::handleCommand(const std::wstring &target, const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &message, std::wstring &perf) {
	if (command == _T("query_nscp")) {
		return query_nscp(arguments, message, perf);
	}
	if (command == _T("submit_nscp")) {
		return query_nscp(arguments, message, perf);
	}
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	std::string buffer;
	nscapi::functions::create_simple_query_request((*cit).second.command, arguments, buffer);
	std::list<std::string> payloads = execute_nscp_command((*cit).second, buffer);
	int ret = NSCAPI::returnUNKNOWN;
	std::list<std::string> result = collect_result(payloads, ret);
	BOOST_FOREACH(std::string p, result) {
		message += utf8::cvt<std::wstring>(p) + _T("\n");
	}
	return ret;
}


NSCAPI::nagiosReturn NSCPClient::query_nscp(std::list<std::wstring> &arguments, std::wstring &message, std::wstring perf) {
	try {
		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description query("Query options");
		add_query_options(query, command_data);

		po::options_description desc("Allowed options");
		desc.add(common).add(query);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);


		std::string buffer;
		nscapi::functions::create_simple_query_request(command_data.command, arguments, buffer);
		std::list<std::string> payloads = execute_nscp_command(command_data, buffer);
		int ret = NSCAPI::returnUNKNOWN;
		std::list<std::string> result = collect_result(payloads, ret);
		BOOST_FOREACH(std::string p, result) {
			message += utf8::cvt<std::wstring>(p) + _T("\n");
		}
		return ret;
	} catch (boost::program_options::validation_error &e) {
		message = _T("Error: ") + utf8::cvt<std::wstring>(e.what());
		return NSCAPI::returnUNKNOWN;
	} catch (...) {
		message = _T("Unknown exception parsing command line");
		return NSCAPI::returnUNKNOWN;
	}
	return NSCAPI::returnUNKNOWN;
}

bool NSCPClient::submit_nscp(std::list<std::wstring> &arguments, std::wstring &result) {
	try {
		NSCPClient::nscp_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description submit("Submit options");
		add_submit_options(submit, command_data);

		po::options_description desc("Allowed options");
		desc.add(common).add(submit);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);


		std::string buffer;
		nscapi::functions::create_simple_query_response(command_data.command, command_data.result, command_data.message, _T(""), buffer);
		std::list<std::string> errors = submit_nscp_command(command_data, buffer);
		
		BOOST_FOREACH(std::string e, errors) {
			result += utf8::cvt<std::wstring>(e) + _T("\n");
		}
	} catch (boost::program_options::validation_error &e) {
		result = _T("Error: ") + utf8::cvt<std::wstring>(e.what());
		return false;
	} catch (...) {
		result = _T("Unknown exception parsing command line");
		return false;
	}
	return true;
}


int NSCPClient::commandLineExec(const std::wstring &command, std::list<std::wstring> &arguments, std::wstring &result) {
	NSCPClient::nscp_connection_data command_data;
	if (command == _T("help")) {
		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description query("Query NSCP options");
		add_query_options(query, command_data);
		po::options_description submit("Submit NSCP options");
		add_submit_options(submit, command_data);
		po::options_description exec("Execute NSCP options");
		add_exec_options(exec, command_data);
		po::options_description desc("Allowed options");
		desc.add(common).add(query).add(submit);

		std::stringstream ss;
		ss << "NSCPClient Command line syntax for command: query_nscp and submit_nscp" << std::endl;;
		ss << desc;
		result = utf8::cvt<std::wstring>(ss.str());
		return NSCAPI::returnOK;
	} else if (command == _T("query_nscp")) {
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description query("Query NSCP options");
		add_query_options(query, command_data);
		po::options_description desc("Allowed options");
		desc.add(common).add(query);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		std::string buffer;
		nscapi::functions::create_simple_query_request(command_data.command, command_data.arguments, buffer);
		std::list<std::string> payloads = execute_nscp_query(command_data, buffer);
		int ret = NSCAPI::returnUNKNOWN;
		std::list<std::string> strings = collect_result(payloads, ret);
		BOOST_FOREACH(std::string p, strings) {
			result += utf8::cvt<std::wstring>(p) + _T("\n");
		}
		return ret;
	} else if (command == _T("exec_nscp")) {
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description query("Query NSCP options");
		add_exec_options(query, command_data);
		po::options_description desc("Allowed options");
		desc.add(common).add(query);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		std::string buffer;
		nscapi::functions::create_simple_exec_request(command_data.command, command_data.arguments, buffer);
		std::list<std::string> payloads = execute_nscp_command(command_data, buffer);
		int ret = NSCAPI::returnUNKNOWN;
		std::list<std::string> strings = collect_result(payloads, ret);
		BOOST_FOREACH(std::string p, strings) {
			result += utf8::cvt<std::wstring>(p) + _T("\n");
		}
		return ret;
	} else if (command == _T("submit_nscp")) {
		boost::program_options::variables_map vm;

		po::options_description common("Common options");
		add_common_options(common, command_data);
		po::options_description submit("Submit  NSCP options");
		add_submit_options(submit, command_data);
		po::options_description desc("Allowed options");
		desc.add(common).add(submit);

		std::vector<std::wstring> vargs(arguments.begin(), arguments.end());
		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(vargs).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);

		std::string buffer;
		nscapi::functions::create_simple_query_response(command_data.command, command_data.result, command_data.message, _T(""), buffer);
		std::list<std::string> errors = submit_nscp_command(command_data, buffer);
		BOOST_FOREACH(std::string p, errors) {
			result += utf8::cvt<std::wstring>(p) + _T("\n");
		}
		return NSCAPI::returnOK;
	}
	return NSCAPI::returnIgnored;
}
std::list<std::string> NSCPClient::execute_nscp_command(nscp_connection_data con, std::string buffer) {
	std::list<std::string> result;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::packet::build_envelope_request(1));
		chunks.push_back(nscp::packet::create_payload(nscp::data::command_request, buffer, 0));
		chunks = send(con, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (chunk.is_query_response()) {
				result.push_back(chunk.payload);
			} else if (chunk.is_error()) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			}
			//NSC_DEBUG_MSG_STD(_T("Found chunk: ") + utf8::cvt<std::wstring>(strEx::format_buffer(chunk.payload.c_str(), chunk.payload.size())));
		}
		return result;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return result;
	}
}
std::list<std::string> NSCPClient::execute_nscp_query(nscp_connection_data con, std::string buffer) {
	std::list<std::string> result;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::packet::build_envelope_request(1));
		chunks.push_back(nscp::packet::create_payload(nscp::data::exec_request, buffer, 0));
		chunks = send(con, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (chunk.is_exec_response()) {
				result.push_back(chunk.payload);
			} else if (chunk.is_error()) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					NSC_LOG_ERROR_STD(_T("Error: ") + utf8::cvt<std::wstring>(message.error(i).message()));
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			}
			//NSC_DEBUG_MSG_STD(_T("Found chunk: ") + utf8::cvt<std::wstring>(strEx::format_buffer(chunk.payload.c_str(), chunk.payload.size())));
		}
		return result;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return result;
	}
}

std::list<std::string> NSCPClient::submit_nscp_command(nscp_connection_data con, std::string buffer) {
	std::list<std::string> result;
	try {
		std::list<nscp::packet> chunks;
		chunks.push_back(nscp::packet::create_payload(nscp::data::command_response, buffer, 0));
		chunks = send(con, chunks);
		BOOST_FOREACH(nscp::packet &chunk, chunks) {
			if (chunk.is_query_response()) {
				result.push_back(chunk.payload);
			} else if (chunk.is_error()) {
				NSCPIPC::ErrorMessage message;
				message.ParseFromString(chunk.payload);
				for (int i=0;i<message.error_size();i++) {
					result.push_back("Error: " + message.error(i).message());
				}
			} else {
				NSC_LOG_ERROR_STD(_T("Unsupported message type: ") + strEx::itos(chunk.signature.payload_type));
			}
			//NSC_DEBUG_MSG_STD(_T("Found chunk: ") + utf8::cvt<std::wstring>(strEx::format_buffer(chunk.payload.c_str(), chunk.payload.size())));
		}
		return result;
	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception: ") + utf8::cvt<std::wstring>(e.what()));
		return result;
	}
}

std::list<nscp::packet> NSCPClient::send(nscp_connection_data &con, std::list<nscp::packet> &chunks) {
	chunks.push_front(nscp::packet::build_envelope_request(1));
	std::list<nscp::packet> tmp, result;
	if (!con.no_ssl) {
#ifdef USE_SSL
		tmp = send_ssl(con.host, con.port, con.timeout, chunks);
#else
		NSC_LOG_ERROR_STD(_T("SSL not avalible (not compiled with USE_SSL)"));
		result.push_back(nscp::packet::create_error(_T("SSL support not available (compiled without USE_SSL)!")));
#endif
	} else {
		tmp = send_nossl(con.host, con.port, con.timeout, chunks);
	}
	BOOST_FOREACH(nscp::packet &p, tmp) {
		if (p.is_envelope_response()) {
			std::cout << "Got envelope" << std::endl;
		} else {
			result.push_back(p);
		}
	}
	return result;
}

#ifdef USE_SSL
std::list<nscp::packet> NSCPClient::send_ssl(std::wstring host, int port, int timeout, const std::list<nscp::packet> &chunks) {
	NSC_DEBUG_MSG_STD(_T("Connecting to: ") + host + _T(":") + strEx::itos(port));
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file(to_string(cert_));
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nscp::client::ssl_socket socket(io_service, ctx, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}
#endif

std::list<nscp::packet> NSCPClient::send_nossl(std::wstring host, int port, int timeout, const std::list<nscp::packet> &chunks) {
	boost::asio::io_service io_service;
	nscp::client::socket socket(io_service, host, port);
	socket.send(chunks, boost::posix_time::seconds(timeout));
	return socket.recv(boost::posix_time::seconds(timeout));
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCPClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF();
NSC_WRAPPERS_CLI_DEF();

