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
#pragma once

#include <thread.h>
#include <Mutex.h>
#include <arrayBuffer.h>
#include <Socket.h>
#include "nsca_enrypt.hpp"

/**
 * @ingroup NSClientCompat
 * PDH collector thread (gathers performance data and allows for clients to retrieve it)
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 * 
 */
namespace NSCAPacket {
#define NSCA_MAX_HOSTNAME_LENGTH	64
#define NSCA_MAX_DESCRIPTION_LENGTH	128
#define NSCA_MAX_PLUGINOUTPUT_LENGTH	512
#define NSCA_MAX_PASSWORD_LENGTH     512

	/******************** MISC DEFINITIONS *****************/

#define NSCA_TRANSMITTED_IV_SIZE     128     /* size of IV to transmit - must be as big as largest IV needed for any crypto algorithm */


	/*************** PACKET STRUCTURE DEFINITIONS **********/

#define NSCA_PACKET_VERSION_3   3		/* packet version identifier */
#define NSCA_PACKET_VERSION_2	2		/* older packet version identifiers */
#define NSCA_PACKET_VERSION_1	1

	typedef short int16_t;
	typedef unsigned long u_int32_t;

	/* data packet containing service check results */
	class NSCAException {
		std::wstring msg_;
	public:
		NSCAException(std::wstring msg) : msg_(msg) {}
		std::wstring getMessage() { return msg_;}
	};
	typedef struct data_packet_struct {
		int16_t   packet_version;
		u_int32_t crc32_value;
		u_int32_t timestamp;
		int16_t   return_code;
		char      host_name[NSCA_MAX_HOSTNAME_LENGTH];
		char      svc_description[NSCA_MAX_DESCRIPTION_LENGTH];
		char      plugin_output[NSCA_MAX_PLUGINOUTPUT_LENGTH];
		data_packet_struct() : packet_version(NSCA_PACKET_VERSION_3) {}

	} data_packet;

	/* initialization packet containing IV and timestamp */
	typedef struct init_packet_struct {
		char      iv[NSCA_TRANSMITTED_IV_SIZE];
		u_int32_t timestamp;
	} init_packet;
}


class Arguments {
	arrayBuffer::arrayBuffer arglist_;
	unsigned int arglen_;
public:
	Arguments() : arglist_(NULL), arglen_(0) {}
	~Arguments() {
		arrayBuffer::destroyArrayBuffer(arglist_, arglen_);
		arglist_ = NULL;
	}
	Arguments(const Arguments& other) : arglist_(NULL), arglen_(0) {
		arglen_ = other.getLen();
		arglist_ = arrayBuffer::copy(other.get(), other.getLen());
	}
	Arguments& operator=(Arguments& other) {
		if (this != &other){
			arglen_ = other.getLen();
			arglist_ = other.detach();
		}
		return *this;
	}
	Arguments& operator=(const Arguments& other) {
		if (this != &other){
			arglen_ = other.getLen();
			arglist_ = arrayBuffer::copy(other.get(), other.getLen());
		}
		return *this;
	}
	Arguments(const std::wstring &other) : arglist_(NULL), arglen_(0)  {
		arglist_ = arrayBuffer::split2arrayBuffer(other, ' ', arglen_, true);
	}
	arrayBuffer::arrayBuffer detach() {
		arrayBuffer::arrayBuffer ret = arglist_;
		arglist_ = NULL;
		arglen_ = 0;
		return ret;
	}
	const arrayBuffer::arrayBuffer get() const {
		return arglist_;
	}
	unsigned int getLen() const {
		return arglen_;
	}
};
class Command {
	std::wstring cmd_;
	std::wstring alias_;
	Arguments args_;
public:

	class Result {
	public:
		Result(std::wstring _host) : host(_host){
			_time32(&time);
			//time+=60*60 + 2*60;
		}
		std::wstring service;
		std::wstring result;
		std::wstring host;
		unsigned int code;
		__time32_t time;
		std::wstring toString() const {
			return _T("service: ") + service + _T(", ") + 
				_T("code: ") + strEx::itos(code) + _T(", ") + 
				_T("time: ") + strEx::format_date(time) + _T(", ") + 
				_T("result: ") + result;
		}

		simpleSocket::DataBuffer getBuffer(nsca_encrypt &crypt_inst) const {
			std::string s = strEx::wstring_to_string(service);
			std::string r = strEx::wstring_to_string(result);
			std::string h = strEx::wstring_to_string(host);

			unsigned int buffer_len = sizeof(NSCAPacket::data_packet);
			unsigned char* buffer = crypt_inst.get_rand_buffer(buffer_len);
			NSCAPacket::data_packet *data = reinterpret_cast<NSCAPacket::data_packet*>(buffer);
			data->packet_version=static_cast<NSCAPacket::int16_t>(htons(NSCA_PACKET_VERSION_3));
			data->timestamp=static_cast<NSCAPacket::u_int32_t>(htonl(time));
			data->return_code = ntohs(code);
			data->crc32_value=static_cast<NSCAPacket::u_int32_t>(0L);

			if (h.length() >= NSCA_MAX_HOSTNAME_LENGTH)
				throw NSCAPacket::NSCAException(_T("Hostname to long"));
			strncpy_s(data->host_name, NSCA_MAX_HOSTNAME_LENGTH, h.c_str(), h.length());
			if (s.length() >= NSCA_MAX_DESCRIPTION_LENGTH)
				throw NSCAPacket::NSCAException(_T("description to long"));
			strncpy_s(data->svc_description, NSCA_MAX_DESCRIPTION_LENGTH, s.c_str(), s.length());
			if (r.length() >= NSCA_MAX_PLUGINOUTPUT_LENGTH)
				throw NSCAPacket::NSCAException(_T("result to long"));
			strncpy_s(data->plugin_output, NSCA_MAX_PLUGINOUTPUT_LENGTH, r.c_str(), r.length());

			unsigned int calculated_crc32=calculate_crc32(buffer,buffer_len);
			data->crc32_value=static_cast<NSCAPacket::u_int32_t>(htonl(calculated_crc32));
			crypt_inst.encrypt_buffer(buffer, buffer_len);
			simpleSocket::DataBuffer ret(buffer,buffer_len);
			crypt_inst.destroy_random_buffer(buffer);
			return ret;
		}

	};

	Command() {}
	Command(const Command & other) {
		cmd_ = other.cmd_;
		args_ = other.args_;
		alias_ = other.alias_;

	}
	Command(std::wstring alias, std::wstring raw) : alias_(alias) {
		strEx::token token = strEx::getToken(raw, ' ');
		cmd_ = token.first;
		args_ = token.second;
	}
	static unsigned int conv_code(NSCAPI::nagiosReturn ret) {
		if (ret == NSCAPI::returnOK)
			return 0;
		if (ret == NSCAPI::returnWARN)
			return 1;
		if (ret == NSCAPI::returnCRIT)
			return 2;
		return 3;
	}
	Result execute(std::wstring host) const;
};

class NSCAThread {
private:
	MutexHandler mutexHandler;
	HANDLE hStopEvent_;
	int checkIntervall_;
	std::list<Command> commands_;
	std::wstring hostname_;
	std::wstring nscahost_;
	unsigned int nscaport_;
	std::string password_;
	int encryption_method_;

public:
	NSCAThread();
	virtual ~NSCAThread();
	DWORD threadProc(LPVOID lpParameter);
	void exitThread(void);
	void send(const std::list<Command::Result> &results);


private:
	void addCommand(std::wstring raw);

};


typedef Thread<NSCAThread> NSCAThreadImpl;