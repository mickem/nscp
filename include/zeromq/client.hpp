#pragma once

#include <boost/thread.hpp>

#include <zeromq/interface.hpp>

namespace zeromq {

	struct zeromq_base_client_reader : public zeromq::zeromq_reader {
		void error(std::string problem) {
			NSC_LOG_ERROR(to_wstring(problem));
		}
	};

	struct handshake_interface {
		virtual std::string get_remote_cookie() = 0;
		virtual bool validate_cookie(nscp::packet &packet) = 0;
	};

	struct zeromq_client_message_reader : public zeromq_base_client_reader {
		nscp::packet request, response;
		handshake_interface *handshaker;
		zeromq_client_message_reader(handshake_interface *handshaker, nscp::packet packet) : handshaker(handshaker), request(packet) {}

		nscp::packet get_outbound() {
			request.signature.cookie = handshaker->get_remote_cookie();
			return request;
		}
		bool process_inbound(nscp::packet &packet) {
			response = packet;
			if (!handshaker->validate_cookie(packet)) {
				error("Invalid cookie in request");
			}
			return true;
		}
	};

	struct zeromq_client_handshake_reader : public zeromq_base_client_reader, public handshake_interface {
		nscp::packet request, response;
		std::string remote_cookie;
		std::string local_cookie;
		zeromq_client_handshake_reader(std::string local_cookie, nscp::packet packet) : local_cookie(local_cookie), request(packet) {}

		nscp::packet get_outbound() {
			request.signature.cookie = local_cookie;
			return request;
		}
		bool process_inbound(nscp::packet &packet) {
			response = packet;
			if (nscp::checks::is_message_envelope_response(packet)) {
				NSCPIPC::MessageResponseEnvelope e;
				e.ParseFromString(response.payload);
				remote_cookie = e.cookie();
			}
			return true;
		}

		std::string get_remote_cookie() {
			return remote_cookie;
		}

		bool validate_cookie(nscp::packet &packet) {
			return packet.signature.cookie == local_cookie;
		}

	};


	struct zeromq_client {
		struct connection_info {
			connection_info(std::string host, int timeout) : host(host), timeout(timeout), retries(1), retry_delay(1) {}
			std::string host;
			int retries;
			int timeout;
			int retry_delay;
		};

		connection_info info;
		zmq::context_t *context;
		boost::shared_ptr<zmq::socket_t> client;
		zeromq::zeromq_reader_interface *handshake_handler;
		std::string cookie;
		zeromq_client(connection_info info, zeromq::zeromq_reader_interface *handshake_handler, zmq::context_t *context) : info(info), handshake_handler(handshake_handler), context(context) {}
		~zeromq_client() {
			disconnect();
		}
		void disconnect() {
#ifdef ZMQ_LINGER
			if (client)
				client->close();
#endif
			client.reset();
		}

		void connect() {
			if (client)
				disconnect();
			NSC_DEBUG_MSG(_T("I: (re)connecting to server: ") + to_wstring(info.host));
			client.reset(new zmq::socket_t(*context, ZMQ_REQ));
			client->connect(info.host.c_str());

			int linger = 0;
#ifdef ZMQ_LINGER
			client->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));
#endif

			if (!do_handshake()) {
				disconnect();
			}
		}



		bool do_handshake() {
			int retries_left = info.retries;
			try {
				while (retries_left) {
					if (!client)
						return false;
					handshake_handler->send(client);
					bool expect_reply = true;
					while (expect_reply) {
						zmq::pollitem_t items[] = { { *client, 0, ZMQ_POLLIN, 0 } };
						int i = zmq::poll(items, 1, info.timeout*1000000);

						//  If we got a reply, process it
						if (items[0].revents & ZMQ_POLLIN) {
							return handshake_handler->read(client);
						} else {
							return false;
						}
					}
				}
			} catch (const std::exception &e) {
				handshake_handler->error(std::string("Exception sending payload: ") + e.what());
			}
			return false;
		}

		bool send(zeromq::zeromq_reader_interface *handler) {
			int retries_left = info.retries;
			try {
				while (retries_left) {
					if (!client)
						connect();
					if (!client) {
						if (--retries_left <= 0) {
							handler->error("E: server seems to be offline, abandoning");
							return false;
						} else {
							NSC_DEBUG_MSG(_T("W: no response from server, retrying..."));
							disconnect();
							boost::this_thread::sleep(boost::posix_time::seconds(info.retry_delay));
							continue;
						}
					}
					handler->send(client);
					bool expect_reply = true;
					while (expect_reply) {
						zmq::pollitem_t items[] = { { *client, 0, ZMQ_POLLIN, 0 } };
						int i = zmq::poll(items, 1, info.timeout*1000000);

						//  If we got a reply, process it
						if (items[0].revents & ZMQ_POLLIN) {
							handler->read(client);
							return true;
						} else {
							expect_reply = false;
							if (--retries_left <= 0) {
								handler->error("E: server seems to be offline, abandoning");
								disconnect();
								return false;
							} else {
								NSC_DEBUG_MSG(_T("W: no response from server, retrying..."));
								disconnect();
								boost::this_thread::sleep(boost::posix_time::seconds(info.retry_delay));
								expect_reply = false;
							}
						}
					}
				}
			} catch (const std::exception &e) {
				handler->error(std::string("Exception sending payload: ") + e.what());
			}
			return false;
		}
	};




	struct client_manager {
		static client_manager *instance;
		static client_manager* get() {
			if (instance == NULL)
				instance = new client_manager();
			return instance;
		}
		static void destroy() {
			client_manager *tmp = instance;
			instance = NULL;
			delete tmp;
		}

		static bool is_authorized(std::string cookie) {
			return cookie == "1234567890";
		}
		static bool authorize(std::string user, std::string password, std::string &error) {
			return true;
		}

	};
	static client_manager *instance = NULL;
	struct zeromq_server_reader : public zeromq::zeromq_reader {
		boost::shared_ptr<nscp::server::server_handler> handler;
		nscp::packet response;
		std::string cookie;
		zeromq_server_reader(boost::shared_ptr<nscp::server::server_handler> handler) : handler(handler) {}

		bool process_inbound(nscp::packet &packet) {
			if (packet.signature.payload_type == nscp::data::message_envelope_request) {
				cookie = packet.signature.cookie;
				std::string cookie = "1234567890";
				response = nscp::factory::create_message_envelope_response(cookie, 0);
			} else {
				if (!client_manager::is_authorized(packet.signature.cookie)) {
					response = nscp::factory::create_error(_T("unautorized client connecting as: ") + to_wstring(packet.signature.cookie));
				} else {
					response = handler->process(packet);
				}
			}
			return true;
		}
		std::string get_cookie() {
			return cookie;
		}
		nscp::packet get_outbound() {
			return response;
		}
		void error(std::string problem) {
			NSC_LOG_ERROR(to_wstring(problem));
		}
	};


}