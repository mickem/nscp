#include "server.hpp"
#include "connection.hpp"


namespace nsca {
	namespace server {
		server_definition::connection_type* server_definition::create(boost::asio::io_service& io_service, handler_type handler, connection_info_type info) {
			return new nsca::server::connection(io_service, handler);
		}
	}
}

