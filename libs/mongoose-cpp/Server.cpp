#include "Server.h"

#include "ServerImpl.h"

Mongoose::Server* Mongoose::Server::make_server(const WebLoggerPtr& logger) { return new ServerImpl(logger); }
