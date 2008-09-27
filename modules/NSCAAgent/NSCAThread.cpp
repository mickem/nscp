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
#include <Settings.h>


NSCAThread::NSCAThread() : hStopEvent_(NULL) {
	checkIntervall_ = SETTINGS_GET_INT(nsca::INTERVAL);
	hostname_ = SETTINGS_GET_STRING(nsca::HOSTNAME);
	nscahost_ = SETTINGS_GET_STRING(nsca::SERVER_HOST);
	nscaport_ = SETTINGS_GET_INT(nsca::SERVER_PORT);
	encryption_method_ = SETTINGS_GET_INT(nsca::ENCRYPTION);
	password_ = strEx::wstring_to_string(SETTINGS_GET_STRING(nsca::PASSWORD));
	cacheNscaHost_ = SETTINGS_GET_BOOL(nsca::CACHE_HOST);
	std::list<std::wstring> items = NSCModuleHelper::getSettingsSection(settings::nsca::CMD_SECTION_PATH);
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
	NSCAPI::nagiosReturn ret = NSCModuleHelper::InjectCommand(cmd_.c_str(), args_.getLen(), args_.get(), msg, perf);
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
	std::wstring value = NSCModuleHelper::getSettingsString(settings::nsca::CMD_SECTION_PATH, key, _T(""));
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
	while (((waitStatus = WaitForSingleObject(hStopEvent_, remain*1000)) == WAIT_TIMEOUT)) {
		MutexLock mutex(mutexHandler);
		if (!mutex.hasMutex()) 
			NSC_LOG_ERROR(_T("Failed to get Mutex!"));
		else {
			__int64 start, stop;
			_time64( &start );

			std::list<Command::Result> results;
			for (std::list<Command>::const_iterator cit = commands_.begin(); cit != commands_.end(); ++cit) {
				results.push_back((*cit).execute(hostname_));
			}
			send(results);
			_time64( &stop );
			__int64 elapsed = stop-start;
			remain = checkIntervall_-static_cast<int>(elapsed);
			if (remain < 0)
				remain = 0;
		} 
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
		if (!socket.readAll(inc, sizeof(NSCAPacket::init_packet_struct), sizeof(NSCAPacket::init_packet_struct))) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to read header from: ") + nscaaddr_ + _T(":") + strEx::itos(nscaport_) + _T(" ") + socket.getLastError());
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
					socket.send((*cit).getBuffer(crypt_inst));
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
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("<<< NSCA Configuration missmatch (hint: if you dont use NSCA dot use the NSCA module)!"));
		return;
	}
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
