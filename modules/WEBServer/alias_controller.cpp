#include "alias_controller.hpp"

#include <boost/json.hpp>
#include <nscapi/protobuf/registry.hpp>

namespace json = boost::json;

alias_controller::alias_controller(const int version, const std::shared_ptr<session_manager_interface> &session, const nscapi::core_wrapper *core,
                                   const unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/aliases" : "/api/v2/aliases"), session(session), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &alias_controller::get_aliases);
}

// Same registry-inventory dance as query_controller::get_queries but asks
// the registry for QUERY_ALIAS items. Each alias is reported with a
// `query_url` pointing at /api/vX/queries/<name>/ because executing an
// alias is identical to executing a query - the core resolves the alias to
// its target on its own.
void alias_controller::get_aliases(Mongoose::Request &request, boost::smatch & /*what*/, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("aliases.list", request, response)) return;

  const std::string fetch_all = request.get("all", "true");
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  payload->mutable_inventory()->set_fetch_all(fetch_all == "true");
  payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY_ALIAS);
  std::string str_response;
  core->registry_query(rrm.SerializeAsString(), str_response);

  PB::Registry::RegistryResponseMessage pb_response;
  pb_response.ParseFromString(str_response);
  json::array root;

  // The aliases endpoint sits on the same /api/vX prefix as queries so we
  // can derive the queries base URL by swapping the last path segment.
  std::string queries_base = get_base(request);
  const auto pos = queries_base.find_last_of('/');
  if (pos != std::string::npos) {
    queries_base.replace(pos + 1, std::string::npos, "queries");
  }

  for (const PB::Registry::RegistryResponseMessage::Response &r : pb_response.payload()) {
    for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
      json::object node;
      node["name"] = i.name();
      if (i.info().plugin_size() > 0) {
        node["plugin"] = i.info().plugin(0);
      }
      // Aliases are executed via the queries endpoint, so the URL we return
      // is intentionally a queries URL rather than an aliases URL - clients
      // should follow it for execution, not try to construct their own.
      node["query_url"] = queries_base + "/" + i.name() + "/";
      node["title"] = i.info().title();
      json::object keys;
      for (const PB::Common::KeyValue &kvp : i.info().metadata()) {
        keys[kvp.key()] = kvp.value();
      }
      node["metadata"] = keys;
      node["description"] = i.info().description();
      root.push_back(node);
    }
  }
  response.append(json::serialize(root));
}
