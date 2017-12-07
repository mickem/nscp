#include "Server.h"
#include "ServerImpl.h"

Mongoose::Server* Mongoose::Server::make_server(std::string port /*= "80"*/) {
	return new ServerImpl(port);
}
