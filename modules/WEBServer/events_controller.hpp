#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <boost/shared_ptr.hpp>

#include "event_store.hpp"
#include "session_manager_interface.hpp"

class events_controller : public Mongoose::RegexpController {
  boost::shared_ptr<session_manager_interface> session;
  boost::shared_ptr<event_store> events;

 public:
  events_controller(int version, const boost::shared_ptr<session_manager_interface> &session, const boost::shared_ptr<event_store> &events);

  void list_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void clear_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
