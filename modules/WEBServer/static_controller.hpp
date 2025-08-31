#pragma once

#include <Controller.h>

#include <boost/filesystem/operations.hpp>
#include <string>

#include "session_manager_interface.hpp"

class StaticController : public Mongoose::Controller {
  boost::shared_ptr<session_manager_interface> session;
  boost::filesystem::path base;

 public:
  StaticController(const boost::shared_ptr<session_manager_interface> &session, const std::string &path);

  Mongoose::Response *handleRequest(Mongoose::Request &request);
  bool handles(std::string method, std::string url);
};