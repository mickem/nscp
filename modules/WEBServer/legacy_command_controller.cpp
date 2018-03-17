#include "legacy_command_controller.hpp"

#include <nscapi/nscapi_protobuf.hpp>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

legacy_command_controller::legacy_command_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core)
	: session(session)
	, core(core) 
{}

void legacy_command_controller::handle_query(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response))
    return;

  Plugin::QueryRequestMessage rm;
  Plugin::QueryRequestMessage::Request *payload = rm.add_payload();

  payload->set_command(obj);
  Mongoose::Request::arg_vector args = request.getVariablesVector();

  BOOST_FOREACH(const Mongoose::Request::arg_vector::value_type &e, args) {
	  if (e.second.empty())
		  payload->add_arguments(e.first);
	  else
		  payload->add_arguments(e.first + "=" + e.second);
  }

  std::string pb_response, json_response;
  core->query(rm.SerializeAsString(), pb_response);
  core->protobuf_to_json("QueryResponseMessage", pb_response, json_response);
  response.append(json_response);
}

void legacy_command_controller::handle_exec(std::string obj, Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("legacy", request, response))
    return;
  std::size_t pos = obj.find("/");
  if (pos == std::string::npos)
    return;
  std::string target = obj.substr(0, pos);
  std::string cmd = obj.substr(pos + 1);
  Plugin::ExecuteRequestMessage rm;
  Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

  payload->set_command(cmd);
  Mongoose::Request::arg_vector args = request.getVariablesVector();

  BOOST_FOREACH(const Mongoose::Request::arg_entry &e, args) {
    if (e.second.empty())
      payload->add_arguments(e.first);
    else
      payload->add_arguments(e.first + "=" + e.second);
  }

  std::string pb_response, json_response;
  core->exec_command(target, rm.SerializeAsString(), pb_response);
  core->protobuf_to_json("ExecuteResponseMessage", pb_response, json_response);
  response.append(json_response);
}

Mongoose::Response* legacy_command_controller::handleRequest(Mongoose::Request &request) {
  Mongoose::StreamResponse *response = new Mongoose::StreamResponse();
  std::string url = request.getUrl();
  if (boost::algorithm::starts_with(url, "/query/")) {
    handle_query(url.substr(7), request, *response);
  } else if (boost::algorithm::starts_with(url, "/exec/")) {
    handle_exec(url.substr(6), request, *response);
  } else {
    response->setCode(HTTP_SERVER_ERROR);
    response->append("Unknown REST node: " + url);
  }
  return response;
}
bool legacy_command_controller::handles(string method, string url) {
  return boost::algorithm::starts_with(url, "/query/") || boost::algorithm::starts_with(url, "/exec/");
}
