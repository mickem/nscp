#include "metadata_controller.hpp"

#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <utility>

#include "str/constant_time.hpp"
#include "str/xtos.hpp"

namespace json = boost::json;

namespace {
bool exec_metadata_command(const nscapi::core_wrapper *core, const std::string &target, const std::string &command,
                           const std::vector<std::string> &arguments, Mongoose::StreamResponse &response, std::string &out_message) {
  PB::Commands::ExecuteRequestMessage rm;
  PB::Commands::ExecuteRequestMessage::Request *payload = rm.add_payload();
  payload->set_command(command);
  for (const std::string &arg : arguments) {
    payload->add_arguments(arg);
  }

  std::string pb_response;
  core->exec_command(target, rm.SerializeAsString(), pb_response);
  PB::Commands::ExecuteResponseMessage resp;
  resp.ParseFromString(pb_response);
  if (resp.payload_size() == 0) {
    response.setCodeServerError("No response from module, is the " + target + " module loaded?");
    return false;
  }
  if (resp.payload_size() != 1) {
    response.setCodeServerError("Invalid response from module");
    return false;
  }
  if (resp.payload(0).result() != PB::Common::ResultCode::OK) {
    response.setCodeServerError("Command returned errors: " + resp.payload(0).message());
    return false;
  }
  out_message = resp.payload(0).message();
  return true;
}
}  // namespace

metadata_controller::metadata_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                         unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/metadata" : "/api/v2/metadata"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &metadata_controller::get_index);
  addRoute("GET", "/counters/?$", this, &metadata_controller::get_counters);
  addRoute("GET", "/channels/?$", this, &metadata_controller::get_channels);
}

void metadata_controller::get_index(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.list", request, response)) return;

  json::array root;
  json::object counters;
  counters["name"] = "counters";
  counters["title"] = "Performance counters";
  counters["url"] = get_base(request) + "/counters";
  root.push_back(counters);
  json::object channels;
  channels["name"] = "channels";
  channels["title"] = "Registered submission channels";
  channels["url"] = get_base(request) + "/channels";
  root.push_back(channels);
  response.append(json::serialize(root));
}

void metadata_controller::get_counters(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.get.counters", request, response)) return;

  std::string message;
  if (!exec_metadata_command(core, "CheckSystem", "pdh", {"--list", "--json", "--all", "--no-instances"}, response, message)) {
    return;
  }
  response.get_headers()["XX-debug"] = "size: " + str::xtos(message.size());

  response.get_headers()["Content-Type"] = "application/json";
  response.append(message);
}

void metadata_controller::get_channels(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("metadata.get.channels", request, response)) return;

  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::HANDLER);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  json::array root;
  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
      json::object node;
      node["name"] = i.name();
      json::array plugins;
      for (const std::string &plugin : i.info().plugin()) {
        plugins.push_back(json::value(plugin));
      }
      node["plugins"] = plugins;
      root.push_back(node);
    }
  }
  response.get_headers()["Content-Type"] = "application/json";
  response.append(json::serialize(root));
}
