#pragma once

#include <boost/asio.hpp>
#include <string>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <socket/socket_helpers.hpp>
#include <socket/server.hpp>
#include <nsca/server/connection.hpp>

#include "handler.hpp"

namespace nsca {

	namespace server {

		struct nsca_connection_info : public socket_helpers::connection_info {
			nsca_connection_info(boost::shared_ptr<nsca::server::handler> request_handler) : request_handler(request_handler) {}
			nsca_connection_info(const nsca_connection_info &other) 
				: socket_helpers::connection_info(other)
				, request_handler(other.request_handler)
			{}
			nsca_connection_info& operator=(const nsca_connection_info &other) {
				socket_helpers::connection_info::operator=(other);
				request_handler = other.request_handler;
				return *this;
			}
			boost::shared_ptr<nsca::server::handler> request_handler;
		};

	
		struct server_definition {
			typedef boost::shared_ptr<nsca::server::handler> handler_type;
			typedef nsca_connection_info connection_info_type;
			typedef nsca::server::connection connection_type;
			
			static connection_type* create(boost::asio::io_service& io_service, handler_type handler, connection_info_type info);
		};
		
		typedef socket_helpers::server::server<server_definition> nsca_server;
	} // namespace server
} // namespace nrpe
