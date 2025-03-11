#include "Server.h"
#include "ServerImpl.h"

Mongoose::Server* Mongoose::Server::make_server(WebLoggerPtr logger) { return new ServerImpl(logger); }
