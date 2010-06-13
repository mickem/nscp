//////////////////////////////////////////////////////////////////////////
// PDH Collector 
// 
// Functions from this file collects data from the PDH subsystem and stores 
// it for later use
// *NOTICE* that this is done in a separate thread so threading issues has 
// to be handled. I handle threading issues in the CounterListener's get/
// set accessors.
//
// Copyright (c) 2004 MySolutions NORDIC (http://www.medin.nu)
//
// Date: 2004-03-13
// Author: Michael Medin - <michael@medin.name>
//
// This software is provided "AS IS", without a warranty of any kind.
// You are free to use/modify this code but leave this header intact.
//
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "NSCAThread.h"

#define REPORT_ERROR	0x01
#define REPORT_WARNING	0x02
#define REPORT_UNKNOWN	0x04
#define REPORT_OK		0x08

unsigned int parse_report_string(std::wstring str) {
	unsigned int report = 0;
	strEx::splitList lst = strEx::splitEx(str, _T(","));
	for (strEx::splitList::const_iterator key = lst.begin(); key != lst.end(); ++key) {
		if (*key == _T("all")) {
			report = REPORT_ERROR|REPORT_OK|REPORT_UNKNOWN|REPORT_WARNING;
		} else if (*key == _T("error") || *key == _T("err") || *key == _T("critical") || *key == _T("crit")) {
			report |= REPORT_ERROR;
		} else if (*key == _T("warning") || *key == _T("warn")) {
			report |= REPORT_WARNING;
		} else if (*key == _T("unknown")) {
			report |= REPORT_UNKNOWN;
		} else if (*key == _T("ok")) {
			report |= REPORT_OK;
		}
	}
	return report;
}
std::wstring generate_report_string(unsigned int report) {
	std::wstring ret;
	if ((report&REPORT_OK)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("ok");
	}
	if ((report&REPORT_WARNING)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("warning");
	}
	if ((report&REPORT_ERROR)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("critical");
	}
	if ((report&REPORT_UNKNOWN)!=0) {
		if (!ret.empty())	ret += _T(",");
		ret += _T("unknown");
	}
	return ret;
}

NSCAThread::NSCAThread() : hStopEvent_(NULL) {
	std::wstring tmpstr = NSCModuleHelper::getSettingsString(NSCA_AGENT_SECTION_TITLE, NSCA_TIME_DELTA, NSCA_TIME_DELTA_DEFAULT);
	std::wstring tmpstr = SETTINGS_GET_STRING(nsca::TIME_DELTA_DEFAULT);
	if (tmpstr[0] == '-' && tmpstr.size() > 2)
		timeDelta_ = 0 - strEx::stoui_as_time(tmpstr.substr(1));
	if (tmpstr[0] == '+' && tmpstr.size() > 2)
		timeDelta_ = strEx::stoui_as_time(tmpstr.substr(1));
	else
		timeDelta_ = strEx::stoui_as_time(tmpstr);
	timeDelta_ = timeDelta_ / 1000;
	NSC_DEBUG_MSG_STD(_T("Time difference for NSCA server is: ") + strEx::itos(timeDelta_));
	report_ = parse_report_string(report);
	NSC_DEBUG_MSG_STD(_T("Only reporting: ") + generate_report_string(report_));
	
	encryption_method_ = NSCModuleHelper::getSettingsInt(NSCA_AGENT_SECTION_TITLE, NSCA_ENCRYPTION, NSCA_ENCRYPTION_DEFAULT);
	std::wstring password = NSCModuleHelper::getSettingsString(NSCA_AGENT_SECTION_TITLE, MAIN_OBFUSCATED_PASWD, MAIN_OBFUSCATED_PASWD_DEFAULT);
	if (!password.empty())
		password = NSCModuleHelper::Decrypt(password);
	if (password.empty())
		password = NSCModuleHelper::getSettingsString(NSCA_AGENT_SECTION_TITLE, NSCA_PASSWORD, NSCA_PASSWORD_DEFAULT);
	if (password.empty()) {
		// read main password if no NSCA one is found
		password = NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_OBFUSCATED_PASWD, MAIN_OBFUSCATED_PASWD_DEFAULT);
		if (!password.empty())
			password = NSCModuleHelper::Decrypt(password);
		if (password.empty())
			password = NSCModuleHelper::getSettingsString(MAIN_SECTION_TITLE, MAIN_SETTINGS_PWD, MAIN_SETTINGS_PWD_DEFAULT);
	}
	password_ = strEx::wstring_to_string(password);
	encryption_method_ = SETTINGS_GET_INT(nsca::ENCRYPTION);
	password_ = strEx::wstring_to_string(SETTINGS_GET_STRING(nsca::PASSWORD));
	cacheNscaHost_ = SETTINGS_GET_INT(nsca::CACHE_HOST);
	read_timeout_ = SETTINGS_GET_INT(nsca::READ_TIMEOUT);

	std::list<std::wstring> items = NSCModuleHelper::getSettingsSection(setting_keys::nsca::CMD_SECTION_TITLE);
	for (std::list<std::wstring>::const_iterator cit = items.begin(); cit != items.end(); ++cit) {
		addCommand(*cit);
	}
	if (hostname_.empty()) {
		TCHAR *buf = new TCHAR[MAX_COMPUTERNAME_LENGTH + 2];
		DWORD size = MAX_COMPUTERNAME_LENGTH+1;
		if (!GetComputerName(buf, &size)) {
			NSC_LOG_ERROR(_T("Failed to get computer name: setting it to <unknown>"));
			hostname_ = _T("<unknown>");
		} else {
			buf[size] = 0;
			hostname_ = buf;
			NSC_DEBUG_MSG_STD(_T("Autodetected hostname: ") + hostname_);
		}
		delete[] buf;
	}
}

NSCAThread::~NSCAThread() 
{
	if (hStopEvent_)
		CloseHandle(hStopEvent_);
}
Command::Result Command::execute(std::wstring host) const {
	Result result(host);
	std::wstring msg;
	std::wstring perf;
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectSimpleCommand(cmd_.c_str(), args_, msg, perf);
	result.service = alias_;
	if (ret == NSCAPI::returnIgnored) {
		result.result = _T("Command was not found: ") + cmd_;
		result.code = NSCAPI::returnUNKNOWN;
	} else if (ret == NSCAPI::returnInvalidBufferLen) {
		result.result = _T("Result was to long: ") + cmd_;
		result.code = NSCAPI::returnUNKNOWN;
	} else {
		result.result = msg + _T("|") + perf;
		result.code = conv_code(ret);
		if (result.result.length() >= NSCA_MAX_PLUGINOUTPUT_LENGTH) {
			NSC_LOG_ERROR(_T("NSCA return data truncated"));
			result.result = result.result.substr(0, NSCA_MAX_PLUGINOUTPUT_LENGTH-1);
		}
	}
	return result;
}


void NSCAThread::addCommand(std::wstring key) {
	std::wstring value = NSCModuleHelper::getSettingsString(setting_keys::nsca::CMD_SECTION_PATH, key, _T(""));
	if ((key.length() > 4) && (key.substr(0,4) == _T("host")))
		commands_.push_back(Command(_T(""), value));
	else
		commands_.push_back(Command(key, value));
}


/**
* Thread that collects the data every "CHECK_INTERVAL" seconds.
*
* @param lpParameter Not used
* @return thread exit status
*
* @author mickem
*
* @date 03-13-2004               
*
* @bug If we have "custom named" counters ?
* @bug This whole concept needs work I think.
*
*/
DWORD NSCAThread::threadProc(LPVOID lpParameter) {
	hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hStopEvent_) {
		NSC_LOG_ERROR_STD(_T("Create StopEvent failed: ") + error::lookup::last_error());
		return 0;
	}

	srand( reinterpret_cast<int>(lpParameter) );

	DWORD waitStatus = 0;
	int drift = (checkIntervall_*rand())/RAND_MAX ;
	NSC_DEBUG_MSG_STD(_T("Drifting: ") + strEx::itos(drift));
	waitStatus = WaitForSingleObject(hStopEvent_, drift*1000);
	if (waitStatus != WAIT_TIMEOUT)  {
		NSC_LOG_ERROR_STD(_T("Drift failed... strange..."));
	}
	int remain = checkIntervall_;
	try {
		while (((waitStatus = WaitForSingleObject(hStopEvent_, remain*1000)) == WAIT_TIMEOUT)) {
			MutexLock mutex(mutexHandler);
			if (!mutex.hasMutex()) 
				NSC_LOG_ERROR(_T("Failed to get Mutex!"));
			else {
				__int64 start, stop;
				_time64( &start );

				std::list<Command::Result> results;
				for (std::list<Command>::const_iterator cit = commands_.begin(); cit != commands_.end(); ++cit) {
					try {
						NSC_DEBUG_MSG_STD(_T("Executing (from NSCA): ") + (*cit).alias());
						Command::Result result = (*cit).execute(hostname_);
						if (
							(result.code == NSCAPI::returnOK && ((report_&REPORT_OK)==REPORT_OK) ) ||
							(result.code == NSCAPI::returnCRIT && ((report_&REPORT_ERROR)==REPORT_ERROR) ) ||
							(result.code == NSCAPI::returnWARN && ((report_&REPORT_WARNING)==REPORT_WARNING) ) ||
							(result.code == NSCAPI::returnUNKNOWN && ((report_&REPORT_UNKNOWN)==REPORT_UNKNOWN) )
							) 
						{
							results.push_back(result);
						} else {
							NSC_DEBUG_MSG_STD(_T("Ignoring result (it does not match our reporting prefix: ") + (*cit).alias());
						}
					} catch (...) {
						NSC_LOG_ERROR_STD(_T("Unknown exception when executing: ") + (*cit).alias());
					}
				}
				if (!results.empty()) {
					try {
						send(results);
					} catch (...) {
						NSC_LOG_ERROR_STD(_T("Unknown exception when sending commands to server..."));
					}
				} else {
					NSC_DEBUG_MSG_STD(_T("Nothing to report, thus not reporting anything..."));
				}
				_time64( &stop );
				__int64 elapsed = stop-start;
				remain = checkIntervall_-static_cast<int>(elapsed);
				if (remain < 1)
					remain = 1;
			} 
		}
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Unknown exception in NSCA Thread terminating..."));
	}


	if (waitStatus != WAIT_OBJECT_0) {
		NSC_LOG_ERROR(_T("Something odd happened, terminating NSCA submission thread!"));
	}

	{
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) {
			NSC_LOG_ERROR(_T("Failed to get Mute when closing thread!"));
		}

		if (!CloseHandle(hStopEvent_)) {
			NSC_LOG_ERROR_STD(_T("Failed to close stopEvent handle: ") + error::lookup::last_error());
		} else
			hStopEvent_ = NULL;
	}
	return 0;
}
void NSCAThread::send(const std::list<Command::Result> &results) {
	NSC_DEBUG_MSG_STD(_T("Sending to server..."));
	try {
		nsca_encrypt crypt_inst;
		simpleSocket::Socket socket(true);
		simpleSocket::DataBuffer inc;
		if (!cacheNscaHost_ || nscaaddr_.empty()) {
			nscaaddr_ = socket.getHostByName(nscahost_);
			NSC_DEBUG_MSG_STD(_T("Looked up ") + nscahost_ + _T(" to ") + nscaaddr_);
		}
		if (nscaaddr_.empty()) {
			NSC_LOG_ERROR_STD(_T("Failed to lookup host: ") + nscahost_);
			return;
		}
		if (socket.connect(nscaaddr_, nscaport_) == SOCKET_ERROR) {
			NSC_LOG_ERROR_STD(_T("<<< Could not connect to: ") + nscaaddr_ + _T(":") + strEx::itos(nscaport_) + _T(" ") + socket.getLastError());
			return;
		}
		socket.setNonBlock();

		time_t start = time(NULL);
		unsigned int sz_packet = sizeof(NSCAPacket::init_packet_struct);
		do {
			bool lastReadHasMore = false;
			try {
				lastReadHasMore = socket.readAll(inc, sz_packet);
			} catch (simpleSocket::SocketException e) {
				NSC_LOG_ERROR_STD(_T("Could not read NSCA hdr packet from socket :") + e.getMessage());
				socket.close();
				return;
			}
			if (inc.getLength() >= sz_packet)
				break;
			if (!lastReadHasMore) {
				NSC_LOG_MESSAGE(_T("Could not read a full NSCA hdr packet from socket, only got: ") + strEx::itos(inc.getLength()));
				socket.close();
				return;
			}
			Sleep(100);
		} while ((time(NULL) - start) < read_timeout_);
		if (inc.getLength() != sz_packet) {
			NSC_LOG_ERROR_STD(_T("Timeout reading NSCA hdr packet (increase socket_timeout), we only got: ") + strEx::itos(inc.getLength()));
			socket.close();
			return;
		}

		NSCAPacket::init_packet_struct *packet_in = (NSCAPacket::init_packet_struct*) inc.getBuffer();
		try {
			crypt_inst.encrypt_init(password_.c_str(),encryption_method_,reinterpret_cast<unsigned char*>(packet_in->iv));
		} catch (nsca_encrypt::encryption_exception &e) {
			NSC_LOG_ERROR_STD(_T("<<< NSCA Encryption header missmatch (hint: if you dont use NSCA dot use the NSCA module): ") + e.getMessage());
			return;
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("<<< NSCA Encryption header missmatch (hint: if you dont use NSCA dot use the NSCA module)!"));
			return;
		}

		try {
			for (std::list<Command::Result>::const_iterator cit = results.begin(); cit != results.end(); ++cit) {
				try {
					socket.send((*cit).getBuffer(crypt_inst, timeDelta_, payload_length_));
				} catch (NSCAPacket::NSCAException &e) {
					NSC_LOG_ERROR_STD(_T("Failed to make command: ") + e.getMessage() );
				}
			}
		} catch (nsca_encrypt::encryption_exception &e) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to encrypt packet: ") + e.getMessage());
			return;
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to encrypt packet!"));
			return;
		}
		socket.close();
	} catch (simpleSocket::SocketException &e) {
		NSC_LOG_ERROR_STD(_T("NSCA Socket exception: ") + e.getMessage());
		return;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("<<< NSCA Configuration missmatch (hint: if you dont use NSCA dot use the NSCA module)!"));
		return;
	}
	NSC_DEBUG_MSG_STD(_T("Finnished sending to server..."));
}



/**
* Request termination of the thread (waiting for thread termination is not handled)
*/
void NSCAThread::exitThread(void) {
	if (hStopEvent_ == NULL) {
		NSC_LOG_ERROR(_T("Stop event is not created!"));
	} else {
		if (!SetEvent(hStopEvent_)) {
			NSC_LOG_ERROR_STD(_T("SetStopEvent failed"));
		}
	}
}
