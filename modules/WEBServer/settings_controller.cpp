#include "settings_controller.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <boost/regex.hpp>
#include <nscapi/protobuf/settings.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace json = boost::json;

settings_controller::settings_controller(const int version, boost::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                         unsigned int plugin_id)
    : RegexpController(version == 1 ? "/api/v1/settings" : "/api/v2/settings"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/descriptions(.*)$", this, &settings_controller::get_desc);
  addRoute("POST", "/command$", this, &settings_controller::command);
  addRoute("GET", "/status$", this, &settings_controller::status);
  addRoute("GET", "/diff$", this, &settings_controller::diff);
  addRoute("GET", "(.*)$", this, &settings_controller::get);
  addRoute("PUT", "(.*)$", this, &settings_controller::put);
  addRoute("DELETE", "(.*)$", this, &settings_controller::del);
}

void settings_controller::get(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.get", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }

  std::string path = what.str(1);

  PB::Settings::SettingsRequestMessage rm;
  PB::Settings::SettingsRequestMessage::Request *payload = rm.add_payload();
  payload->mutable_query()->mutable_node()->set_path(path);
  payload->mutable_query()->set_recursive(true);
  payload->mutable_query()->set_include_keys(true);
  payload->set_plugin_id(plugin_id);

  std::string str_response;
  core->settings_query(rm.SerializeAsString(), str_response);
  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  if (pb_response.payload_size() != 1) {
    response.setCodeServerError("Failed to fetch keys");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response rKeys = pb_response.payload(0);
  if (!rKeys.has_query()) {
    response.setCodeServerError("Key not found: " + path);
    return;
  }

  json::array node;
  for (const PB::Settings::Node &s : rKeys.query().nodes()) {
    json::object rs;
    rs["path"] = s.path();
    rs["key"] = s.key();
    rs["value"] = s.value();
    node.push_back(rs);
  }

  response.append(json::serialize(node));
}

void settings_controller::get_desc(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.get", request, response)) return;

  if (!validate_arguments(1, what, response)) {
    return;
  }

  std::string path = what.str(1);

  PB::Settings::SettingsRequestMessage rm_fetch_paths;
  PB::Settings::SettingsRequestMessage::Request *payload_1 = rm_fetch_paths.add_payload();
  payload_1->mutable_inventory()->mutable_node()->set_path(path);
  payload_1->mutable_inventory()->set_recursive_fetch(request.get_bool("recursive", true));
  payload_1->mutable_inventory()->set_fetch_paths(true);
  payload_1->mutable_inventory()->set_fetch_keys(true);
  payload_1->mutable_inventory()->set_fetch_samples(request.get_bool("samples", false));
  payload_1->set_plugin_id(plugin_id);

  std::string str_response_1;
  core->settings_query(rm_fetch_paths.SerializeAsString(), str_response_1);
  PB::Settings::SettingsResponseMessage pb_response_1;
  pb_response_1.ParseFromString(str_response_1);

  if (pb_response_1.payload_size() != 1) {
    response.setCodeServerError("Failed to fetch keys");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response rKeys_1 = pb_response_1.payload(0);
  if (rKeys_1.inventory_size() == 0) {
    response.setCodeNotFound("Key not found: " + path);
    return;
  }
  typedef std::map<std::string, std::string> values_type;
  values_type values;

  if (true) {
    PB::Settings::SettingsRequestMessage rm_fetch_keys;
    PB::Settings::SettingsRequestMessage::Request *payload_2 = rm_fetch_keys.add_payload();
    payload_2->mutable_query()->mutable_node()->set_path(path);
    payload_2->mutable_query()->set_recursive(true);
    payload_2->mutable_query()->set_include_keys(true);
    payload_2->set_plugin_id(plugin_id);

    std::string str_response_2;
    core->settings_query(rm_fetch_keys.SerializeAsString(), str_response_2);
    PB::Settings::SettingsResponseMessage pb_response_2;
    pb_response_2.ParseFromString(str_response_2);

    if (pb_response_2.payload_size() != 1) {
      response.setCodeServerError("Failed to fetch keys");
      return;
    }

    const PB::Settings::SettingsResponseMessage::Response rKeys_2 = pb_response_2.payload(0);
    if (!rKeys_2.has_query()) {
      response.setCodeNotFound("Key not found: " + path);
      return;
    }

    for (const PB::Settings::Node &s : rKeys_2.query().nodes()) {
      if (!s.value().empty()) {
        values[s.path() + "$$$" + s.key()] = s.value();
      }
    }
  }

  json::array node;
  for (const PB::Settings::SettingsResponseMessage::Response::Inventory &s : rKeys_1.inventory()) {
    json::object rs;
    rs["path"] = s.node().path();
    rs["key"] = s.node().key();
    if (values.size() > 0) {
      values_type::const_iterator cit = values.find(s.node().path() + "$$$" + s.node().key());
      if (cit != values.end()) {
        rs["value"] = cit->second;
      } else {
        rs["value"] = s.info().default_value();
      }
    }
    rs["type"] = s.info().type();
    rs["title"] = s.info().title();
    rs["icon"] = s.info().icon();
    rs["description"] = s.info().description();
    rs["is_advanced_key"] = s.info().advanced();
    rs["is_sample_key"] = s.info().sample();
    rs["is_template_key"] = s.info().is_template();
    rs["is_object"] = s.info().subkey();
    rs["sample_usage"] = s.info().sample_usage();
    rs["default_value"] = s.info().default_value();

    json::array plugins;
    for (const ::std::string &p : s.info().plugin()) {
      plugins.push_back(json::value(p));
    }
    rs["plugins"] = plugins;

    node.push_back(rs);
  }

  response.append(json::serialize(node));
}

namespace {
// `mode` controls what an "empty value" means when the request leaves it out:
//   - update: leaving value empty maps to "remove this key" (the settings core
//             treats an empty value as the key not being set). Callers that
//             want to write an empty string explicitly can do so.
//   - remove: value is ignored - we always send empty so the core deletes.
enum class write_mode { update, remove };

// Translate one JSON object into one Update payload on `srm`. Returns true
// when the entry is well-formed; on failure the caller has already written
// an error to `response`.
bool append_settings_entry(const json::object &o, const std::string &default_path, write_mode mode, PB::Settings::SettingsRequestMessage &srm,
                           Mongoose::StreamResponse &response) {
  std::string current_path;
  if (auto *p = o.if_contains("path")) {
    if (auto *s = p->if_string()) current_path.assign(s->c_str(), s->size());
  }
  if (current_path.empty()) current_path = default_path;

  std::string key;
  if (auto *k = o.if_contains("key")) {
    if (auto *s = k->if_string()) key.assign(s->c_str(), s->size());
  }
  // `key` is optional for DELETE (empty key means "remove the whole path"),
  // and required for any update because the update payload otherwise
  // implicitly removes a path the caller did not name.
  if (mode == write_mode::update && key.empty()) {
    response.setCodeBadRequest("Key is required");
    return false;
  }
  // Same safety net as the query-string DELETE path: reject "delete with no
  // target" so a stray empty entry in a batch does not silently fail (or,
  // worse, ask the core to drop the root).
  if (mode == write_mode::remove && current_path.empty() && key.empty()) {
    response.setCodeBadRequest("DELETE entry requires either a path or a key");
    return false;
  }

  std::string value;
  if (mode == write_mode::update) {
    if (auto *v = o.if_contains("value")) {
      if (auto *s = v->if_string()) value.assign(s->c_str(), s->size());
    }
  }

  PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
  PB::Settings::Node *node = payload->mutable_update()->mutable_node();
  node->set_path(current_path);
  node->set_key(key);
  node->set_value(value);
  return true;
}

// Drives the JSON body parsing for both PUT and DELETE - the only difference
// between the two is how the absence of a value/key field is interpreted.
bool process_settings_payload(const json::value &root, const std::string &default_path, write_mode mode, int &count_out,
                              PB::Settings::SettingsRequestMessage &srm, Mongoose::StreamResponse &response) {
  count_out = 0;
  if (root.is_array()) {
    for (const auto &v : root.as_array()) {
      if (!v.is_object()) {
        response.setCodeBadRequest("Expected an object in the array");
        return false;
      }
      if (!append_settings_entry(v.as_object(), default_path, mode, srm, response)) return false;
      ++count_out;
    }
  } else if (root.is_object()) {
    if (!append_settings_entry(root.as_object(), default_path, mode, srm, response)) return false;
    ++count_out;
  } else {
    response.setCodeBadRequest("Expected an object or an array of objects");
    return false;
  }
  return true;
}
}  // namespace

void settings_controller::put(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.put", request, response)) return;
  if (!validate_arguments(1, what, response)) {
    return;
  }
  std::string path = what.str(1);

  try {
    auto root = json::parse(request.getData());
    PB::Settings::SettingsRequestMessage srm;

    int keys = 0;
    if (!process_settings_payload(root, path, write_mode::update, keys, srm, response)) {
      return;
    }

    std::string str_response;
    core->settings_query(srm.SerializeAsString(), str_response);

    PB::Settings::SettingsResponseMessage pb_response;
    pb_response.ParseFromString(str_response);

    json::object node;
    // TODO: Parse status here
    node["status"] = "success";
    node["keys"] = keys;
    response.append(json::serialize(node));

  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
    response.setCodeBadRequest("Problems parsing JSON");
  }
}

namespace {
// Result of expanding a single user-supplied "remove this path" entry into
// the concrete keys/paths that have to be deleted. The settings core's
// remove_path is non-recursive (it only drops the section that exactly
// matches the path) which is why a plain `DELETE /api/v2/settings/foo`
// otherwise leaves keys under `foo/bar` orphaned.
struct delete_target {
  std::string path;
  std::string key;  // empty means "remove the path itself"
};

// Issue a single recursive query against the settings core. `include_keys`
// flips between "list every key in the subtree" and "list every sub-path".
bool query_subtree(const nscapi::core_wrapper *core, unsigned int plugin_id, const std::string &path, bool include_keys, std::vector<PB::Settings::Node> &out) {
  PB::Settings::SettingsRequestMessage rm;
  auto *payload = rm.add_payload();
  payload->mutable_query()->mutable_node()->set_path(path);
  payload->mutable_query()->set_recursive(true);
  payload->mutable_query()->set_include_keys(include_keys);
  payload->set_plugin_id(plugin_id);

  std::string raw;
  if (!core->settings_query(rm.SerializeAsString(), raw)) return false;
  PB::Settings::SettingsResponseMessage pb;
  if (!pb.ParseFromString(raw)) return false;
  if (pb.payload_size() != 1 || !pb.payload(0).has_query()) return false;
  for (const auto &n : pb.payload(0).query().nodes()) out.push_back(n);
  return true;
}

// Expand a path-level delete (key is empty) into the full set of leaf-level
// removals the core can actually carry out. Two queries are required because
// recurse_find emits keys *or* paths but never both:
//   - include_keys=true  -> every (path, key) tuple in the subtree
//   - include_keys=false -> every sub-path under the base
// Sub-paths come back deepest-first so that when the core later flushes the
// delete cache the parent does not get re-created by a child save.
void expand_recursive_path_delete(const nscapi::core_wrapper *core, unsigned int plugin_id, const std::string &path, std::vector<delete_target> &out) {
  std::vector<PB::Settings::Node> keys;
  query_subtree(core, plugin_id, path, /*include_keys=*/true, keys);
  for (const auto &n : keys) {
    if (!n.key().empty()) out.push_back({n.path(), n.key()});
  }

  std::vector<PB::Settings::Node> paths;
  query_subtree(core, plugin_id, path, /*include_keys=*/false, paths);
  // Deepest-first - more slashes => deeper. Stable sort keeps query order
  // for ties, which matters for tests that snapshot the request stream.
  std::stable_sort(paths.begin(), paths.end(),
                   [](const PB::Settings::Node &a, const PB::Settings::Node &b) { return std::count(a.path().begin(), a.path().end(), '/') > std::count(b.path().begin(), b.path().end(), '/'); });
  for (const auto &n : paths) {
    out.push_back({n.path(), ""});
  }
  out.push_back({path, ""});
}

void emit_target(const delete_target &t, PB::Settings::SettingsRequestMessage &srm) {
  PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
  PB::Settings::Node *node = payload->mutable_update()->mutable_node();
  node->set_path(t.path);
  node->set_key(t.key);
  // Value left empty - the core treats an empty value as "remove".
}
}  // namespace

// DELETE /api/vN/settings/<path>
//   Form A: query-string driven       -> ?key=<name>             (remove key)
//                                     -> (no key)                 (remove path)
//   Form B: single-object body        -> {"key":"…"} optionally with "path"
//   Form C: array body for batch      -> [{"path":"…","key":"…"}, …]
//
// All three drop down to the same parse_update path inside the core: an
// empty value plus a non-empty key removes the key; an empty key removes the
// entire path. We never let "delete without a key" happen by accident -
// callers must POST an array entry without a key, or omit `?key=` AND send
// no body at all, before we will remove a path.
//
// Path-level deletes are recursive by default: every key in every sub-path
// under the target is removed, then every sub-path itself, then the target.
// Pass `?recursive=false` to keep the legacy "drop only this section's
// direct keys" behaviour.
void settings_controller::del(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.delete", request, response)) return;
  if (!validate_arguments(1, what, response)) {
    return;
  }
  const std::string path = what.str(1);
  const std::string body = request.getData();
  const bool recursive = request.get_bool("recursive", true);

  // Step 1: collect the user's request as a flat list of targets. Body
  // shapes (single object / array) and the query-string shape both end up
  // here so step 2 can treat them uniformly.
  std::vector<delete_target> targets;
  if (body.empty()) {
    const std::string key = request.get("key", "");
    if (path.empty() && key.empty()) {
      response.setCodeBadRequest("DELETE requires a path; pass ?key= to remove a single key");
      return;
    }
    targets.push_back({path, key});
  } else {
    try {
      auto root = json::parse(body);
      // Reuse the PUT/array parsing by funnelling through a temporary
      // request so we get the same validation errors. The result message
      // only carries the (path, key) tuples - the empty value the helper
      // sets is exactly what we want for delete.
      PB::Settings::SettingsRequestMessage tmp;
      int count = 0;
      if (!process_settings_payload(root, path, write_mode::remove, count, tmp, response)) {
        return;
      }
      targets.reserve(static_cast<std::size_t>(count));
      for (const auto &p : tmp.payload()) {
        targets.push_back({p.update().node().path(), p.update().node().key()});
      }
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
      response.setCodeBadRequest("Problems parsing JSON");
      return;
    }
  }

  // Step 2: turn the user-facing targets into the concrete payload list
  // the core will execute. Path-level entries (empty key) get expanded into
  // every key + sub-path beneath them when recursion is on.
  PB::Settings::SettingsRequestMessage srm;
  std::vector<delete_target> expanded;
  for (const delete_target &t : targets) {
    if (t.key.empty() && recursive && !t.path.empty()) {
      expand_recursive_path_delete(core, plugin_id, t.path, expanded);
    } else {
      expanded.push_back(t);
    }
  }
  for (const delete_target &t : expanded) emit_target(t, srm);

  std::string str_response;
  core->settings_query(srm.SerializeAsString(), str_response);

  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  json::object node;
  node["status"] = "success";
  node["keys"] = static_cast<int>(expanded.size());
  node["recursive"] = recursive;
  response.append(json::serialize(node));
}

void settings_controller::command(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.put", request, response)) return;
  try {
    auto root = json::parse(request.getData());
    json::object o = root.as_object();
    auto command = o["command"].as_string();

    if (command == "reload") {
      if (!session->is_logged_in("settings.put", request, response)) return;
      if (!core->reload("delayed,service")) {
        response.setCodeServerError("500 Query failed");
        return;
      }
    } else {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *payload = srm.add_payload();
      if (command == "load") {
        payload->mutable_control()->set_command(PB::Settings::Command::LOAD);
      } else if (command == "save") {
        payload->mutable_control()->set_command(PB::Settings::Command::SAVE);
      } else {
        response.setCodeNotFound("Unknown command: " + static_cast<std::string>(command.data()));
        return;
      }
      std::string str_response;
      core->settings_query(srm.SerializeAsString(), str_response);

      PB::Settings::SettingsResponseMessage pb_response;
      pb_response.ParseFromString(str_response);
      // TODO: Parse status here
    }
    json::object node;
    node["status"] = "success";
    response.append(json::serialize(node));
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to parse JSON: " + std::string(e.what()));
    response.setCodeBadRequest("Problems parsing JSON");
  }
}

void settings_controller::status(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.get", request, response)) return;
  PB::Settings::SettingsRequestMessage rm;
  PB::Settings::SettingsRequestMessage::Request *payload = rm.add_payload();
  payload->mutable_status();
  payload->set_plugin_id(plugin_id);

  std::string str_response, json_response;
  core->settings_query(rm.SerializeAsString(), str_response);

  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  if (pb_response.payload_size() != 1) {
    response.setCodeNotFound("Failed to get status");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response &response_payload = pb_response.payload(0);
  if (!response_payload.has_status()) {
    response.setCodeNotFound("Failed to get status");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response::Status &status = response_payload.status();

  json::object node;
  node["context"] = status.context();
  node["type"] = status.type();
  node["has_changed"] = status.has_changed();
  response.append(json::serialize(node));
}

namespace {
const char *change_type_to_string(PB::Settings::SettingsResponseMessage::Response::Diff::ChangeType t) {
  switch (t) {
    case PB::Settings::SettingsResponseMessage::Response::Diff::ADDED:
      return "added";
    case PB::Settings::SettingsResponseMessage::Response::Diff::REMOVED:
      return "removed";
    case PB::Settings::SettingsResponseMessage::Response::Diff::PATH_ADDED:
      return "path_added";
    case PB::Settings::SettingsResponseMessage::Response::Diff::PATH_REMOVED:
      return "path_removed";
    case PB::Settings::SettingsResponseMessage::Response::Diff::MODIFIED:
    default:
      return "modified";
  }
}
}  // namespace

void settings_controller::diff(Mongoose::Request &request, boost::smatch & /*what*/, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("settings.get", request, response)) return;

  PB::Settings::SettingsRequestMessage rm;
  PB::Settings::SettingsRequestMessage::Request *payload = rm.add_payload();
  PB::Settings::SettingsRequestMessage::Request::Diff *diff = payload->mutable_diff();
  std::string path_filter = request.get("path", "");
  if (!path_filter.empty()) {
    diff->mutable_node()->set_path(path_filter);
  }
  diff->set_recursive(request.get_bool("recursive", true));
  payload->set_plugin_id(plugin_id);

  std::string str_response;
  if (!core->settings_query(rm.SerializeAsString(), str_response)) {
    response.setCodeServerError("Failed to fetch settings diff");
    return;
  }
  PB::Settings::SettingsResponseMessage pb_response;
  pb_response.ParseFromString(str_response);

  if (pb_response.payload_size() != 1) {
    response.setCodeServerError("Failed to fetch settings diff");
    return;
  }

  const PB::Settings::SettingsResponseMessage::Response &resp = pb_response.payload(0);
  if (!resp.has_diff()) {
    response.setCodeServerError("Diff missing from response");
    return;
  }

  json::array entries;
  for (const auto &e : resp.diff().entries()) {
    json::object o;
    o["path"] = e.path();
    o["key"] = e.key();
    o["old_value"] = e.old_value();
    o["new_value"] = e.new_value();
    o["change_type"] = change_type_to_string(e.change_type());
    o["is_sensitive"] = e.is_sensitive();
    entries.push_back(o);
  }
  json::object out;
  out["entries"] = entries;
  out["count"] = static_cast<int>(entries.size());
  response.append(json::serialize(out));
}
