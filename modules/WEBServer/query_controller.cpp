#include "query_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <str/xtos.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>


query_controller::query_controller(boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController("/api/v1/queries")
{
	addRoute("GET", "/?$", this, &query_controller::get_modules);
	addRoute("GET", "/([^/]+)/?$", this, &query_controller::get_module);
	addRoute("PUT", "/([^/]*)/?$", this, &query_controller::post_module);
	addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &query_controller::module_command);
}

void query_controller::get_modules(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin(request, response))
    return;

  std::string fetch_all = request.get("all", "true");
  Plugin::RegistryRequestMessage rrm;
  Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_fetch_all(fetch_all == "true");
  payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  Plugin::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json_spirit::Array root;

  BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
	  BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
		  json_spirit::Object node;
		  node["name"] = i.name();
		  node["title"] = i.info().title();
		  json_spirit::Object keys;
		  BOOST_FOREACH(const ::Plugin::Common::KeyValue &kvp, i.info().metadata()) {
				keys[kvp.key()] = kvp.value();
		  }
		  node["metadata"] = keys;
		  node["description"] = i.info().description();
		  root.push_back(node);
	  }
  }
  response.append(json_spirit::write(root));
}

void query_controller::get_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin(request, response))
		return;

	if (what.size() != 2) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Query not found");
	}
	std::string module = what.str(1);

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	payload->mutable_inventory()->set_name(module);
	payload->mutable_inventory()->set_fetch_all(false);
	payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_QUERY);
	std::string str_response;
	core->registry_query(rrm.SerializeAsString(), str_response);

	Plugin::RegistryResponseMessage pb_response;
	pb_response.ParseFromString(str_response);
	json_spirit::Object node;

	BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
		BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
			node["name"] = i.name();
			node["title"] = i.info().title();
			json_spirit::Object keys;
			BOOST_FOREACH(const ::Plugin::Common::KeyValue &kvp, i.info().metadata()) {
				keys[kvp.key()] = kvp.value();
			}
			node["metadata"] = keys;
			node["description"] = i.info().description();
		}
	}
	response.setCode(HTTP_OK);
	response.append(json_spirit::write(node));
}

void query_controller::module_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (what.size() != 3) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Invalid request");
	}
	std::string module = what.str(1);
	std::string command = what.str(2);

	if (command == "execute") {

		
		load_module(module, request.getVariablesVector(), response);
	} else {
		response.setCode(HTTP_NOT_FOUND);
		response.append("unknown command: " + command);
	}
}



void query_controller::load_module(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
	Plugin::QueryRequestMessage qrm;
	Plugin::QueryRequestMessage::Request *payload = qrm.add_payload();

	payload->set_command(module);
	BOOST_FOREACH(const Mongoose::Request::arg_vector::value_type &e, args) {
		if (e.second.empty())
			payload->add_arguments(e.first);
		else
			payload->add_arguments(e.first + "=" + e.second);
	}
	std::string pb_response, json_response;
	core->query(qrm.SerializeAsString(), pb_response);
	Plugin::QueryResponseMessage response;
	response.ParseFromString(pb_response);

	json_spirit::Object node;
	BOOST_FOREACH(const Plugin::QueryResponseMessage::Response &r, response.payload()) {
		node["command"] = r.command();
		node["result"] = r.result();
		json_spirit::Array lines;
		BOOST_FOREACH(const Plugin::QueryResponseMessage::Response::Line &l, r.lines()) {
			json_spirit::Object line;
			line["message"] = l.message();
			line["perf"] = nscapi::protobuf::functions::build_performance_data(l, -1);
			lines.push_back(line);
		}
		node["lines"] = lines;
		break;
	}
	http_response.setCode(HTTP_OK);
	http_response.append(json_spirit::write(node));
}


void query_controller::post_module(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin(request, response))
		return;

		response.setCode(HTTP_SERVER_ERROR);
		response.append("Invalid request ");
}
