#include "ServerImpl.h"

#include "StreamResponse.h"
#include "mcp_exception.hpp"

#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <string>

using namespace std;
using namespace Mongoose;

struct oubound_frame {
	job_id id;
	ServerImpl *server;
	char *data;
	int len;
};

struct udt {
	ServerImpl *server;
	unsigned long job_id;
	unsigned long magic;

	udt(ServerImpl *server)
		: server(server)
		, job_id(0)
		, magic(123456789) 
	{}
	~udt() {
		server = NULL;
		job_id = 0;
		magic = 0;
	}
};

void sendStockResponse(struct mg_connection *connection, int code, std::string msg) {
	StreamResponse response;
	response.setCode(code);
	response.append(msg);
	std::string buffer = response.getData();
	mg_send(connection, buffer.c_str(), buffer.size());
	connection->flags |= MG_F_SEND_AND_CLOSE;
}


void on_wake_up(struct mg_connection *connection, int ev, void *ev_data) {
	if (ev_data == NULL || connection->user_data == NULL) {
		return;
	}
	oubound_frame *frame = reinterpret_cast<oubound_frame*>(ev_data);
	if (frame->server->execute_reply_async(frame->id, frame->data, frame->len)) {
		char *tmp = frame->data;
		frame->data = NULL;
		delete[] tmp;
	}
}
job_id job_index = 1;

static void *server_poll(void *param)
{
    ServerImpl *server = (ServerImpl*)param;
	if (server != NULL) {
		server->poll();
	}
    return NULL;
}

boost::posix_time::ptime now() {
	return boost::get_system_time();
}

void ServerImpl::request_thread_proc(int id) {
	try {
		job_queue_type::value_type instance;
		while (!stopped) {
			instance = job_queue_.pop();
			if (!instance) {
				boost::unique_lock<boost::mutex> lock(idle_thread_mutex_);
				idle_thread_cond_.wait(lock);
				continue;
			}

			try {
				if (instance->is_late(now())) {
					instance->toLate();
				} else {
					instance->run();
				}
			} catch (const boost::thread_interrupted &) {
				if (stopped) {
					return;
				}
				continue;
			} catch (...) {
				// Log error
				continue;
			}
		}
	} catch (const boost::thread_interrupted &e) {
	} catch (const std::exception &e) {
	} catch (...) {
	}
}

namespace Mongoose
{
	ServerImpl::ServerImpl(std::string port_)
        : stopped(false)
		, destroyed(true)
		, port(port_)
    {
		memset(&mgr, 0, sizeof(struct mg_mgr));
		mg_mgr_init(&mgr, NULL);
		memset(&opts, 0, sizeof(struct mg_bind_opts));
    }

	ServerImpl::~ServerImpl() {
        stop();
		vector<Controller *>::iterator it;
		for (it = controllers.begin(); it != controllers.end(); it++) {
			delete (*it);
		}
		controllers.clear();
    }

	void ServerImpl::request_reply_async(job_id id, std::string data) {
		oubound_frame frame;
		frame.server = this;
		frame.id = id;
		frame.len = data.size();
		frame.data = new char[frame.len + 10];
		memcpy(frame.data, data.c_str(), frame.len);
		mg_broadcast(&mgr, on_wake_up, (void*)&frame, sizeof(frame));
	}

	bool ServerImpl::execute_reply_async(job_id id, const void *buf, int len) {
		struct mg_connection *c;
		if (buf == NULL) {
			return false;
		}
		for (c = mg_next(&mgr, NULL); c != NULL; c = mg_next(&mgr, c)) {
			if (c->user_data != NULL && ((udt*)c->user_data)->job_id == id) {
				mg_send(c, buf, len);
				c->flags |= MG_F_SEND_AND_CLOSE;
				return true;
			}
		}
		return false;
	}

	void ServerImpl::setSsl(const char *certificate) {
		opts.ssl_cert = certificate;
	}


    void ServerImpl::start(int thread_count) {
		const char *err = "";
		opts.error_string = &err;
		opts.user_data = this;
		server_connection = mg_bind_opt(&mgr, port.c_str(), ServerImpl::event_handler, opts);
		if (server_connection == NULL) {
			throw mcp::mcp_exception(err);
		}
		stopped = false;
		mg_set_protocol_http_websocket(server_connection);

		int thread_count_ = 10;

		for (std::size_t i = 0; i < thread_count; i++) {
			boost::function<void()> f = boost::bind(&ServerImpl::request_thread_proc, this, 100 + i);
			threads_.createThread(f);
		}
		mg_start_thread(server_poll, this);
    }

    void ServerImpl::poll()
    {
		if (!stopped)
			destroyed = false;
        while (!stopped) {
			mg_mgr_poll(&mgr, 1000);
        }

        destroyed = true;
		mg_mgr_free(&mgr);
    }

    void ServerImpl::stop()
    {
		threads_.interruptThreads();
		threads_.waitForThreads();
        stopped = true;
        while (!destroyed) {
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }
    }

    void ServerImpl::registerController(Controller *controller) {
        controllers.push_back(controller);
    }

	void ServerImpl::event_handler(struct mg_connection *connection, int ev, void *ev_data) {
		if (connection->user_data != NULL) {
			if (ev == MG_EV_CLOSE) {
				udt *data = (udt*)connection->user_data;
				connection->user_data = NULL;
				if (data->magic == 123456789) {
					delete data;
				}
			} else if (ev == MG_EV_HTTP_REQUEST) {
				udt *data = new udt((ServerImpl*)connection->user_data);
				connection->user_data = data;
				if (job_index < 100 || job_index > 1000000) {
					job_index = 100;
				} else {
					job_index++;
				}
				data->job_id = job_index;
				struct http_message *message = (struct http_message *) ev_data;
				data->server->onHttpRequest(connection, message, data->job_id);
			}
		}
	}


	Request build_request(const std::string ip, struct http_message *message, bool is_ssl, const std::string method) {
		std::string url = std::string(message->uri.p, message->uri.len);
		std::string query;
		if (message->query_string.p != NULL) {
			query = std::string(message->query_string.p, message->query_string.len);
		}

		Request::headers_type headers;
		for (int i = 0; i < ARRAY_SIZE(message->header_names); i++) {
			std::string key = std::string(message->header_names[i].p, message->header_names[i].len);
			std::string value = std::string(message->header_values[i].p, message->header_values[i].len);
			headers[key] = value;
		}

		// Downloading POST data
		ostringstream postData;
		postData.write(message->body.p, message->body.len);
		std::string data = postData.str();
		return Request(ip, is_ssl, method, url, query, headers, data);

	}

    void ServerImpl::onHttpRequest(struct mg_connection *connection, struct http_message *message, job_id job_id) {

		bool is_ssl = (connection->flags&MG_F_SSL) == MG_F_SSL;
		std::string url = std::string(message->uri.p, message->uri.len);
		std::string method = std::string(message->method.p, message->method.len);

		for (int i = 0; i < ARRAY_SIZE(message->header_names); i++) {
			if (message->header_names[i].p != NULL && message->header_names[i].len > 0 &&
				strncmp(message->header_names[i].p, "X-HTTP-Method-Override", message->header_names[i].len) == 0) {
				method = std::string(message->header_values[i].p, message->header_values[i].len);
			}
		}

		BOOST_FOREACH(Controller *ctrl, controllers) {
			if (ctrl->handles(method, url)) {

				std::string ip = std::string(inet_ntoa(connection->sa.sin.sin_addr));
				Request request = build_request(ip, message, is_ssl, method);
				request_job job(this, ctrl, request, now(), job_id);

				if (!job_queue_.push(job)) {
					sendStockResponse(connection, HTTP_SERVER_ERROR, "Failed to process request");
				}
				idle_thread_cond_.notify_one();
				return;
			}
		}
		sendStockResponse(connection, HTTP_NOT_FOUND, "Document not found");
    }


	bool request_job::is_late(boost::posix_time::ptime now) {
		boost::posix_time::time_duration off = now - time;
		return off.total_seconds() > 30; // TODO: Should not be hard-coded
	}

	void request_job::run() {
		if (server != NULL && controller != NULL) {
			Response *resp = controller->handleRequest(request);
			if (resp) {
				server->request_reply_async(id, resp->getData());
				delete resp;
			} else {
				StreamResponse response;
				response.setCode(HTTP_SERVER_ERROR);
				response.append("No response from command");
				server->request_reply_async(id, response.getData());
			}
		}
	}

	void request_job::toLate() {
		StreamResponse response;
		response.setCode(HTTP_SERVICE_UNAVALIBLE);
		response.append("Server is overloaded, please try later");
		server->request_reply_async(id, response.getData());
	}

}
