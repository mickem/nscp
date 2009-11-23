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
#include "NRPEClient.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <msvc_wrappers.h>
//#include <execute_process.hpp>
#include <strEx.h>



NRPEClient gNRPEClient;

#ifdef WIN32
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}
#endif

NRPEClient::NRPEClient() : buffer_length_(0) {
}

NRPEClient::~NRPEClient() {
}

bool NRPEClient::loadModule(NSCAPI::moduleLoadMode mode) {
	std::list<std::wstring> commands;
	buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
	try {
		SETTINGS_REG_PATH(nrpe::CH_SECTION);
		commands = NSCModuleHelper::getSettingsSection(setting_keys::nrpe::CH_SECTION_PATH);
	} catch (NSCModuleHelper::NSCMHExcpetion &e) {
		NSC_LOG_ERROR_STD(_T("Failed to register command: ") + e.msg_);
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Failed to register command."));
	}
	for (std::list<std::wstring>::const_iterator it = commands.begin(); it != commands.end(); ++it) {
		NSC_DEBUG_MSG_STD(*it);
		std::wstring s = NSCModuleHelper::getSettingsString(setting_keys::nrpe::CH_SECTION_PATH, (*it), _T(""));
		if (s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid NRPE-client entry: ") + (*it));
		} else {
			addCommand((*it).c_str(), s);
		}
	}
	return true;
}

void NRPEClient::add_options(po::options_description &desc, nrpe_connection_data command_data) {
	desc.add_options()
		("help,h", "Show this help message.")
		("host,H", po::wvalue<std::wstring>(&command_data.host), "The address of the host running the NRPE daemon")
		("port,p", po::value<int>(&command_data.port), "The port on which the daemon is running (default=5666)")
		("command,c", po::wvalue<std::wstring>(&command_data.command), "The name of the command that the remote daemon should run")
		("timeout,t", po::value<int>(&command_data.timeout), "Number of seconds before connection times out (default=10)")
		("buffer-length,l", po::value<unsigned int>(&command_data.buffer_length), std::string("Length of payload (has to be same as on the server (default=" + to_string(buffer_length_) + ")").c_str())
		("no-ssl,n", po::value<bool>(&command_data.no_ssl)->zero_tokens()->default_value(false), "Do not initial an ssl handshake with the server, talk in plaintext.")
		("arguments,a", po::wvalue<std::vector<std::wstring> >(&command_data.argument_vector), "list of arguments")
		;
}


void NRPEClient::addCommand(strEx::blindstr key, std::wstring args) {
	try {

		NRPEClient::nrpe_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
		add_options(desc, command_data);

		po::positional_options_description p;
		p.add("arguments", -1);

		std::vector<std::wstring> list;
		//explicit escaped_list_separator(Char e = '\\', Char c = ',',Char q = '\"')
		boost::escaped_list_separator<wchar_t> sep(L'\\', L' ', L'\"');
		typedef boost::tokenizer<boost::escaped_list_separator<wchar_t>,std::wstring::const_iterator, std::wstring > tokenizer_t;
		tokenizer_t tok(args, sep);
		for(tokenizer_t::iterator beg=tok.begin(); beg!=tok.end();++beg){
			std::wcout << *beg << std::endl;
			list.push_back(*beg);
		}

		po::wparsed_options parsed = po::basic_command_line_parser<wchar_t>(list).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);
		command_data.parse_arguments();

		NSC_DEBUG_MSG_STD(_T("Added NRPE Client: ") + key.c_str() + _T(" = ") + command_data.toString());
		commands[key] = command_data;
	} catch (boost::program_options::validation_error &e) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str() + strEx::string_to_wstring(e.what()));
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Could not parse: ") + key.c_str());
	}
}

bool NRPEClient::unloadModule() {
	return true;
}

bool NRPEClient::hasCommandHandler() {
	return true;
}
bool NRPEClient::hasMessageHandler() {
	return false;
}
NSCAPI::nagiosReturn NRPEClient::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf)
{
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	std::wstring args = (*cit).second.arguments;
	if (SETTINGS_GET_BOOL(nrpe::ALLOW_ARGS) == 1) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (SETTINGS_GET_INT(nrpe::ALLOW_NASTY) == 0) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}

	NSC_DEBUG_MSG_STD(_T("Rewrote command arguments: ") + args);
	nrpe_result_data r = execute_nrpe_command((*cit).second, args);
	message = r.text;
	return r.result;
}

int NRPEClient::commandLineExec(const unsigned int argLen, TCHAR** args) {
	try {

		NRPEClient::nrpe_connection_data command_data;
		boost::program_options::variables_map vm;

		po::options_description desc("Allowed options");
		buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
		add_options(desc, command_data);

		po::positional_options_description p;
		p.add("arguments", -1);
		po::wparsed_options parsed = basic_command_line_parser_ex<wchar_t>(argLen, args).options(desc).positional(p).run();
		po::store(parsed, vm);
		po::notify(vm);
		command_data.parse_arguments();

		if (vm.count("help")) {
			std::cout << desc << "\n";
			return 1;
		}
		nrpe_result_data result = execute_nrpe_command(command_data, command_data.arguments);
		std::wcout << result.text << std::endl;
		return result.result;
	} catch (boost::program_options::validation_error &e) {
		std::cout << e.what() << std::endl;
	} catch (...) {
		std::cout << "Unknown exception parsing command line" << std::endl;
	}
	return NSCAPI::returnUNKNOWN;
}
NRPEClient::nrpe_result_data NRPEClient::execute_nrpe_command(nrpe_connection_data con, std::wstring arguments) {
	try {
		NRPEPacket packet;
		if (!con.no_ssl) {
#ifdef USE_SSL
			packet = send_ssl(con.host, con.port, con.timeout, NRPEPacket::make_request(con.get_cli(arguments), con.buffer_length));
#else
			return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("SSL support not available (compiled without USE_SSL)!"));
#endif
		} else
			packet = send_nossl(con.host, con.port, con.timeout, NRPEPacket::make_request(con.get_cli(arguments), con.buffer_length));
		return nrpe_result_data(packet.getResult(), packet.getPayload());
	} catch (NRPEPacket::NRPEPacketException &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("NRPE Packet errro: ") + e.getMessage());
	} catch (std::runtime_error &e) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Socket error: ") + boost::lexical_cast<std::wstring>(e.what()));
	} catch (...) {
		return nrpe_result_data(NSCAPI::returnUNKNOWN, _T("Unknown error -- REPORT THIS!"));
	}
}
/*
NRPEPacket NRPEClient::send_ssl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
#ifndef USE_SSL
	return send_nossl(host, port, timeout, packet);
#else
	
	simpleSSL::Socket socket(true);
	socket.connect(host, port);
	NSC_DEBUG_MSG_STD(_T(">>>length: ") + strEx::itos(packet.getBufferLength()));
	socket.sendAll(packet.getBuffer(), packet.getBufferLength());
	simpleSocket::DataBuffer buffer;
	socket.readAll(buffer, packet.getBufferLength());
	NSC_DEBUG_MSG_STD(_T("<<<length: ") + strEx::itos(buffer.getLength()));
	packet.readFrom(buffer.getBuffer(), buffer.getLength());
	return packet;
	

	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	ctx.use_tmp_dh_file("d:\\nrpe_512.pem");
	ctx.set_verify_mode(boost::asio::ssl::context::verify_peer);
	nrpe_ssl_socket socket(io_service, ctx, host, port);
	//socket.

#endif
}
*/
using boost::asio::ip::tcp;

void set_result(boost::optional<boost::system::error_code>* a, boost::system::error_code b)
{
	a->reset(b);
} 
template <typename AsyncReadStream, typename RawSocket, typename MutableBufferSequence>
void read_with_timeout(AsyncReadStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration)
{
	boost::optional<boost::system::error_code> timer_result;
	boost::asio::deadline_timer timer(sock.io_service());
	timer.expires_from_now(duration);
	timer.async_wait(boost::bind(set_result, &timer_result, _1));

	boost::optional<boost::system::error_code> read_result;
	async_read(sock, buffers, boost::bind(set_result, &read_result, _1));

	sock.io_service().reset();
	while (sock.io_service().run_one())
	{
		if (read_result)
			timer.cancel();
		else if (timer_result)
			rawSocket.close();
	}

	if (*read_result)
		throw boost::system::system_error(*read_result);
} 

template <typename AsyncWriteStream, typename RawSocket, typename MutableBufferSequence>
void write_with_timeout(AsyncWriteStream& sock, RawSocket& rawSocket, const MutableBufferSequence& buffers, boost::posix_time::time_duration duration)
{
	boost::optional<boost::system::error_code> timer_result;
	boost::asio::deadline_timer timer(sock.io_service());
	timer.expires_from_now(duration);
	timer.async_wait(boost::bind(set_result, &timer_result, _1));

	boost::optional<boost::system::error_code> read_result;
	async_write(sock, buffers, boost::bind(set_result, &read_result, _1));

	sock.io_service().reset();
	while (sock.io_service().run_one())
	{
		if (read_result)
			timer.cancel();
		else if (timer_result)
			rawSocket.close();
	}

	if (*read_result)
		throw boost::system::system_error(*read_result);
}

class nrpe_socket : public boost::noncopyable {
public:
	tcp::socket socket_;

public:
	nrpe_socket(boost::asio::io_service &io_service, std::wstring host, int port) : socket_(io_service) {
		NSC_LOG_CRITICAL(_T("Looking up..."));
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(to_string(host), to_string(port));
		//tcp::resolver::query query("www.medin.name", "80");
		//tcp::resolver::query query("test_server", "80");

		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		NSC_LOG_CRITICAL(_T("connecting..."));
		boost::system::error_code error = boost::asio::error::host_not_found;
		while (error && endpoint_iterator != end)
		{
			tcp::resolver::endpoint_type ep = *endpoint_iterator;
			NSC_DEBUG_MSG_STD(_T("Attempting to connect to: ") + to_wstring(ep.address().to_string()) + _T(":") + to_wstring(ep.port()));
			socket_.close();
			socket_.connect(*endpoint_iterator++, error);
		}
		NSC_LOG_CRITICAL(_T("Connected..."));
		if (error)
			throw boost::system::system_error(error);
	}
	~nrpe_socket() {
		socket_.close();
	}

	void send(NRPEPacket &packet, boost::posix_time::seconds timeout) {
		std::vector<char> buf(packet.getBufferLength());
		write_with_timeout(socket_, socket_, boost::asio::buffer(packet.getBuffer(), packet.getBufferLength()), timeout);
	}
	NRPEPacket recv(const NRPEPacket &packet, boost::posix_time::seconds timeout) {
		std::vector<char> buf(packet.getBufferLength());
		read_with_timeout(socket_, socket_, boost::asio::buffer(buf), timeout);
		return NRPEPacket(&buf[0], buf.size(), packet.getInternalBufferLength());
	}
};
#ifdef USE_SSL

class nrpe_ssl_socket {

private:
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
public:
	nrpe_ssl_socket(boost::asio::io_service &io_service, boost::asio::ssl::context &ctx, std::wstring host, int port) : socket_(io_service, ctx) {
		NSC_LOG_CRITICAL(_T("Looking up..."));
		tcp::resolver resolver(io_service);
		//tcp::resolver::query query(to_string(host), to_string(port));
		tcp::resolver::query query("www.medin.name", "80");
		//tcp::resolver::query query("test_server", "80");

		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;

		boost::system::error_code error = boost::asio::error::host_not_found;
		NSC_LOG_CRITICAL(_T("Connecting..."));
		while (error && endpoint_iterator != end)
		{
			tcp::resolver::endpoint_type ep = *endpoint_iterator;
			NSC_DEBUG_MSG_STD(_T("Attempting to connect to: ") + to_wstring(ep.address().to_string()) + _T(":") + to_wstring(ep.port()));
			socket_.lowest_layer().close();
			socket_.lowest_layer().connect(*endpoint_iterator++, error);
		}
		if (error)
			throw boost::system::system_error(error);
		NSC_LOG_CRITICAL(_T("Connected..."));

		NSC_LOG_CRITICAL(_T("Handshaking..."));
		//socket_.handshake(boost::asio::ssl::stream_base::client);
		socket_.handshake(boost::asio::ssl::stream_base::client);
		NSC_LOG_CRITICAL(_T("Handshook...") + strEx::itos(error.value()));

	}

	void send(NRPEPacket &packet, boost::posix_time::seconds timeout) {
		NSC_LOG_CRITICAL(_T("Writing..."));
		std::vector<char> buf(packet.getBufferLength());
		write_with_timeout(socket_, socket_.lowest_layer(), boost::asio::buffer(packet.getBuffer(), packet.getBufferLength()), timeout);
		NSC_LOG_CRITICAL(_T("Written..."));
	}
	NRPEPacket recv(const NRPEPacket &packet, boost::posix_time::seconds timeout) {
		NSC_LOG_CRITICAL(_T("Reading..."));
		std::vector<char> buf(packet.getBufferLength());
		read_with_timeout(socket_, socket_.lowest_layer(), boost::asio::buffer(buf), timeout);
		return NRPEPacket(&buf[0], buf.size(), packet.getInternalBufferLength());
		NSC_LOG_CRITICAL(_T("Read..."));
	}
};
NRPEPacket NRPEClient::send_ssl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	boost::asio::io_service io_service;
	boost::asio::ssl::context ctx(io_service, boost::asio::ssl::context::sslv23);
	SSL_CTX_set_cipher_list(ctx.impl(), "ADH");
	ctx.use_tmp_dh_file("d:\\nrpe_512.pem");
	ctx.set_verify_mode(boost::asio::ssl::context::verify_none);
	nrpe_ssl_socket socket(io_service, ctx, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}
#endif

NRPEPacket NRPEClient::send_nossl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	boost::asio::io_service io_service;
	nrpe_socket socket(io_service, host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}

/*
NRPEPacket NRPEClient::send_nossl(std::wstring host, int port, int timeout, NRPEPacket packet)
{
	unsigned char dh512_p[] = {
		0xCF, 0xFF, 0x65, 0xC2, 0xC8, 0xB4, 0xD2, 0x68, 0x8C, 0xC1, 0x80, 0xB1,
		0x7B, 0xD6, 0xE8, 0xB3, 0x62, 0x59, 0x62, 0xED, 0xA7, 0x45, 0x6A, 0xF8,
		0xE9, 0xD8, 0xBE, 0x3F, 0x38, 0x42, 0x5F, 0xB2, 0xA5, 0x36, 0x03, 0xD3,
		0x06, 0x27, 0x81, 0xC8, 0x9B, 0x88, 0x50, 0x3B, 0x82, 0x3D, 0x31, 0x45,
		0x2C, 0xB4, 0xC5, 0xA5, 0xBE, 0x6A, 0xE3, 0x2E, 0xA6, 0x86, 0xFD, 0x6A,
		0x7E, 0x1E, 0x6A, 0x73,
	};
	unsigned char dh512_g[] = { 0x02, };

	DH *dh_2 = DH_new();
	dh_2->p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
	dh_2->g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);

	FILE *outfile = fopen("d:\\nrpe_512.pem", "w");
	PEM_write_DHparams(outfile, dh_2);
	PEM_write_DHparams(stdout, dh_2);
	fclose(outfile);

	nrpe_socket socket(host, port);
	socket.send(packet, boost::posix_time::seconds(timeout));
	return socket.recv(packet, boost::posix_time::seconds(timeout));
}
*/





NSC_WRAPPERS_MAIN_DEF(gNRPEClient);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEClient);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNRPEClient);
NSC_WRAPPERS_CLI_DEF(gNRPEClient);


MODULE_SETTINGS_START(NRPEClient, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NRPEClient.dll will listen to.")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("port")) 
OPTION(_T("default"), _T("5666")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_arguments"), _T("This option determines whether or not the NRPE daemon will allow clients to specify arguments to commands that are executed.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_arguments")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("allow_nasty_meta_chars"), _T("This might have security implications (depending on what you do with the options)")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allow_nasty_meta_chars")) 
OPTION(_T("default"), _T("false")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

ITEM_CHECK_BOOL(_T("use_ssl"), _T("This option will enable SSL encryption on the NRPE data socket (this increases security somwhat.")) 
ITEM_MAP_TO(_T("basic_ini_bool_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("use_ssl")) 
OPTION(_T("default"), _T("true")) 
OPTION(_T("true_value"), _T("1")) 
OPTION(_T("false_value"), _T("0")) 
ITEM_END()

PAGE_END()
ADVANCED_PAGE(_T("Access configuration")) 

ITEM_EDIT_OPTIONAL_LIST(_T("Allow connection from:"), _T("This is the hosts that will be allowed to poll performance data from the NRPE server.")) 
OPTION(_T("disabledCaption"), _T("Use global settings (defined previously)")) 
OPTION(_T("enabledCaption"), _T("Specify hosts for NRPE server")) 
OPTION(_T("listCaption"), _T("Add all IP addresses (not hosts) which should be able to connect:")) 
OPTION(_T("separator"), _T(",")) 
OPTION(_T("disabled"), _T("")) 
ITEM_MAP_TO(_T("basic_ini_text_mapper")) 
OPTION(_T("section"), _T("NRPE")) 
OPTION(_T("key"), _T("allowed_hosts")) 
OPTION(_T("default"), _T("")) 
ITEM_END()

PAGE_END()
MODULE_SETTINGS_END()
