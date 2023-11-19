#include "Server.h"
#include "ServerImpl.h"

Mongoose::Server* Mongoose::Server::make_server() {
	return new ServerImpl();
}
