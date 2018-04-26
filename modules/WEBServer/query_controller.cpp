#include "query_controller.hpp"
#include "helpers.hpp"

#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_protobuf.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <str/xtos.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>


query_controller::query_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
  : session(session)
  , core(core)
  , plugin_id(plugin_id)
  , RegexpController(version==1?"/api/v1/queries": "/api/v2/queries")
{
	addRoute("GET", "/?$", this, &query_controller::get_queries);
	addRoute("GET", "/([^/]+)/?$", this, &query_controller::get_query);
	addRoute("GET", "/([^/]+)/commands/([^/]*)/?$", this, &query_controller::query_command);
}

void query_controller::get_queries(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_loggedin("queries.list", request, response))
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
		  if (i.info().plugin_size() > 0) {
			  node["plugin"] = i.info().plugin(0);
		  }
		  node["query_url"] = get_base(request) + "/" + i.name() + "/";
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

void query_controller::get_query(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("queries.get", request, response))
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
			if (i.info().plugin_size() > 0) {
				node["plugin"] = i.info().plugin(0);
			}
			node["title"] = i.info().title();
			node["execute_url"] = get_base(request) + "/" + i.name() + "/commands/execute";
			node["execute_nagios_url"] = get_base(request) + "/" + i.name() + "/commands/execute_nagios";
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

void query_controller::query_command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("queries.execute", request, response))
		return;

	if (what.size() != 3) {
		response.setCode(HTTP_NOT_FOUND);
		response.append("Invalid request");
	}
	std::string module = what.str(1);
	std::string command = what.str(2);

	if (command == "execute") {
		if (request.readHeader("Accept") == "text/plain") {
			execute_query_text(module, request.getVariablesVector(), response);
		} else {
			execute_query(module, request.getVariablesVector(), response);
		}
	} else if (command == "execute_nagios") {
		if (request.readHeader("Accept") == "text/plain") {
			execute_query_text(module, request.getVariablesVector(), response);
		} else {
			execute_query_nagios(module, request.getVariablesVector(), response);
		}
	} else {
		response.setCode(HTTP_NOT_FOUND);
		response.append("unknown command: " + command);
	}
}



void query_controller::execute_query(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
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


			json_spirit::Object perf;
			BOOST_FOREACH(const Plugin::Common::PerformanceData &p, l.perf()) {
				json_spirit::Object pdata;

				if (p.has_int_value()) {
					pdata["value"] = p.int_value().value();
					pdata["minimum"] = p.int_value().minimum();
					pdata["maximum"] = p.int_value().maximum();
					pdata["warning"] = p.int_value().warning();
					pdata["critical"] = p.int_value().critical();
					pdata["unit"] = p.int_value().unit();
				}
				if (p.has_float_value()) {
					pdata["value"] = p.float_value().value();
					pdata["minimum"] = p.float_value().minimum();
					pdata["maximum"] = p.float_value().maximum();
					pdata["warning"] = p.float_value().warning();
					pdata["critical"] = p.float_value().critical();
					pdata["unit"] = p.float_value().unit();
				}
				if (p.has_bool_value()) {
					pdata["value"] = p.bool_value().value();
				}
				if (p.has_string_value()) {
					pdata["value"] = p.string_value().value();
				}
				perf[p.alias()] = pdata;
			}
			line["perf"] = perf;
			lines.push_back(line);
		}
		node["lines"] = lines;
		break;
	}
	http_response.setCode(HTTP_OK);
	http_response.append(json_spirit::write(node));
}

void query_controller::execute_query_nagios(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
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
		node["result"] = nscapi::plugin_helper::translateReturn(r.result());
		json_spirit::Array lines;
		BOOST_FOREACH(const Plugin::QueryResponseMessage::Response::Line &l, r.lines()) {
			json_spirit::Object line;
			line["message"] = l.message();
			line["perf"] = nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation);
			lines.push_back(line);
		}
		node["lines"] = lines;
		break;
	}
	http_response.setCode(HTTP_OK);
	http_response.append(json_spirit::write(node));
}


void query_controller::execute_query_text(std::string module, arg_vector args, Mongoose::StreamResponse &http_response) {
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

	int code = 200;
	BOOST_FOREACH(const Plugin::QueryResponseMessage::Response &r, response.payload()) {
		if (r.result() == Plugin::Common_ResultCode_CRITICAL) {
			code = HTTP_SERVER_ERROR;
		} else if (r.result() == Plugin::Common_ResultCode_UNKNOWN) {
			code = 503;
		} else if (r.result() == Plugin::Common_ResultCode_WARNING) {
			code = 202;
		}
		BOOST_FOREACH(const Plugin::QueryResponseMessage::Response::Line &l, r.lines()) {
			http_response.append(l.message());
			if (l.perf_size() > 0) {
				http_response.append("|" + nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation));
			}
			http_response.append("\n");
		}
	}
	http_response.setCode(code);
}
