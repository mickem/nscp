#include "Client.hpp"

#include "StreamResponse.h"
#include "string_response.hpp"

#include "ext/mongoose.h"

#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>

#include <string>

using namespace std;
using namespace Mongoose;

class Handler {

public:
	std::string error;
	int exit_flag;
	boost::shared_ptr<Response> response;

	Handler() : exit_flag(0) {}

	static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
		Handler *c = (Handler*)nc->user_data;
		c->handler(nc, ev, ev_data);
	}
	void handler(struct mg_connection *nc, int ev, void *ev_data) {
		struct http_message *hm = (struct http_message *) ev_data;

		switch (ev) {
		case MG_EV_CONNECT:
			if (*(int *)ev_data != 0) {
				error = std::string("connect() failed: ") + strerror(*(int *)ev_data);
				exit_flag = 1;
			}
			break;
		case MG_EV_HTTP_REPLY:
			nc->flags |= MG_F_CLOSE_IMMEDIATELY;
			parseReply(hm);
			exit_flag = 1;
			break;
		case MG_EV_CLOSE:
			if (exit_flag == 0) {
				exit_flag = 1;
			}
			break;
		default:
			break;
		}
	}

	bool is_done() {
		return exit_flag != 0;
	}

	void parseReply(struct http_message * hm) {
		size_t i = 0;
		response = boost::shared_ptr<Response>(new mcp::string_response(hm->resp_code, std::string(hm->body.p, hm->body.len)));
		for (i = 0; hm->header_names[i].len > 0; i++) {
			if (hm->header_names[i].p != NULL && hm->header_values[i].p != NULL) {
				response->setHeader(std::string(hm->header_names[i].p, hm->header_names[i].len),
					std::string(hm->header_values[i].p, hm->header_values[i].len));
			}
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

		Handler handler;
		struct mg_mgr mgr;
		int i;
		//memset(&mgr, 0, sizeof(struct mg_mgr));

		mg_mgr_init(&mgr, NULL);
		std::stringstream headers;
		BOOST_FOREACH(const header_type::value_type &v, hdr) {
			headers << v.first << ": " << v.second << "\r\n";
		}

		mg_connection *nc = mg_connect_http(&mgr, &Handler::ev_handler, url_.c_str(), verb.c_str(), headers.str().c_str(), payload.c_str());
		nc->user_data = &handler;

		while (!handler.is_done()) {
			mg_mgr_poll(&mgr, 1000);
		}
		mg_mgr_free(&mgr);
		return handler.response;
	}

}
