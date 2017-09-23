#pragma once

#include "session_manager_interface.hpp"

#include <Controller.h>

#include <boost/filesystem/operations.hpp>

#include <string>

class StaticController : public Mongoose::Controller {
	boost::shared_ptr<session_manager_interface> session;
	boost::filesystem::path base;
public:
	StaticController(boost::shared_ptr<session_manager_interface> session, std::string path);

	Mongoose::Response *handleRequest(Mongoose::Request &request);
	bool handles(string method, string url);
};