#pragma once

#include <RegexController.h>
#include <StreamResponse.h>

#include <memory>

#include "event_store.hpp"
#include "session_manager_interface.hpp"

class events_controller : public Mongoose::RegexpController {
  std::shared_ptr<session_manager_interface> session;
  std::shared_ptr<event_store> events;

 public:
  events_controller(int version, const std::shared_ptr<session_manager_interface> &session, const std::shared_ptr<event_store> &events);

  void list_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
  void clear_events(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response);
};
