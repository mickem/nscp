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
#include "NRPEListener.h"
#include <strEx.h>
#include <time.h>
#include <config.h>
#include <msvc_wrappers.h>

NRPEListener gNRPEListener;

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	NSCModuleWrapper::wrapDllMain(hModule, ul_reason_for_call);
	return TRUE;
}

NRPEListener::NRPEListener() : noPerfData_(false), buffer_length_(0) {
}
NRPEListener::~NRPEListener() {
}

std::wstring getAllowedHosts() {
	return SETTINGS_GET_STRING_FALLBACK(nrpe::ALLOWED_HOSTS, protocol_def::ALLOWED_HOSTS);
}
bool getCacheAllowedHosts() {
	return SETTINGS_GET_BOOL_FALLBACK(nrpe::CACHE_ALLOWED, protocol_def::CACHE_ALLOWED);
}


void NRPEListener::addAllScriptsFrom(std::wstring path) {
	std::wstring baseDir;
	std::wstring::size_type pos = path.find_last_of('*');
	if (pos == std::wstring::npos) {
		path += _T("*.*");
	}
	WIN32_FIND_DATA wfd;
	HANDLE hFind = FindFirstFile(path.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if ((wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
				addCommand(script_dir, wfd.cFileName);
			}
		} while (FindNextFile(hFind, &wfd));
	} else {
		NSC_LOG_ERROR_STD(_T("No scripts found in path: ") + path);
		return;
	}
	FindClose(hFind);
}

bool NRPEListener::loadModule(NSCAPI::moduleLoadMode mode) {
#ifdef USE_SSL
		
#else
	if (NSCModuleHelper::getSettingsInt(NRPE_SECTION_TITLE, NRPE_SETTINGS_USE_SSL ,NRPE_SETTINGS_USE_SSL_DEFAULT)==1) {
		NSC_LOG_ERROR_STD(_T("SSL not avalible! (not compiled with openssl support)"));
	}
#endif
	SETTINGS_REG_KEY_I(nrpe::PORT);
	SETTINGS_REG_KEY_S(nrpe::BINDADDR);
	SETTINGS_REG_KEY_I(nrpe::LISTENQUE);
	SETTINGS_REG_KEY_I(nrpe::READ_TIMEOUT);
	SETTINGS_REG_KEY_B(nrpe::KEYUSE_SSL);
	SETTINGS_REG_KEY_I(nrpe::PAYLOAD_LENGTH);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_PERFDATA);
	SETTINGS_REG_KEY_S(nrpe::SCRIPT_PATH);
	SETTINGS_REG_KEY_I(nrpe::CMD_TIMEOUT);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_ARGS);
	SETTINGS_REG_KEY_B(nrpe::ALLOW_NASTY);

	SETTINGS_REG_PATH(nrpe::SECTION);
	SETTINGS_REG_PATH(nrpe::SECTION_HANDLERS);

	bUseSSL_ = SETTINGS_GET_BOOL(nrpe::KEYUSE_SSL)==1;
	noPerfData_ = SETTINGS_GET_INT(nrpe::ALLOW_PERFDATA)==0;
	timeout = SETTINGS_GET_INT(nrpe::CMD_TIMEOUT);
	socketTimeout_ = SETTINGS_GET_INT(nrpe::READ_TIMEOUT);
	scriptDirectory_ = SETTINGS_GET_STRING(nrpe::SCRIPT_PATH);
	buffer_length_ = SETTINGS_GET_INT(nrpe::PAYLOAD_LENGTH);
	allowArgs_ = SETTINGS_GET_BOOL(nrpe::ALLOW_ARGS);
	allowNasty_ = SETTINGS_GET_BOOL(nrpe::ALLOW_NASTY);
	if (buffer_length_ != 1024)
		NSC_DEBUG_MSG_STD(_T("Non-standard buffer length (hope you have recompiled check_nrpe changing #define MAX_PACKETBUFFER_LENGTH = ") + strEx::itos(buffer_length_));
	NSC_DEBUG_MSG_STD(_T("Loading all commands (from NRPE)"));
	std::list<std::wstring> commands = NSCModuleHelper::getSettingsSection(setting_keys::nrpe::SECTION_HANDLERS_PATH);
	std::list<std::wstring>::const_iterator it;
	for (it = commands.begin(); it != commands.end(); ++it) {
		std::wstring command_name;
		if (((*it).length() > 7)&&((*it).substr(0,7) == _T("command"))) {
			strEx::token t = strEx::getToken((*it), '[');
			t = strEx::getToken(t.second, ']');
			command_name = t.first;
		} else {
			command_name = (*it);
		}
		std::wstring s = NSCModuleHelper::getSettingsString(setting_keys::nrpe::SECTION_HANDLERS_PATH, (*it), _T(""));
		if (command_name.empty() || s.empty()) {
			NSC_LOG_ERROR_STD(_T("Invalid command definition: ") + (*it));
		} else {
			if ((s.length() > 7)&&(s.substr(0,6) == _T("inject"))) {
				addCommand(inject, command_name.c_str(), s.substr(7));
			} else {
				addCommand(script, command_name.c_str(), s);
			}
		}
	}

	if (!scriptDirectory_.empty()) {
		addAllScriptsFrom(scriptDirectory_);
	}

	allowedHosts.setAllowedHosts(strEx::splitEx(getAllowedHosts(), _T(",")), getCacheAllowedHosts());
	try {
		NSC_DEBUG_MSG_STD(_T("Starting NRPE socket..."));
		unsigned short port = SETTINGS_GET_INT(nrpe::PORT);
		std::wstring host = SETTINGS_GET_STRING(nrpe::BINDADDR);
		unsigned int backLog = SETTINGS_GET_INT(nrpe::LISTENQUE);
		if (mode == NSCAPI::normalStart) {
#ifdef USE_SSL
			if (bUseSSL_) {
				socket_ssl_.setHandler(this);
				socket_ssl_.StartListener(host, port, backLog);
			} else {
#else
			{
#endif
				socket_.setHandler(this);
				socket_.StartListener(host, port, backLog);
			}
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
#ifdef USE_SSL
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
#endif
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}
	root_ = NSCModuleHelper::getBasePath();

	return true;
}

bool NRPEListener::unloadModule() {
	try {
#ifdef USE_SSL
		if (bUseSSL_) {
			socket_ssl_.removeHandler(this);
			if (socket_ssl_.hasListener())
				socket_ssl_.StopListener();
		} else {
#else
		{
#endif
			socket_.removeHandler(this);
			if (socket_.hasListener())
				socket_.StopListener();
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
#ifdef USE_SSL
	} catch (simpleSSL::SSLException e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + e.getMessage());
		return false;
#endif
	}
	return true;
}


bool NRPEListener::hasCommandHandler() {
	return true;
}
bool NRPEListener::hasMessageHandler() {
	return false;
}


NSCAPI::nagiosReturn NRPEListener::handleCommand(const strEx::blindstr command, const unsigned int argLen, TCHAR **char_args, std::wstring &message, std::wstring &perf) {
	command_list::const_iterator cit = commands.find(command);
	if (cit == commands.end())
		return NSCAPI::returnIgnored;

	const command_data cd = (*cit).second;
	std::wstring args = cd.arguments;
	if (allowArgs_) {
		arrayBuffer::arrayList arr = arrayBuffer::arrayBuffer2list(argLen, char_args);
		arrayBuffer::arrayList::const_iterator cit2 = arr.begin();
		int i=1;

		for (;cit2!=arr.end();cit2++,i++) {
			if (allowNasty_) {
				if ((*cit2).find_first_of(NASTY_METACHARS) != std::wstring::npos) {
					NSC_LOG_ERROR(_T("Request string contained illegal metachars!"));
					return NSCAPI::returnIgnored;
				}
			}
			strEx::replace(args, _T("$ARG") + strEx::itos(i) + _T("$"), (*cit2));
		}
	}
	if (cd.type == inject) {
		strEx::token t = strEx::getToken(args, ' ');
		std::wstring s = t.second;
		std::wstring sTarget;

		std::wstring::size_type p = 0;
		while(true) {
			std::wstring::size_type pStart = p;
			std::wstring::size_type pEnd = std::wstring::npos;
			if (s[p] == '\"') {
				pStart++;
				while (true) {
					p = s.find(' ', ++p);
					if (p == std::wstring::npos)
						break;
					if ((p>1)&&(s[p-1]=='\"')&&(((p>2)&&(s[p-2]!='\\'))||(p==2)))
						break;
				}
				if (p != std::wstring::npos)
					pEnd = p-1;
				else
					pEnd = s.length()-1;
				if (p != std::wstring::npos) {
					p++;
				}
			} else {
				pEnd = p = s.find(' ', ++p);
				if (p != std::wstring::npos) {
					p = s.find_first_not_of(' ', p);
				}
			}
			if (!sTarget.empty())
				sTarget += _T("!");
			if (p == std::wstring::npos) {
				if (pEnd == std::wstring::npos)
					sTarget += s.substr(pStart);
				else
					sTarget += s.substr(pStart, pEnd-pStart);
				break;
			}
			sTarget += s.substr(pStart,pEnd-pStart);
			//p++;
		}
		try {
			return NSCModuleHelper::InjectSplitAndCommand(t.first, sTarget, '!', message, perf);
		} catch (NSCModuleHelper::NSCMHExcpetion e) {
			NSC_LOG_ERROR_STD(_T("Failed to inject command (") + command.c_str() + _T("): ") + e.msg_);
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("Failed to inject command (") + command.c_str() + _T("): Unknown error REPORT THIS"));
		}
	} else if (cd.type == script) {
		int result = process::executeProcess(root_, args, message, perf, timeout);
		if (!NSCHelper::isNagiosReturnCode(result)) {
			NSC_LOG_ERROR_STD(_T("The command (") + command.c_str() + _T(") returned an invalid return code: ") + strEx::itos(result));
			return NSCAPI::returnUNKNOWN;
		}
		return NSCHelper::int2nagios(result);
	} else if (cd.type == script_dir) {
		std::wstring args = arrayBuffer::arrayBuffer2string(char_args, argLen, _T(" "));
		std::wstring cmd = scriptDirectory_ + command.c_str() + _T(" ") +args;
		int result = process::executeProcess(root_, cmd, message, perf, timeout);
		if (!NSCHelper::isNagiosReturnCode(result)) {
			NSC_LOG_ERROR_STD(_T("The command (") + command.c_str() + _T(") returned an invalid return code: ") + strEx::itos(result));
			return NSCAPI::returnUNKNOWN;
		}
		return NSCHelper::int2nagios(result);
	} else {
		NSC_LOG_ERROR_STD(_T("Unknown script type: ") + command.c_str());
		return NSCAPI::critical;
	}
	return NSCAPI::returnIgnored;
}

void NRPEListener::onClose()
{}

void NRPEListener::onAccept(simpleSocket::Socket *client) 
{
	if (!allowedHosts.inAllowedHosts(client->getAddr())) {
		NSC_LOG_ERROR(_T("Unauthorize access from: ") + client->getAddrString());
		client->close();
		return;
	}
	try {
		simpleSocket::DataBuffer block;
		int i;
		int maxWait = socketTimeout_*10;
		for (i=0;i<maxWait;i++) {
			bool lastReadHasMore = false;
			try {
				lastReadHasMore = client->readAll(block, 1048);
			} catch (simpleSocket::SocketException e) {
				NSC_LOG_MESSAGE(_T("Could not read NRPE packet from socket :") + e.getMessage());
				client->close();
				return;
			}
			if (block.getLength() >= NRPEPacket::getBufferLength(buffer_length_))
				break;
			if (!lastReadHasMore) {
				NSC_LOG_MESSAGE(_T("Could not read a full NRPE packet from socket, only got: ") + strEx::itos(block.getLength()));
				client->close();
				return;
			}
			Sleep(100);
		}
		if (i >= maxWait) {
			NSC_LOG_ERROR_STD(_T("Timeout reading NRPE-packet (increase socket_timeout), we only got: ") + strEx::itos(block.getLength()));
			client->close();
			return;
		}
		if (block.getLength() == NRPEPacket::getBufferLength(buffer_length_)) {
			try {
				NRPEPacket out = handlePacket(NRPEPacket(block.getBuffer(), block.getLength(), buffer_length_));
				block.copyFrom(out.getBuffer(), out.getBufferLength());
			} catch (NRPEPacket::NRPEPacketException e) {
				NSC_LOG_ERROR_STD(_T("NRPESocketException: ") + e.getMessage());
				try {
					NRPEPacket err(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("Could not construct return paket in NRPE handler check clientside (nsclient.log) logs..."), buffer_length_);
					block.copyFrom(err.getBuffer(), err.getBufferLength());
				} catch (NRPEPacket::NRPEPacketException e) {
					NSC_LOG_ERROR_STD(_T("NRPESocketException (again): ") + e.getMessage());
					client->close();
					return;
				}
			}
			int maxWait = socketTimeout_*10;
			for (i=0;i<maxWait;i++) {
				bool lastReadHasMore = false;
				try {
					if (client->canWrite())
						lastReadHasMore = client->sendAll(block);
				} catch (simpleSocket::SocketException e) {
					NSC_LOG_MESSAGE(_T("Could not send NRPE packet from socket :") + e.getMessage());
					client->close();
					return;
				}
				if (!lastReadHasMore) {
					client->close();
					return;
				}
				Sleep(100);
			}
			if (i >= maxWait) {
				NSC_LOG_ERROR_STD(_T("Timeout reading NRPE-packet (increase socket_timeout)"));
				client->close();
				return;
			}
		} else {
			NSC_LOG_ERROR_STD(_T("We got more then we wanted ") + strEx::itos(NRPEPacket::getBufferLength(buffer_length_)) + _T(", we only got: ") + strEx::itos(block.getLength()));
			client->close();
			return;
		}
	} catch (simpleSocket::SocketException e) {
		NSC_LOG_ERROR_STD(_T("SocketException: ") + e.getMessage());
	} catch (NRPEException e) {
		NSC_LOG_ERROR_STD(_T("NRPEException: ") + e.getMessage());
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unhandled Exception in NRPE listner..."));
	}
	client->close();
}

NRPEPacket NRPEListener::handlePacket(NRPEPacket p) {
	if (p.getType() != NRPEPacket::queryPacket) {
		NSC_LOG_ERROR(_T("Request is not a query."));
		throw NRPEException(_T("Invalid query type: ") + strEx::itos(p.getType()));
	}
	if (p.getVersion() != NRPEPacket::version2) {
		NSC_LOG_ERROR(_T("Request had unsupported version."));
		throw NRPEException(_T("Invalid version"));
	}
	if (!p.verifyCRC()) {
		NSC_LOG_ERROR(_T("Request had invalid checksum."));
		throw NRPEException(_T("Invalid checksum"));
	}
	strEx::token cmd = strEx::getToken(p.getPayload(), '!');
	if (cmd.first == _T("_NRPE_CHECK")) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnOK, _T("I (") + NSCModuleHelper::getApplicationVersionString() + _T(") seem to be doing fine..."), buffer_length_);
	}
	std::wstring msg, perf;

	if (allowArgs_) {
		if (!cmd.second.empty()) {
			NSC_LOG_ERROR(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
			throw NRPEException(_T("Request contained arguments (not currently allowed, check the allow_arguments option)."));
		}
	}
	if (allowNasty_) {
		if (cmd.first.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request command contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
		if (cmd.second.find_first_of(NASTY_METACHARS) != std::wstring::npos) {
			NSC_LOG_ERROR(_T("Request arguments contained illegal metachars!"));
			throw NRPEException(_T("Request command contained illegal metachars!"));
		}
	}
	//TODO REMOVE THIS
	//return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("TEST TEST TEST"), buffer_length_);

	NSCAPI::nagiosReturn ret = -3;
	try {
		ret = NSCModuleHelper::InjectSplitAndCommand(cmd.first, cmd.second, '!', msg, perf);
	} catch (...) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, NSCAPI::returnUNKNOWN, _T("UNKNOWN: Internal exception"), buffer_length_);
	}
	switch (ret) {
		case NSCAPI::returnInvalidBufferLen:
			msg = _T("UNKNOWN: Return buffer to small to handle this command.");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnIgnored:
			msg = _T("UNKNOWN: No handler for that command");
			ret = NSCAPI::returnUNKNOWN;
			break;
		case NSCAPI::returnOK:
		case NSCAPI::returnWARN:
		case NSCAPI::returnCRIT:
		case NSCAPI::returnUNKNOWN:
			break;
		default:
			msg = _T("UNKNOWN: Internal error.");
			ret = NSCAPI::returnUNKNOWN;
	}
	if (msg.length() >= buffer_length_-1) {
		NSC_LOG_ERROR(_T("Truncating returndata as it is bigger then NRPE allowes :("));
		msg = msg.substr(0,buffer_length_-2);
	}
	if (perf.empty()||noPerfData_) {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg, buffer_length_);
	} else {
		return NRPEPacket(NRPEPacket::responsePacket, NRPEPacket::version2, ret, msg + _T("|") + perf, buffer_length_);
	}
}

NSC_WRAPPERS_MAIN_DEF(gNRPEListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_HANDLE_CMD_DEF(gNRPEListener);
NSC_WRAPPERS_HANDLE_CONFIGURATION(gNRPEListener);


MODULE_SETTINGS_START(NRPEListener, _T("NRPE Listener configuration"), _T("...")) 

PAGE(_T("NRPE Listsner configuration")) 

ITEM_EDIT_TEXT(_T("port"), _T("This is the port the NRPEListener.dll will listen to.")) 
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
