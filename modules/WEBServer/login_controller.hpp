#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/thread/shared_mutex.hpp>
#include <client/simple_client.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>

#include "helpers.hpp"
#include "session_manager_interface.hpp"

class login_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;

 public:
  login_controller(const int version, const boost::shared_ptr<session_manager_interface> &session);

  void is_loggedin(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
