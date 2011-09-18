#pragma once

#include <boost/thread.hpp>
#include <zmq.hpp>

#include <zeromq/client.hpp>


namespace zeromq_worker {
	struct connection_info {
		connection_info(std::string backend, std::string suffix, int thread_count) : backend(backend), suffix(suffix), thread_count(thread_count) {}
		std::string backend;
		std::string suffix;
		unsigned int thread_count;
	};
	struct worker_manager {
		static void worker_thread(zmq::context_t *context, std::string backend, std::string identity) {
			boost::shared_ptr<nscp::server::server_handler> handler;
			handler.reset(new handler_impl());

			try {
				boost::shared_ptr<zmq::socket_t> worker = boost::shared_ptr<zmq::socket_t>(new zmq::socket_t(*context, ZMQ_REQ));

				NSC_DEBUG_MSG_STD(_T("Worker ") + to_wstring(identity) + _T(" connecting to:") + to_wstring(backend));
				worker->setsockopt(ZMQ_IDENTITY, (const void*)identity.c_str(), identity.length());
				worker->connect(backend.c_str());

				//  Tell queue we're ready for work
				NSC_DEBUG_MSG_STD(_T("Worker ready: ") + to_wstring(identity));
				zxmsg msg("READY");
				worker->send(msg);

				while (1) {
					zeromq::zeromq_server_reader reader(handler);
					if (!reader.read_wrapped(worker)) 
						break;
					reader.send_wrapped(worker);
				}
			} catch (const zmq::error_t &e) {
				NSC_LOG_ERROR_STD(utf8::cvt<std::wstring>(e.what()));
			}
		}

		void start(zmq::context_t *context, boost::thread_group &threads, connection_info info) {
			for (std::size_t i = 0; i < info.thread_count; ++i) {
				std::string identity = "worker." + info.suffix + "." + to_string(i);
				boost::thread *t = new boost::thread(worker_manager::worker_thread, context, info.backend, identity);
				threads.add_thread(t);
			}
		}
	};
};