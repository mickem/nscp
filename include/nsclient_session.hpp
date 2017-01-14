/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <vector>
#include <memory>
#include <string>
#include <map>
//#include <thread.h>
//#include <event_handler.hpp>
//#include <shared_memory.hpp>

namespace nsclient_session {
	class session_exception {
		std::wstring what_;
	public:
		session_exception(std::wstring what) : what_(what) {}
		std::wstring what() {
			return what_;
		}
	};

	class session_handler_interface {
	public:
		virtual void session_error(std::wstring file, unsigned int line, std::wstring msg) = 0;
		virtual void session_info(std::wstring file, unsigned int line, std::wstring msg) = 0;
		virtual void session_log_message(int msgType, const TCHAR* file, const int line, std::wstring message) = 0;
		virtual int session_inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &msg, std::wstring & perf) = 0;
		virtual std::pair<std::wstring,std::wstring> session_get_name() = 0;
	};

	class session_interface {
	public:
		virtual void error(std::wstring file, unsigned int line, std::wstring msg) = 0;
		virtual bool handle_raw_message() = 0;
		virtual HANDLE get_signal_event() = 0;
		virtual std::wstring get_name() = 0;
	};

	const static std::wstring message_attach = _T("core_attach");
	const static std::wstring message_detach = _T("core_detach");
	const static std::wstring message_log = _T("core_log");
	const static std::wstring message_inject = _T("core_inject");
	const static std::wstring message_get_name = _T("core_get_name");
	const static std::wstring message_master_shutdown = _T("core_master_shutdown");
	const static std::wstring message_master_attach = _T("core_master_attach");
	
	const static int message_version = 1;
	const static std::wstring channel_prefix = _T("Global\\NSClientPP_channel_");

	class remote_channel {
	public:
		struct local_message_type {
			unsigned int message_id;
			std::wstring command;
			std::wstring sender;
			std::vector<std::wstring> arguments;
			local_message_type(std::wstring command_) : message_id(0), command(command_) {}
			local_message_type() : message_id(0) {}

		};
		typedef struct {
			unsigned int version;
			unsigned int message_id;
			unsigned int argCount;
		} shared_message_type;

	private:
		session_interface *instance_;
		event_handler write_event;
		event_handler signal_event;
		shared_memory_handler shared_memory;
		std::wstring channel_name_;

	public:
		remote_channel(session_interface *instance, std::wstring channel_name) 
			: instance_(instance),
			channel_name_(channel_name),
			write_event(true, channel_prefix + channel_name + _T("_write")),
			signal_event(true, channel_prefix + channel_name + _T("_signal")),
			shared_memory(true, channel_prefix + channel_name + _T("_memory"))
		{}

		void open() {
			try {
				write_event.open();
				signal_event.open();
				shared_memory.open();
			} catch (shared_memory_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': Unknown exception!"));
			}
		}
		void close() {
			try {
				write_event.close();
				signal_event.close();
				shared_memory.close();
			} catch (shared_memory_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': Unknown exception!"));
			}
		}
		void create() {
			try {
				write_event.create();
				signal_event.create();
				shared_memory.create();
				write_event.set();
			} catch (shared_memory_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': Unknown exception!"));
			}
		}
		void create_passive() {
			try {
				write_event.create();
				signal_event.create();
				shared_memory.create();
			} catch (shared_memory_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': Unknown exception!"));
			}
		}
		void activate() {
			try {
				write_event.set();
			} catch (shared_memory_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create channel '") + channel_name_ + _T("': Unknown exception!"));
			}
		}
		HANDLE getSignalHandle() {
			return signal_event;
		}

		local_message_type read() {
			if (!shared_memory.isOpen())
				throw session_exception(_T("Invalid session: no memory (cant read message)!"));
			if (!write_event.isOpen())
				throw session_exception(_T("Invalid session: no signal (cant read message)!"));

			local_message_type msg;
			shared_message_type *tmpMsg = reinterpret_cast<shared_message_type*>(shared_memory.getBufferUnsafe());
			if (tmpMsg->version != message_version) 
				throw session_exception(_T("Invalid message version"));
			msg.message_id = tmpMsg->message_id;

			wchar_t *strBuffer = shared_memory.getBufferUnsafe() + sizeof(shared_message_type);

			msg.sender = std::wstring(strBuffer);
			strBuffer = pushString(strBuffer);

			msg.command = std::wstring(strBuffer);
			strBuffer = pushString(strBuffer);

			for (unsigned int i=0;i<tmpMsg->argCount;i++) {
				msg.arguments.push_back(std::wstring(strBuffer));
				strBuffer = pushString(strBuffer);
			}
			try {
				write_event.set();
			} catch (event_exception e) {
				error(__FILEW__, __LINE__, _T("Failed to release mutext: ") + e.what());
			}
			return msg;
		}

		void write(local_message_type msg, DWORD timeout = 5000L) {
			if (!shared_memory.isOpen())
				throw session_exception(_T("Invalid session: no memory (cant send message)!"));
			if (!signal_event.isOpen())
				throw session_exception(_T("Invalid session: no signal (cant send message)!"));

			try {
				if (!write_event.accuire())
					throw session_exception(_T("Failed to get mutex when attempting to post message: ") + write_event.get_wait_result());
			} catch (event_exception e) {
				throw session_exception(_T("Failed to get mutex when attempting to post message to the shared session: ") + e.what());
			}

			shared_message_type *tmpMsg = reinterpret_cast<shared_message_type*>(shared_memory.getBufferUnsafe());
			wchar_t *strBuffer = shared_memory.getBufferUnsafe() + sizeof(shared_message_type);
			tmpMsg->message_id = msg.message_id;
			tmpMsg->argCount = static_cast<unsigned int>(msg.arguments.size());
			tmpMsg->version = message_version;

			wcsncpy(strBuffer, msg.sender.c_str(), msg.sender.length());
			strBuffer[msg.sender.length()] = 0;
			strBuffer = pushString(strBuffer);

			wcsncpy(strBuffer, msg.command.c_str(), msg.command.length());
			strBuffer[msg.command.length()] = 0;
			strBuffer = pushString(strBuffer);

			std::vector<std::wstring>::const_iterator cit;
			for (cit=msg.arguments.begin();cit!=msg.arguments.end();++cit) {
				wcsncpy(strBuffer, (*cit).c_str(), (*cit).length());
				strBuffer[(*cit).length()] = 0;
				strBuffer = pushString(strBuffer);
			}
			try {
				signal_event.set();
			} catch (event_exception e) {
				throw session_exception(_T("Failed to send message: ") + e.what());
			}
		}
		bool exists() const {
			return event_handler::exists(signal_event.get_name());
		}
		static bool exists(std::wstring name) {
			return event_handler::exists(channel_prefix + name + _T("_signal"));
		}
		std::wstring get_name() const {
			return _T("channel-") + channel_name_ + _T("= {") +
				write_event.get_name() + _T(", ") + 
				signal_event.get_name() + _T(", ") + 
				shared_memory.get_name() + _T("}");
		}


	private:
		inline wchar_t* pushString(wchar_t* strBuffer) {
			return strBuffer+wcslen(strBuffer)+1;
		}
		void error(std::wstring file, unsigned int line, std::wstring msg) {
			if (instance_)
				instance_->error(file, line, msg);
		}


	};

	class responder {
		HANDLE hStopEvent_;
		session_interface *instance_;
	public:
		responder() : hStopEvent_(NULL) {}

		~responder() {
			if (hStopEvent_)
				CloseHandle(hStopEvent_);
		}

		DWORD threadProc(LPVOID lpParameter) {
			instance_ = (session_interface*)lpParameter;
			std::wstring name = instance_->get_name();
			hStopEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (!hStopEvent_) {
				instance_->error(__FILEW__, __LINE__, _T("Create StopEvent failed: ") + error::lookup::last_error());
				return 0;
			}
			instance_->error(__FILEW__, __LINE__, _T("Starting session responder for: ") + name);
			HANDLE *handles = new HANDLE[2];
			handles[0] = hStopEvent_;
			handles[1] = instance_->get_signal_event();
			if (handles[1] == NULL)
				instance_->error(__FILEW__, __LINE__, _T("Invalid signal event!"));
			else {
				while (true) {
					DWORD waitStatus = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
					if (waitStatus == WAIT_OBJECT_0)
						break; // exit event
					if (waitStatus == WAIT_OBJECT_0+1) {
						try {
							if (!instance_->handle_raw_message())
								break;
						} catch (session_exception e) {
							instance_->error(__FILEW__, __LINE__, _T("Exception in message handler thread: ") + e.what());
						} catch (...) {
							instance_->error(__FILEW__, __LINE__, _T("Exception in message handler thread: Unknown exception!"));
						}
					}
				}
			}
			instance_->error(__FILEW__, __LINE__, _T("Closing session for: ") + name);

			if (!CloseHandle(hStopEvent_)) {
				instance_->error(__FILEW__, __LINE__, _T("Failed to close stopEvent handle: ") + error::lookup::last_error());
			} else
				hStopEvent_ = NULL;
			return 0;
		}

		void exitThread(void) {
			if (instance_ == NULL)
				throw session_exception(_T("No instance exists cant close the session, make sure the thread has been started!"));
			if (hStopEvent_ == NULL) {
				instance_->error(__FILEW__, __LINE__, _T("Stop event is not created!"));
			} else {
				if (!SetEvent(hStopEvent_)) {
					instance_->error(__FILEW__, __LINE__, _T("SetStopEvent failed"));
				}
			}
		}
	};

	typedef Thread<responder> responder_thread;

	class shared_client_session;
	class shared_session : public session_interface {
	public:
		typedef std::map<std::wstring,remote_channel*> handle_type;
		typedef std::map<unsigned int,event_handler*> message_list_type;
		typedef std::map<unsigned int,remote_channel::local_message_type> response_list_type;

		friend shared_client_session;
	protected:
		std::auto_ptr<remote_channel> master_channel_;
		std::auto_ptr<remote_channel> client_channel_;
		responder_thread responder_;
		session_handler_interface *handler_;
		unsigned int message_id_;
		std::wstring client_id_;
		handle_type remote_channels_;
		message_list_type wait_list_;
		response_list_type response_list_;
		std::wstring master_id_;

	public:
		shared_session(std::wstring session_id, session_handler_interface *handler) 
			: responder_(_T("shmem-responder-thread")), 
			client_id_(session_id),
			handler_(handler), 
			message_id_(0),
			master_id_(_T("server"))
		{}
		~shared_session() {
			try {
				close_session();
			} catch (session_exception &e) {
				error(__FILEW__, __LINE__, _T("Failed to close session, probably leaking memory and handles: ") + e.what());
			} catch (...) {
				error(__FILEW__, __LINE__, _T("Failed to close session, probably leaking memory and handles: Unknown exception"));
			}
			handler_=NULL;
		}
		void set_handler(session_handler_interface *handler) {
			handler_ = handler;
		}
		std::wstring get_client_id() const {
			return client_id_;
		}
		virtual std::wstring get_name() {
			return client_id_;
		}


		void error(std::wstring file, unsigned int line, std::wstring msg) {
			if (handler_)
				handler_->session_error(file, line, msg);
		}
		void info(std::wstring file, unsigned int line, std::wstring msg) {
			if (handler_)
				handler_->session_info(file, line, msg);
		}

		HANDLE get_signal_event() {
			if (client_channel_.get() == NULL && master_channel_.get() == NULL)
				throw session_exception(_T("Cant initalize session ") + client_id_ + _T(": no session!"));
			else if (client_channel_.get() == NULL)
				return master_channel_->getSignalHandle();
			else
				return client_channel_->getSignalHandle();
		}
		virtual void on_attach_client(std::wstring client_id) {
			remote_channel *rc = new remote_channel(this, client_id);
			rc->create_passive();
			remote_channels_[client_id] = rc;
			info(__FILEW__, __LINE__, client_id + _T(" says hello!"));
		}

		virtual void add_client_channel(std::wstring client_id) {
			info(__FILEW__, __LINE__, _T("Creating session for: ") + client_id);
			remote_channel *rc = new remote_channel(this, client_id);
			rc->create_passive();
			remote_channels_[client_id] = rc;
		}
		void remove_client(std::wstring client_id) {
			handle_type::iterator it = remote_channels_.find(client_id);
			if (it == remote_channels_.end())
				throw session_exception(_T("Failed to detach client: ") + client_id + _T(": client was not found."));
			remote_channel *tmp = (*it).second;
			remote_channels_.erase(it);
			delete tmp;
		}

		virtual void on_detach_client(std::wstring client_id) {
			remove_client(client_id);
			info(__FILEW__, __LINE__, client_id + _T(" says bye bye!"));
		}

		inline remote_channel::local_message_type read() {
			if (client_channel_.get() == NULL && master_channel_.get() == NULL)
				throw session_exception(_T("Cant read from session ") + client_id_ + _T(": no session!"));
			else if (client_channel_.get() == NULL)
				return master_channel_->read();
			else
				return client_channel_->read();
		}

		virtual void on_master_shutdown(std::wstring client_id) = 0;
		virtual void on_master_attach(std::wstring client_id) = 0;

		bool handle_raw_message() {
			remote_channel::local_message_type msg = read();
			if (msg.command == message_attach) {
				if (client_channel_.get() != NULL)
					throw session_exception(_T("Cant attach ") + msg.sender + _T(" to a client session!"));
				on_attach_client(msg.sender);
			} else if (msg.command == message_detach) {
				if (client_channel_.get() != NULL)
					throw session_exception(_T("Cant detach ") + msg.sender + _T(" to a client session!"));
				on_detach_client(msg.sender);
			} else if (msg.command == message_master_shutdown) {
				if (client_channel_.get() == NULL)
					throw session_exception(_T("Cant handle master shutdown from ") + msg.sender + _T(" not a client session!"));
				on_master_shutdown(msg.sender);
				//return false;
			} else if (msg.command == message_master_attach) {
				error(__FILEW__, __LINE__, _T("Got attach from master!"));
				if (client_channel_.get() == NULL)
					throw session_exception(_T("Cant handle master attach from ") + msg.sender + _T(" not a client session!"));
				on_master_attach(msg.sender);
				//on_master_shutdown(msg.sender);
				//return false;
			} else if (msg.command.size() == 0)
				throw session_exception(_T("Invalid message from ") + msg.sender);
			else
				handle_message(msg);
			return true;
		}

		virtual void handle_message(remote_channel::local_message_type &msg) = 0;


		virtual void create_new_session() {
			try {
				master_channel_.reset(new remote_channel(this, client_id_));
				master_channel_->create();
				responder_.createThread(this);
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create shared memory arena: ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create shared memory arena: Unknown exception"));
			}
			info(__FILEW__, __LINE__, _T("Created new master session: ") + master_channel_->get_name());
		}

		virtual void close_session(DWORD dwTimeout = 5000) {
			try {
				if (client_channel_.get() != NULL) {
					//send_detach();
					client_channel_->close();
					client_channel_.reset();
				}
				if (!remote_channels_.empty()) {
					//send_master_shutdown();
					for (handle_type::iterator it = remote_channels_.begin(); it != remote_channels_.end(); ++it) {
						remote_channel *tmp = (*it).second;
						delete tmp;
					}
					remote_channels_.clear();
				}
				if (master_channel_.get() != NULL) {
					master_channel_->close();
					master_channel_.reset();
				}
				responder_.exitThread(dwTimeout);
			} catch (session_exception &e) {
				throw session_exception(_T("Failed to close shared session: ") + e.what());
			} catch (event_exception &e) {
				throw session_exception(_T("Failed to close shared session: ") + e.what());
			} catch (ThreadException &e) {
				throw session_exception(_T("Failed to close shared session: ") + e.e_);
			} catch (...) {
				throw session_exception(_T("Failed to close shared session: Unknown exception"));
			}
		}
		remote_channel::local_message_type wait_reply(unsigned int msg_id, DWORD dwTimeout = INFINITE) {
			event_handler *h = new event_handler();
			wait_list_[msg_id] = h;
			h->accuire(dwTimeout);
			return response_list_[msg_id];
		}
		inline unsigned int send_server(std::wstring cmd) {
			remote_channel::local_message_type msg(cmd);
			return send_server(msg);
		}
		unsigned int send_server(remote_channel::local_message_type &msg) {
			//if (client_channel_.get() == NULL)
			//	throw session_exception(_T("Master session cant send to self!"));
			if (master_channel_.get() == NULL)
				throw session_exception(_T("Master session cant be down!"));
			msg.message_id = message_id_++;
			msg.sender = client_id_;
			try {
				master_channel_->write(msg);
			} catch (session_exception e) {
				error(__FILEW__, __LINE__, _T("Server channel is down: ") + e.what());
			}
			return msg.message_id;
		}
		inline void send_clients(std::wstring cmd) {
			remote_channel::local_message_type msg(cmd);
			send_clients(msg);
		}
		void send_clients(remote_channel::local_message_type &msg) {
			if (client_channel_.get() != NULL)
				throw session_exception(_T("Server channels cant have client sessions..."));
			if (remote_channels_.empty())
				return;
			msg.message_id = message_id_++;
			msg.sender = client_id_;
			for (handle_type::iterator it = remote_channels_.begin(); it != remote_channels_.end(); ++it) {
				try {
					(*it).second->write(msg);
				} catch (session_exception e) {
					remove_client((*it).first);
					error(__FILEW__, __LINE__, _T("Client channel is down (removing it): ") + e.what());
					return;
				}
			}
		}
		inline void send_client(std::wstring client, std::wstring cmd) {
			remote_channel::local_message_type msg(cmd);
			send_client(client, msg);
		}
		void send_client(std::wstring client, remote_channel::local_message_type &msg) {
			if (client_channel_.get() != NULL)
				throw session_exception(_T("Server channels cant have client session..."));
			if (remote_channels_.empty())
				return;
			msg.sender = client_id_;
			handle_type::iterator it = remote_channels_.find(client);
			if (it != remote_channels_.end()) {
				try {
					(*it).second->write(msg);
				} catch (session_exception e) {
					error(__FILEW__, __LINE__, _T("Client channel is down: ") + e.what());
				}
			}
		}
	};

	class shared_client_session : public shared_session {
		std::wstring channel_id_;

	public:
		shared_client_session(std::wstring channel, session_handler_interface *handler) 
			: shared_session(channel, handler), channel_id_(channel) {}
		virtual ~shared_client_session() {}

		void create_new_session() {
			throw session_exception(_T("Client channel cant create master session!"));
		}

		void open_master_channel() {
			try {
				master_channel_.reset(new remote_channel(this, master_id_));
				master_channel_->open();
			} catch (session_exception e) {
				error(__FILEW__, __LINE__, _T("Failed to attach to master channel: ") + e.what());
				master_channel_.reset();
			} catch (...) {
				error(__FILEW__, __LINE__, _T("Failed to attach to remote channel: Unknown exception"));
				master_channel_.reset();
			}
			if (master_channel_.get() != NULL) {
				info(__FILEW__, __LINE__, _T("Attempting to attach to: ") + master_channel_->get_name());
				try {
					send_attach();
				} catch (session_exception e) {
					throw session_exception(_T("Failed to send attach message: ") + e.what());
				} catch (...) {
					throw session_exception(_T("Failed to send attach message: Unknown exception"));
				}
			}
		}
		void wait_for_my_channel() {
			if (!remote_channel::exists(client_id_)) {
				info(__FILEW__, __LINE__, _T("Waiting for shared memory area to be created by core."));
				int i=0;
				for (;i<10;i++) {
					Sleep(1000*i);
					info(__FILEW__, __LINE__, _T("Waiting... (") + client_id_ + _T(")"));
					if (remote_channel::exists(client_id_))
						break;
				}
				if (i == 10)
					throw session_exception(_T("Failed to open shared mempory (was never created) for: ") + client_id_);
			}
		}
		void start_my_channel() {
			try {
				client_channel_.reset(new remote_channel(this, client_id_));
				client_channel_->open();
				client_channel_->activate();
				responder_.createThread(this);
			} catch (event_exception e) {
				throw session_exception(_T("Failed to create shared memory arena: ") + e.what());
			} catch (session_exception e) {
				throw session_exception(_T("Failed to create shared memory arena: ") + e.what());
			} catch (...) {
				throw session_exception(_T("Failed to create shared memory arena: Unknown exception"));
			}
		}
		virtual void attach_to_session(std::wstring channel) {
			client_id_ = channel;
			info(__FILEW__, __LINE__, _T("Attaching to remote session..."));
			open_master_channel();
			wait_for_my_channel();
			if (master_channel_.get() == NULL)
				open_master_channel();
			start_my_channel();
			info(__FILEW__, __LINE__, _T("Created new client: ") + client_channel_->get_name());
		}
		bool session_exists() const {
			return remote_channel::exists(master_id_);
		}

		void handle_message(remote_channel::local_message_type &msg) {
			message_list_type::iterator it = wait_list_.find(msg.message_id);
			if (it != wait_list_.end()) {
				response_list_[msg.message_id] = msg;
				(*it).second->set();
			} else if (msg.command == message_log) {
				if (handler_)
					handler_->session_log_message(strEx::stoi(msg.arguments[0]), msg.arguments[1].c_str(), strEx::stoi(msg.arguments[2]), msg.arguments[3]);
			} else {
				error(__FILEW__, __LINE__, _T("Unknown command: ") + msg.command);
			}
		}

		std::pair<std::wstring,std::wstring> get_client_name() {
			remote_channel::local_message_type msg;
			msg.command = message_get_name;
			unsigned int msg_id = send_server(msg);
			remote_channel::local_message_type response = wait_reply(msg_id);
			if (response.arguments.size() != 2)
				return std::pair<std::wstring,std::wstring>(_T("Unknown: Failed to get service name"),_T(""));
			return std::pair<std::wstring,std::wstring>(response.arguments[0],response.arguments[1]);
		}
		int inject(std::wstring command, std::wstring arguments, TCHAR splitter, bool escape, std::wstring &message, std::wstring & perf) {
			remote_channel::local_message_type msg;
			msg.command = message_inject;
			
			msg.arguments.push_back(command);
			msg.arguments.push_back(arguments);
			msg.arguments.push_back(strEx::ctos(splitter));
			msg.arguments.push_back(escape?_T("1"):_T("0"));

			//error(__FILEW__, __LINE__, _T("arg1") + msg.arguments[1]);
			//error(__FILEW__, __LINE__, _T("arg2") + msg.arguments[2]);
			//error(__FILEW__, __LINE__, _T("arg3") + msg.arguments[3]);

			unsigned int msg_id = send_server(msg);
			remote_channel::local_message_type response = wait_reply(msg_id);
			if (response.arguments.size() != 3)
				return -1;
			int code = strEx::stoi(response.arguments[0]);
			message = response.arguments[1];
			perf = response.arguments[2];
			return code;
		}
		bool hasMaster() const {
			return master_channel_.get() != NULL && client_channel_.get() != NULL;
		}
		void send_attach() {
			send_server(message_attach);
		}
		void send_detach() {
			send_server(message_detach);
		}
		virtual void close_session(DWORD dwTimeout = 5000) {
			try {
				send_detach();
			} catch (session_exception &e) {
				error(__FILEW__, __LINE__, _T("Failed to send detach message to master: ") + e.what());
			} catch (...) {
				error(__FILEW__, __LINE__, _T("Failed to send detach message to master: Unknown exception"));
			}
			shared_session::close_session(dwTimeout);
		}
		virtual void on_master_shutdown(std::wstring client_id) {
			//client_channel_.reset();
			master_channel_.reset();
			info(__FILEW__, __LINE__, client_id + _T(" has shut down!"));
		}
		virtual void on_master_attach(std::wstring client_id) {
			//client_channel_.reset();
			open_master_channel();
			info(__FILEW__, __LINE__, client_id + _T(" has attached it self!"));
		}

	};



	class shared_server_session : public shared_session {
	private:
		handle_type detached_channels_;

	public:
		shared_server_session(session_handler_interface *handler, std::wstring title = _T("server")) : shared_session(title, handler) {}
		virtual ~shared_server_session() {
			if (!detached_channels_.empty()) {
				for (handle_type::iterator it = detached_channels_.begin(); it != detached_channels_.end(); ++it) {
					remote_channel *tmp = (*it).second;
					delete tmp;
				}
				detached_channels_.clear();
			}
		}

		void attach_to_session() {
			throw session_exception(_T("Master channel cant attach to a master session!"));
		}
		bool session_exists() const {
			return remote_channel::exists(client_id_);
		}
		std::wstring create_child_channel(DWORD dwSessionId) {
			std::wstring id = _T("__") + strEx::itos(dwSessionId) + _T("__");
			add_client_channel(id);
			return id;
		}
		bool re_attach_client(DWORD dwSessionId) {
			std::wstring id = _T("__") + strEx::itos(dwSessionId) + _T("__");
			handle_type::iterator it = remote_channels_.find(id);
			if (it != remote_channels_.end())
				return true;
			if (remote_channel::exists(id)) {
				add_client_channel(id);
				send_master_attach(id);
				return true;
			}
			add_detached_client_channel(id);
			return false;
		}

		virtual void add_detached_client_channel(std::wstring client_id) {
			info(__FILEW__, __LINE__, _T("Creating session for: ") + client_id);
			remote_channel *rc = new remote_channel(this, client_id);
			rc->create_passive();
			detached_channels_[client_id] = rc;
		}
		virtual void on_attach_client(std::wstring client_id) {
			handle_type::iterator cit = remote_channels_.find(client_id);
			if (cit != remote_channels_.end()) {
				info(__FILEW__, __LINE__, client_id + _T(" says hello (again, probably bad)!"));
				// TODO: reconnect here?
				return;
			}
			cit = detached_channels_.find(client_id);
			if (cit != detached_channels_.end()) {
				info(__FILEW__, __LINE__, client_id + _T(" says hello (was detached)!"));
				remote_channels_[client_id] = (*cit).second;
				detached_channels_.erase(cit);
				return;
			}
			remote_channel *rc = new remote_channel(this, client_id);
			rc->create_passive();
			remote_channels_[client_id] = rc;
			info(__FILEW__, __LINE__, client_id + _T(" says hello!"));
		}

		void handle_message(remote_channel::local_message_type &msg) {
			if (msg.command == message_inject) {
				std::wstring message;
				std::wstring perf;
				int ret = -1;
				if (msg.arguments.size() == 4) {
					try {
						ret = handler_->session_inject(msg.arguments[0], msg.arguments[1], strEx::stoc(msg.arguments[2]), msg.arguments[3]==_T("1"), message, perf);
					} catch (...) {
						message = _T("Unknown exception!");
					}
				} else {
					message = _T("Error invalid message!");
				}
				remote_channel::local_message_type response;
				response.command = message_inject;
				response.message_id = msg.message_id;
				response.arguments.push_back(strEx::itos(ret));
				response.arguments.push_back(message);
				response.arguments.push_back(perf);
				send_client(msg.sender, response);
			} else if (msg.command == message_get_name) {
				std::wstring name;
				std::wstring version;
				if (msg.arguments.size() == 0) {
					try {
						std::pair<std::wstring,std::wstring> tmp = handler_->session_get_name();
						name = tmp.first;
						version = tmp.second;
					} catch (...) {
						name = _T("Unknown exception!");
					}
				} else {
					name = _T("Failed to get name!");
				}
				remote_channel::local_message_type response;
				response.command = message_get_name;
				response.message_id = msg.message_id;
				response.arguments.push_back(name);
				response.arguments.push_back(version);
				send_client(msg.sender, response);
			} else {
				error(__FILEW__, __LINE__, _T("Unknown command: ") + msg.command);
			}
		}
		void sendLogMessageToClients(int msgType, const TCHAR* file, const int line, std::wstring message) {
			remote_channel::local_message_type msg;
			msg.command = message_log;
			msg.arguments.push_back(strEx::itos(msgType));
			msg.arguments.push_back(file);
			msg.arguments.push_back(strEx::itos(line));
			msg.arguments.push_back(message);
			send_clients(msg);
		}
		bool hasClients() const {
			return master_channel_.get() != NULL && !remote_channels_.empty();
		}
		void send_master_shutdown() {
			send_clients(message_master_shutdown);
		}
		void send_master_attach(std::wstring client) {
			send_client(client, message_master_attach);
		}
		virtual void close_session(DWORD dwTimeout = 5000) {
			send_master_shutdown();
			shared_session::close_session(dwTimeout);
		}
		virtual void on_master_shutdown(std::wstring client_id) {
			error(__FILEW__, __LINE__, client_id + _T(" has shut down?!?!"));
		}
		virtual void on_master_attach(std::wstring client_id) {
			error(__FILEW__, __LINE__, client_id + _T(" has attached it self?!?!"));
		}
	};
}

