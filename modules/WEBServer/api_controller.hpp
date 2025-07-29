#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <string>

#include "helpers.hpp"
#include "session_manager_interface.hpp"

class api_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;

 public:
  api_controller(const boost::shared_ptr<session_manager_interface> &session);

  void get_versions(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void get_eps(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
