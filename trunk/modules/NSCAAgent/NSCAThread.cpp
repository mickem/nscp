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
	checkIntervall_ = NSCModuleHelper::getSettingsInt(NSCA_AGENT_SECTION_TITLE, NSCA_INTERVAL, NSCA_INTERVAL_DEFAULT);
	host_ = NSCModuleHelper::getSettingsString(NSCA_AGENT_SECTION_TITLE, NSCA_HOSTNAME, NSCA_HOSTNAME_DEFAULT);
	port_ = NSCModuleHelper::getSettingsInt(NSCA_AGENT_SECTION_TITLE, NSCA_PORT, NSCA_PORT_DEFAULT);
	encryption_method_ = NSCModuleHelper::getSettingsInt(NSCA_AGENT_SECTION_TITLE, NSCA_ENCRYPTION, NSCA_ENCRYPTION_DEFAULT);
	password_ = strEx::wstring_to_string(NSCModuleHelper::getSettingsString(NSCA_AGENT_SECTION_TITLE, NSCA_PASSWORD, NSCA_PASSWORD_DEFAULT));
	std::list<std::wstring> items = NSCModuleHelper::getSettingsSection(NSCA_CMD_SECTION_TITLE);
	for (std::list<std::wstring>::const_iterator cit = items.begin(); cit != items.end(); ++cit) {
		addCommand(*cit);
	}
	//std::string s = NSCModuleHelper::getSettingsString(C_SYSTEM_SECTION_TITLE, C_SYSTEM_CPU_BUFFER_TIME, C_SYSTEM_CPU_BUFFER_TIME_DEFAULT);
	//unsigned int i = strEx::stoui_as_time(s, checkIntervall_*100);
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
	result.code = conv_code(ret);
	if (ret == NSCAPI::returnIgnored) {
		result.result = _T("Command was not found: ") + cmd_;
	} else if (ret == NSCAPI::returnInvalidBufferLen) {
		result.result = _T("Result was to long: ") + cmd_;
	} else {
		result.result = msg + _T("|") + perf;
	}
	NSC_LOG_MESSAGE_STD(_T("Result: ") + result.toString());
	return result;
}


void NSCAThread::addCommand(std::wstring key) {
	std::wstring value = NSCModuleHelper::getSettingsString(NSCA_CMD_SECTION_TITLE, key, _T(""));
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

	DWORD waitStatus = 0;
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
				results.push_back((*cit).execute(host_));
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
		if (socket.connect(host_, port_) == SOCKET_ERROR) {
			NSC_LOG_ERROR_STD(_T("<<< Could not connect to: ") + host_ + strEx::itos(port_));
			return;
		}
		if (!socket.readAll(inc, sizeof(NSCAPacket::init_packet_struct), sizeof(NSCAPacket::init_packet_struct))) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to read header: ") + host_ + strEx::itos(port_));
			return;
		}
		NSCAPacket::init_packet_struct *packet_in = (NSCAPacket::init_packet_struct*) inc.getBuffer();
		try {
			crypt_inst.encrypt_init(password_.c_str(),encryption_method_,packet_in->iv);
		} catch (nsca_encrypt::exception &e) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to initalize encryption header: ") + e.getMessage());
			return;
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to initalize encryption header!"));
			return;
		}

		try {
			for (std::list<Command::Result>::const_iterator cit = results.begin(); cit != results.end(); ++cit) {
				//NSC_DEBUG_MSG_STD(_T("Sending : ") + (*cit).toString());
				socket.send((*cit).getBuffer(crypt_inst));
			}
		} catch (nsca_encrypt::exception &e) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to encrypt packet: ") + e.getMessage());
			return;
		} catch (...) {
			NSC_LOG_ERROR_STD(_T("<<< Failed to encrypt packet!"));
			return;
		}
		socket.close();
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("<<< Failed to initalize encryption header!"));
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
