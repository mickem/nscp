#include "ServerImpl.h"

#include "StreamResponse.h"
#include "mcp_exception.hpp"

#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <string>

using namespace std;
using namespace Mongoose;

boost::posix_time::ptime now() {
	return boost::get_system_time();
}


namespace Mongoose
{
	ServerImpl::ServerImpl()
		: stop_thread_(false)
    {
		memset(&mgr, 0, sizeof(struct mg_mgr));
		mg_mgr_init(&mgr);
    }

	ServerImpl::~ServerImpl() {
        stop();
		vector<Controller *>::iterator it;
		for (it = controllers.begin(); it != controllers.end(); it++) {
			delete (*it);
		}
		controllers.clear();
    }

	void ServerImpl::setSsl(const char *new_certificate) {
#if MG_ENABLE_OPENSSL
		certificate = new_certificate;
#else
        //NSC_LOG_ERROR("");
#endif
	}



    void ServerImpl::thread_proc() {
        try {
            while (true) {
                mg_mgr_poll(&mgr, 1000);
                if (stop_thread_) {
                    mg_mgr_free(&mgr);
                    return;
                }
            }
        } catch (...) {
            //NSC_LOG_ERROR("");
        }
    }


    void ServerImpl::start(std::string bind) {
        mg_http_listen(&mgr, bind.c_str(), ServerImpl::event_handler, (void*)this);
        thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&ServerImpl::thread_proc, this)));
    }

    void ServerImpl::stop()
    {
        if (thread_) {
            stop_thread_ = true;
            thread_->interrupt();
            thread_->join();
        }
        thread_.reset();
    }

    void ServerImpl::registerController(Controller *controller) {
        controllers.push_back(controller);
    }

	void ServerImpl::event_handler(struct mg_connection *connection, int ev, void *ev_data, void *fn_data) {
		if (fn_data != NULL) {
            ServerImpl *impl = (ServerImpl*)fn_data;
#if MG_ENABLE_OPENSSL
            if (ev == MG_EV_ACCEPT) {
                impl->initTls(connection);
            }
#endif
			if (ev == MG_EV_HTTP_MSG) {
				struct mg_http_message *message = (struct mg_http_message *) ev_data;
                impl->onHttpRequest(connection, message);
			}
		}
	}
#if MG_ENABLE_OPENSSL
    void ServerImpl::initTls(struct mg_connection *connection) {
        if (certificate.empty()) {
            return;
        }
    }
#endif

	Request build_request(const std::string ip, struct mg_http_message *message, bool is_ssl, const std::string method) {
		std::string url = std::string(message->uri.ptr, message->uri.len);
		std::string query;
		if (message->query.ptr != NULL) {
			query = std::string(message->query.ptr, message->query.len);
		}

		Request::headers_type headers;
        size_t max = sizeof(message->headers) / sizeof(message->headers[0]);
        for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
			std::string key = std::string(message->headers[i].name.ptr, message->headers[i].name.len);
			std::string value = std::string(message->headers[i].value.ptr, message->headers[i].value.len);
			headers[key] = value;
		}

		// Downloading POST data
		ostringstream postData;
		postData.write(message->body.ptr, message->body.len);
		std::string data = postData.str();
		return Request(ip, is_ssl, method, url, query, headers, data);

	}

    void ServerImpl::onHttpRequest(struct mg_connection *connection, struct mg_http_message *message) {

		bool is_ssl = connection->is_tls;
		std::string url = std::string(message->uri.ptr, message->uri.len);
		std::string method = std::string(message->method.ptr, message->method.len);

        size_t max = sizeof(message->headers) / sizeof(message->headers[0]);
        for (size_t i = 0; i < max && message->headers[i].name.len > 0; i++) {
            struct mg_str *k = &message->headers[i].name, *v = &message->headers[i].value;
			if (message->headers[i].value.len > 0 &&
				strncmp(message->headers[i].name.ptr, "X-HTTP-Method-Override", message->headers[i].name.len) == 0) {
				method = std::string(message->headers[i].value.ptr, message->headers[i].value.len);
			}
		}

		BOOST_FOREACH(Controller *ctrl, controllers) {
			if (ctrl->handles(method, url)) {

                char buf[100];
                mg_straddr(&connection->rem, buf, sizeof(buf));
				std::string ip = std::string(buf);
				Request request = build_request(ip, message, is_ssl, method);

                Response *response = ctrl->handleRequest(request);
				std::stringstream headers;
                bool has_content_type = false;
				BOOST_FOREACH(const Response::header_type::value_type & v, response->get_headers()) {
					headers << v.first << ": " << v.second << "\r\n";
                    if (v.first == "Content-Type") {
                        has_content_type = true;
                    }
				}
                headers << "Access-Control-Allow-Origin: *\r\n";
                if (response->getCode() == 200 && !has_content_type) {
                    headers << "Content-Type: application/json\r\n";
                }
                if (response->getCode() == 403 && !has_content_type) {
                    headers << "Content-Type: text/plain\r\n";
                }

                mg_http_reply(connection, response->getCode(), headers.str().c_str(), "%s", response->getBody().c_str());

				return;
			}
		}
        mg_http_reply(connection, HTTP_NOT_FOUND, "", "Document not found");
    }


}
