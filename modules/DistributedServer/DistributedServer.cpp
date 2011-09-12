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

#include <zmq.h>
#include <zmq.hpp>

#include "ZeroMQServer.h"
#include <strEx.h>
#include <time.h>
#include <queue>
#include <list>
#include <nscp/packet.hpp>
#include <boost/thread.hpp>

#include <config.h>
#include "handler_impl.hpp"

#include <settings/client/settings_client.hpp>



//
//  Asynchronous client-to-server (DEALER to ROUTER)
//
//  While this example runs in a single process, that is just to make
//  it easier to start and stop the example. Each task has its own
//  context and conceptually acts as a separate process.

#include <zmq.hpp>
#include <zmsg.hpp>


struct zxmsg : public zmq::message_t {

	zxmsg(std::string string) : zmq::message_t(string.size()) {
		memcpy(data(), string.data(), string.size());
	}
	static std::string read(zmq::message_t &msg) {
		return std::string(reinterpret_cast<char*>(msg.data()), msg.size());
	}

};

void start_queue(zmq::context_t *context, boost::thread_group *threads, int my_thread_count);

void queue_thread(zmq::context_t *context) {

    zmq::socket_t frontend(*context, ZMQ_XREP);
	zmq::socket_t backend(*context, ZMQ_XREP);
	zmq::socket_t control(*context, ZMQ_REP);
    frontend.bind("tcp://*:5555");
	backend.bind("inproc://backend");
	control.bind("inproc://control");

	std::queue<std::string> worker_queue;

	try {
		while (1) {

			zmq::pollitem_t items [] = {
				{ backend,  0, ZMQ_POLLIN, 0 },
				{ control,  0, ZMQ_POLLIN, 0 },
				{ frontend, 0, ZMQ_POLLIN, 0 }
			};
			//  Poll frontend only if we have available workers
			if (worker_queue.size())
				zmq::poll(items, 3, -1);
			else
				zmq::poll(items, 2, -1);

			//  Handle worker activity on backend
			if (items[0].revents & ZMQ_POLLIN) {
				NSC_DEBUG_MSG_STD(_T("---got worker msg---"));
				zmsg zm;
				if (!zm.recv(backend)) {
					NSC_DEBUG_MSG_STD(_T("---failed to read---"));
					return;
				}

				//  Use worker address for LRU routing
				//assert (worker_queue.size() < worker_count);

				zmsg::opstring_t s = zm.unwrap();
				if (s)
					worker_queue.push(*s);

				//  Return reply to client if it's not a READY
				zmsg::opstring_t a = zm.address();
				if (a && *a == "READY")
					zm.clear();
				else
					zm.send(frontend);
			}
			if (items[1].revents & ZMQ_POLLIN) {
				NSC_DEBUG_MSG_STD(_T("---got control msg---"));
				zmq::message_t query;
				if (!control.recv(&query)) {
					NSC_DEBUG_MSG_STD(_T("---failed to read---"));
					return;
				}
				std::string command = zxmsg::read(query);
				NSC_DEBUG_MSG_STD(_T("=>") + to_wstring(command));
				zxmsg resp("OK");
				control.send(resp);
			}
			if (items[2].revents & ZMQ_POLLIN) {
				NSC_DEBUG_MSG_STD(_T("---got client msg---"));
				//  Now get next client request, route to next worker

				zmsg zm;
				if (!zm.recv(frontend)) {
					NSC_DEBUG_MSG_STD(_T("---failed to read---"));
					return;
				}

				//  REQ socket in worker needs an envelope delimiter
				zm.wrap(worker_queue.front(), "");
				zm.send(backend);

				//  Dequeue and drop the next worker address
				worker_queue.pop();
			}
		}
	} catch (zmq::error_t &e) {
		NSC_LOG_ERROR(_T("Failed in process: ") + to_wstring(e.what()));
	}
}
int worker_thread(zmq::context_t *context, std::string foo);

void start_queue(zmq::context_t *context, boost::thread_group &threads, int my_thread_count) {
	boost::thread *t = new boost::thread(queue_thread, context);
	threads.add_thread(t);

	zmq::socket_t control(*context, ZMQ_REQ);
	for (int i=0;i<10;i++) {
		try {
			control.connect("inproc://control");
			NSC_LOG_ERROR_STD(_T("connected to control..."));
			zxmsg msg("start");
			if (!control.send(msg)) {
				NSC_LOG_ERROR_STD(_T("Failed to send start"));
				continue;
			}
			break;
		} catch (zmq::error_t &e) {
			NSC_LOG_ERROR_STD(_T("control: ") + utf8::cvt<std::wstring>(e.what()));
		}
		boost::this_thread::sleep(boost::posix_time::seconds(1));
	}
	zmq::message_t resp;
	control.recv(&resp);
	NSC_DEBUG_MSG(_T("Got: ") + to_wstring(zxmsg::read(resp)));


	for (std::size_t i = 0; i < my_thread_count; ++i) {
		boost::thread *t = new boost::thread(worker_thread, context, "wt_" + to_string(i));
		threads.add_thread(t);
	}
}

void stop_queue(zmq::context_t *context) {
	zmq::socket_t control(*context, ZMQ_REQ);
	for (int i=0;i<10;i++) {
		try {
			control.connect("inproc://control");
			NSC_LOG_ERROR_STD(_T("connected to control..."));
			zxmsg msg("stop");
			if (!control.send(msg)) {
				NSC_LOG_ERROR_STD(_T("Failed to send start"));
				continue;
			}
			break;
		} catch (zmq::error_t &e) {
			NSC_LOG_ERROR_STD(_T("control: ") + utf8::cvt<std::wstring>(e.what()));
		}
		boost::this_thread::sleep(boost::posix_time::seconds(1));
	}
}

int worker_count = 0;
int worker_thread(zmq::context_t *context, std::string key) {
	//boost::shared_ptr<nscp::handler> handler = new handler_impl();
	try {
		//zmq::context_t context(1);
		zmq::socket_t worker(*context, ZMQ_REQ);

		//  Set random identity to make tracing easier
		std::string identity = key;
		worker.connect("inproc://backend");

		//  Tell queue we're ready for work
		NSC_DEBUG_MSG_STD(_T("I: (") + to_wstring(identity) + _T(") worker ready"));
		zxmsg msg("READY");
		if (!worker.send(msg)) {
			NSC_DEBUG_MSG_STD(_T("FAIELD TO SEND!!!!!!"));
		}

		while (1) {
			zmsg zm;
			if (!zm.recv(worker)) {
				NSC_DEBUG_MSG_STD(_T("Failed to read in (") + to_wstring(identity) + _T(")"));
				return 0;
			}
			NSC_DEBUG_MSG_STD(_T("I: (") + to_wstring(identity) + _T(") from: ") + to_wstring(*zm.address()));
			NSC_DEBUG_MSG_STD(_T("I: (") + to_wstring(identity) + _T(") sent: ") + to_wstring(*zm.body()));
			//sleep (1);              //  Do some heavy work
			zm.send(worker);
		}
	} catch (const zmq::error_t &e) {
		NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(e.what()));
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////

static zmq::socket_t * s_client_socket(zmq::context_t* context) {
	NSC_DEBUG_MSG(_T("I: (re)connecting to server"));
	zmq::socket_t *client = new zmq::socket_t(*context, ZMQ_REQ);
	client->connect ("tcp://localhost:5555");

	//  Configure socket to not wait at close time
	//int linger = 0;
	//client->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
	return client;
}

int start_client (zmq::context_t *context, int request_retries, int timeout) {
	zmq::socket_t * client = s_client_socket(context);

	int sequence = 0;
	int retries_left = request_retries;

	while (retries_left) {
		std::stringstream request;
		request << ++sequence;
		zxmsg msg(request.str());
		client->send(msg);
		//sleep (1);

		bool expect_reply = true;
		while (expect_reply) {
			//  Poll socket for a reply, with timeout
			zmq::pollitem_t items[] = { { *client, 0, ZMQ_POLLIN, 0 } };
			zmq::poll (&items[0], 1, timeout * 1000);

			//  If we got a reply, process it
			if (items[0].revents & ZMQ_POLLIN) {
				//  We got a reply from the server, must match sequence
				zmq::message_t msg;
				client->recv(&msg);
				std::string reply(reinterpret_cast<char*>(msg.data()), msg.size());
				if (atoi (reply.c_str ()) == sequence) {
					NSC_DEBUG_MSG(_T("I: server replied OK (") + to_wstring(reply) + _T(")"));
					retries_left = request_retries;
					expect_reply = false;
				} else {
					NSC_DEBUG_MSG(_T("E: malformed reply from server (") + to_wstring(reply) + _T(")"));
				}
			}
			else
				if (--retries_left == 0) {
					NSC_DEBUG_MSG(_T("E: server seems to be offline, abandoning"));
					expect_reply = false;
					break;
				} else {
					NSC_DEBUG_MSG(_T("W: no response from server, retrying..."));
					//  Old socket will be confused; close it and open a new one
					delete client;
					client = s_client_socket(context);
					//  Send request again, on new socket
					zxmsg msg(request.str());
					client->send(msg);
				}
		}
	}
	delete client;
	return 0;
}


namespace sh = nscapi::settings_helper;

NSCPListener::NSCPListener() : context(NULL) {
}
NSCPListener::~NSCPListener() {
	delete context;
}

bool NSCPListener::loadModule() {
	return false;
}

bool NSCPListener::loadModuleEx(std::wstring alias, NSCAPI::moduleLoadMode mode) {
	try {
		/*
		sh::settings_registry settings(get_settings_proxy());
		settings.set_alias(_T("nscp"), alias, _T("server"));

		settings.alias().add_path_to_settings()
			(_T("NSCP SERVER SECTION"), _T("Section for NSCP (NSCPListener.dll) (check_nscp) protocol options."))
			;

		settings.alias().add_key_to_settings()
			(_T("port"), sh::uint_key(&info_.port, 5668),
			_T("PORT NUMBER"), _T("Port to use for NSCP."))

			;

		settings.alias().add_parent(_T("/settings/default")).add_key_to_settings()

			(_T("thread pool"), sh::uint_key(&info_.thread_pool_size, 10),
			_T("THREAD POOL"), _T(""))

			(_T("bind to"), sh::string_key(&info_.address),
			_T("BIND TO ADDRESS"), _T("Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses."))

			(_T("socket queue size"), sh::int_key(&info_.back_log, 0),
			_T("LISTEN QUEUE"), _T("Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts."))

			(_T("allowed hosts"), sh::string_fun_key<std::wstring>(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), _T("127.0.0.1")),
			_T("ALLOWED HOSTS"), _T("A comaseparated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges."))

			(_T("cache allowed hosts"), sh::bool_key(&info_.allowed_hosts.cached, true),
			_T("CACHE ALLOWED HOSTS"), _T("If hostnames should be cached, improves speed and security somewhat but wont allow you to have dynamic IPs for your nagios server."))

			(_T("timeout"), sh::uint_key(&info_.timeout, 30),
			_T("TIMEOUT"), _T("Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out."))

			(_T("use ssl"), sh::bool_key(&info_.use_ssl, true),
			_T("ENABLE SSL ENCRYPTION"), _T("This option controls if SSL should be enabled."))

			(_T("certificate"), sh::wpath_key(&info_.certificate, _T("${certificate-path}/nrpe_dh_512.pem")),
			_T("SSL CERTIFICATE"), _T(""))

			;

		settings.register_all();
		settings.notify();
		*/

		context = new zmq::context_t(5);
		start_queue(context, threads, 5);
		//start_client(context, 5, 10);
		//std::wcout << _T("---QUEUE STARTED---") << std::endl;
		//start_client(10, 30);

	} catch (std::exception &e) {
		NSC_LOG_ERROR_STD(_T("Exception caught: ") + to_wstring(e.what()));
		return false;
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN EXCEPTION>"));
		return false;
	}


	return true;
}

void free_z_buffer(void *data, void *hint) {
	delete [] reinterpret_cast<char*>(data);
}
void* create_z_buffer(std::string &buffer) {
	char *tmp = new char[buffer.size()+1];
	memcpy(tmp, buffer.c_str(), buffer.size());
	return tmp;
}



struct zeromq_server {
	zmq::socket_t socket;
	boost::shared_ptr<nscp::handler> handler;

	zeromq_server(zmq::context_t context, boost::shared_ptr<nscp::handler> handler) : socket(context, ZMQ_REP), handler(handler) {
	}

	void start_server(std::string address = "tcp://*:5555") {
		socket.bind(address.c_str());

		while (true) {
			zmq::message_t request;
			socket.recv(&request);

			std::list<nscp::packet> read_list;
			bool has_more = true;
			while (has_more) {
				nscp::packet packet;
				std::string buffer(reinterpret_cast<char*>(request.data()), request.size());
				packet.read_all(buffer);
				read_list.push_back(packet);
				std::cout << "<<< " << packet.to_string() << std::endl;
				has_more = packet.signature.additional_packet_count > 0;
			}
			std::list<nscp::packet> result = handler->process_all(read_list);
			send(result);
		}
	}
	void send(std::list<nscp::packet> packets) {
		unsigned long count = packets.size();
		BOOST_FOREACH(nscp::packet &packet, packets) {
			packet.signature.additional_packet_count = count--;
			std::cout << ">>> " << packet.to_string() << std::endl;
			std::string buffer = packet.write_string();
			zmq::message_t msg(create_z_buffer(buffer), buffer.size(), &free_z_buffer);
			socket.send(msg);
		}
	}
};

bool NSCPListener::unloadModule() {
	try {
		delete context;
		context = NULL;
		//stop_queue(context);
		threads.join_all();
	} catch (...) {
		NSC_LOG_ERROR_STD(_T("Exception caught: <UNKNOWN>"));
		return false;
	}
	return true;
}


bool NSCPListener::hasCommandHandler() {
	return false;
}
bool NSCPListener::hasMessageHandler() {
	return false;
}

NSC_WRAP_DLL();
NSC_WRAPPERS_MAIN_DEF(NSCPListener);
NSC_WRAPPERS_IGNORE_MSG_DEF();
NSC_WRAPPERS_IGNORE_CMD_DEF();
