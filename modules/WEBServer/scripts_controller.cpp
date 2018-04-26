#include "scripts_controller.hpp"

#include <nscapi/nscapi_protobuf.hpp>

#include <file_helpers.hpp>

#include <str/xtos.hpp>

#include <json_spirit.h>

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem/path.hpp>

#include <fstream>
#include <iostream>

#define EXT_SCR "CheckExternalScripts"
#define PY_SCR "PythonScript"

std::string get_runtime(const std::string &runtime) {
	if (runtime == "ext") {
		return EXT_SCR;
	}
	if (runtime == "py") {
		return PY_SCR;
	}
	return runtime;
}

bool validate_response(const Plugin::ExecuteResponseMessage &resp, Mongoose::StreamResponse &response) {
	if (resp.payload_size() == 0) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("No response from module, is the module loaded?");
		return false;
	}
	if (resp.payload_size() != 1) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("Invalid response from module");
		return false;
	}
	if (resp.payload(0).result() != ::Plugin::Common_ResultCode_OK) {
		response.setCode(HTTP_SERVER_ERROR);
		response.append("Command returned errors: " + resp.payload(0).message());
		return false;
	}

	return true;
}

scripts_controller::scripts_controller(const int version, boost::shared_ptr<session_manager_interface> session, nscapi::core_wrapper* core, unsigned int plugin_id)
	: RegexpController(version==1?"/api/v1/scripts":"/api/v2/scripts")
	, session(session)
	, core(core)
	, plugin_id(plugin_id) {
	addRoute("GET", "/?$", this, &scripts_controller::get_runtimes);
	addRoute("GET", "/([^/]+)/?$", this, &scripts_controller::get_scripts);
	addRoute("GET", "/([^/]+)/(.+)/?$", this, &scripts_controller::get_script);
	addRoute("PUT", "/([^/]*)/(.+)/?$", this, &scripts_controller::add_script);
	addRoute("DELETE", "/([^/]*)/(.+)/?$", this, &scripts_controller::delete_script);
}

void scripts_controller::get_runtimes(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("scripts.list.runtimes", request, response))
		return;

	Plugin::RegistryRequestMessage rrm;
	Plugin::RegistryRequestMessage::Request *payload = rrm.add_payload();
	payload->mutable_inventory()->set_fetch_all(false);
	payload->mutable_inventory()->add_type(Plugin::Registry_ItemType_MODULE);
	std::string str_response;
	core->registry_query(rrm.SerializeAsString(), str_response);

	Plugin::RegistryResponseMessage pb_response;
	pb_response.ParseFromString(str_response);
	json_spirit::Array root;

	BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response r, pb_response.payload()) {
		BOOST_FOREACH(const Plugin::RegistryResponseMessage::Response::Inventory i, r.inventory()) {
			if (i.name() == PY_SCR
				|| i.name() == EXT_SCR
				|| i.name() == "LUAScript") {
				json_spirit::Object node;
				std::string name = i.name();
				if (i.name() == PY_SCR) {
					name = "py";
				} else if (i.name() == EXT_SCR) {
					name = "ext";
				} else if (i.name() == "LUAScript") {
					name = "lua";
				} else {
					name = i.name();
				}
				node["name"] = name;
				node["module"] = i.name();
				node["title"] = i.info().title();
				node["runtime_url"] = get_base(request) + "/" + name;
				root.push_back(node);
			}
		}
	}
	response.append(json_spirit::write(root));
}

void scripts_controller::get_scripts(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("scripts", request, response))
		return;

	if (!validate_arguments(1, what, response)) {
		return;
	}
	std::string runtime = get_runtime(what.str(1));

	std::string fetch_all = request.get("all", "false");

	if (!session->can("scripts.lists." + runtime, request, response))
		return;

	Plugin::ExecuteRequestMessage rm;
	Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

	payload->set_command("list");
	payload->add_arguments("--json");
	if (fetch_all != "true") {
		payload->add_arguments("--query");
	}

	std::string pb_response;
	core->exec_command(runtime, rm.SerializeAsString(), pb_response);
	Plugin::ExecuteResponseMessage resp;
	resp.ParseFromString(pb_response);
	if (!validate_response(resp, response)) {
		return;
	}

	response.append(resp.payload(0).message());
}



void scripts_controller::get_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("scripts", request, response))
		return;

	if (!validate_arguments(2, what, response)) {
		return;
	}
	std::string runtime = get_runtime(what.str(1));
	std::string script = what.str(2);

	if (!session->can("scripts.get." + runtime, request, response))
		return;

	Plugin::ExecuteRequestMessage rm;
	Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

	payload->set_command("show");
	payload->add_arguments("--script");
	payload->add_arguments(script);

	std::string pb_response;
	core->exec_command(runtime, rm.SerializeAsString(), pb_response);
	Plugin::ExecuteResponseMessage resp;
	resp.ParseFromString(pb_response);
	if (!validate_response(resp, response)) {
		return;
	}

	response.append(resp.payload(0).message());
}



void scripts_controller::add_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("scripts", request, response))
		return;

	if (!validate_arguments(2, what, response)) {
		return;
	}
	std::string runtime = get_runtime(what.str(1));
	std::string script = what.str(2);

	if (!session->can("scripts.add." + runtime, request, response))
		return;

	boost::filesystem::path name = script;
	boost::filesystem::path file = core->expand_path("${temp}/" + file_helpers::meta::get_filename(name));
	std::ofstream ofs(file.string().c_str(), std::ios::binary);
	ofs << request.getData();
	ofs.close();

	Plugin::ExecuteRequestMessage rm;
	Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

	payload->set_command("add");
	payload->add_arguments("--script");
#ifdef WIN32
	boost::algorithm::replace_all(script, "/", "\\");
#endif
	payload->add_arguments(script);
	payload->add_arguments("--import");
	payload->add_arguments(file.string());
	payload->add_arguments("--replace");

	std::string pb_response;
	core->exec_command(runtime, rm.SerializeAsString(), pb_response);
	Plugin::ExecuteResponseMessage resp;
	resp.ParseFromString(pb_response);
	if (!validate_response(resp, response)) {
		return;
	}

	response.append(resp.payload(0).message());
}


void scripts_controller::delete_script(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
	if (!session->is_loggedin("scripts", request, response))
		return;

	if (!validate_arguments(2, what, response)) {
		return;
	}
	std::string runtime = get_runtime(what.str(1));
	std::string script = what.str(2);

	if (!session->can("scripts.delete." + runtime, request, response))
		return;

	Plugin::ExecuteRequestMessage rm;
	Plugin::ExecuteRequestMessage::Request *payload = rm.add_payload();

	payload->set_command("delete");
	payload->add_arguments("--script");
#ifdef WIN32
	boost::algorithm::replace_all(script, "/", "\\");
#endif
	payload->add_arguments(script);

	std::string pb_response;
	core->exec_command(runtime, rm.SerializeAsString(), pb_response);
	Plugin::ExecuteResponseMessage resp;
	resp.ParseFromString(pb_response);
	if (!validate_response(resp, response)) {
		return;
	}

	response.append(resp.payload(0).message());
}


