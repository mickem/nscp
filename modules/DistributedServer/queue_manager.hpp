#pragma once
#include <queue>

#include <boost/thread.hpp>

#include <zmq.hpp>
#include <zmsg.hpp>

namespace zeromq_queue {
	struct connection_info {
		connection_info(std::string host, std::string suffix) : host(host), suffix(suffix) {}
		std::string suffix;
		std::string host;
		std::string get_control() {
			return std::string("inproc://control." + suffix);
		}
		std::string get_backend() {
			return std::string("inproc://backend." + suffix);
		}
	};

	struct queue_manager {

		static void queue_thread(zmq::context_t *context, connection_info info) {

			zmq::socket_t frontend(*context, ZMQ_XREP);
			zmq::socket_t backend(*context, ZMQ_XREP);
			zmq::socket_t control(*context, ZMQ_REP);
			frontend.bind(info.host.c_str());
			backend.bind(info.get_backend().c_str());
			control.bind(info.get_control().c_str());
			NSC_DEBUG_MSG_STD(_T("Queue frontend online on: ") + to_wstring(info.host));
			NSC_DEBUG_MSG_STD(_T("Queue backend online on: ") + to_wstring(info.get_backend()));
			NSC_DEBUG_MSG_STD(_T("Queue control online on: ") + to_wstring(info.get_control()));

			std::queue<std::string> worker_queue;
			try {
				while (1) {
					zmq::pollitem_t items [] = {
						{ backend,  0, ZMQ_POLLIN, 0 },
						{ control,  0, ZMQ_POLLIN, 0 },
						{ frontend, 0, ZMQ_POLLIN, 0 }
					};
					if (!zmq::poll(items, (worker_queue.size()>0)?3:2, -1)) {
						NSC_LOG_ERROR(_T("Queue got non read event..."));
						return;
					}

					if (items[0].revents & ZMQ_POLLIN) {
						zmsg zm;
						zm.recv(backend);
						zmsg::opstring_t s = zm.unwrap();
						if (s)
							worker_queue.push(*s);
						else
							NSC_LOG_ERROR(_T("Worker message was un addressed this is bad..."));
						NSC_DEBUG_MSG_STD(_T("Worker message was from: ") + to_wstring(*s));

						zmsg::opstring_t a = zm.address();
						if (a && *a == "READY")
							zm.clear();
						else {
							zm.send(frontend);
						}
					}
					if (items[1].revents & ZMQ_POLLIN) {
						zmq::message_t query;
						control.recv(&query);
						std::string command = zxmsg::read(query);
						std::string response = "UNKNOWN";
						if (command == "HELLO") {
							response = "OK";
						} else if (command == "WORKERS") {
							response = "SIZE=" + to_string(worker_queue.size());
						}
						zxmsg resp(response);
						control.send(resp);
					}
					if (items[2].revents & ZMQ_POLLIN) {
						zmsg zm(frontend);
						NSC_DEBUG_MSG_STD(_T("Got message from: ") + to_wstring(worker_queue.front()));
						zm.wrap(worker_queue.front(), "");
						zm.send(backend);
						worker_queue.pop();
					}
				}
			} catch (zmq::error_t &e) {
				NSC_LOG_ERROR(_T("Distributed NSCP process failure: ") + to_wstring(e.what()));
			}
		}


		void start(zmq::context_t *context, boost::thread_group &threads, connection_info info) {
			boost::thread *t = new boost::thread(queue_manager::queue_thread, context, info);
			threads.add_thread(t);

			zmq::socket_t control(*context, ZMQ_REQ);
			for (int i=0;i<10;i++) {
				try {
					control.connect(info.get_control().c_str());
					NSC_DEBUG_MSG_STD(_T("Connecting to queue controller on: ") + to_wstring(info.get_control()));
					zxmsg msg("HELLO");
					if (!control.send(msg)) {
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
			NSC_DEBUG_MSG(_T("Got control response: ") + to_wstring(zxmsg::read(resp)));
		}
	};
}