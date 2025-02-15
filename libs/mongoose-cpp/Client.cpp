#include "Client.hpp"

#include "StreamResponse.h"
#include "string_response.hpp"

#include "ext/mongoose.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <string>
#include <list>

using namespace std;
using namespace Mongoose;

class Handler {

public:
	std::string payload_;
	std::string error;
	boost::shared_ptr<Response> response;

	Handler(std::string payload) : payload_(payload) {}


	void send(struct mg_connection* c, std::string data) {
		mg_send(c, data.c_str(), data.length());
	}


	static void ev_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
		Handler *handler = (Handler*)fn_data;
		handler->handler(c, ev, ev_data);
	}
	void handler(struct mg_connection *c, int ev, void *ev_data) {
		struct mg_http_message *hm = (struct mg_http_message *) ev_data;

		switch (ev) {
		case MG_EV_CONNECT:
			mg_send(c, payload_.c_str(), payload_.length());
			break;
		case MG_EV_ERROR:
			error = std::string("connect() failed: ") + strerror(*(int *)ev_data);
			break;
		case MG_EV_HTTP_MSG:
			parseReply(hm);
			break;
		default:
			break;
		}
	}

	void parseReply(struct mg_http_message * hm) {
		size_t i = 0;
		response = boost::shared_ptr<Response>(new mcp::string_response(mg_http_status(hm), std::string(hm->message.ptr, hm->message.len)));
		for (i = 0; hm->headers[i].name.len > 0; i++) {
			response->setHeader(std::string(hm->headers[i].name.ptr, hm->headers[i].name.len),
				std::string(hm->headers[i].value.ptr, hm->headers[i].value.len));
		}
	}
};
namespace Mongoose
{
    Client::Client(std::string url) : url_(url)
    {
    }

	Client::~Client() {
    }


	boost::shared_ptr<Response> Client::fetch(std::string verb, Client::header_type hdr, std::string payload) {
		struct mg_mgr mgr;

		mg_mgr_init(&mgr);
		std::stringstream request;

		struct mg_str host = mg_url_host(url_.c_str());
		std::string uri = mg_url_uri(url_.c_str());

		request << verb + " " + uri + " HTTP/1.0\r\n";
		for(const header_type::value_type &v: hdr) {
			request << v.first << ": " << v.second << "\r\n";
		}
		request << "\n\r";
		Handler handler(request.str());

		mg_http_connect(&mgr, url_.c_str(), &Handler::ev_handler, &handler);
		//while (!handler.is_done()) {
		mg_mgr_poll(&mgr, 1000);
		//}
		mg_mgr_free(&mgr);
		return handler.response;
	}

}
