// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_controller.hpp"

#include <boost/json.hpp>
#include <nscapi/protobuf/diagnostics.hpp>
#include <nscapi/protobuf/diagnostics_json.hpp>
#include <str/xtos.hpp>
#include <string>
#include <utility>

namespace json = boost::json;

namespace {
// A core that predates the call answers with nothing; that reads as
// diagnostics being off, which is what it amounts to for the caller.
bool parse_envelope(const std::string &envelope, PB::Diagnostics::DiagnosticsResponseMessage &message) {
  return !envelope.empty() && message.ParseFromString(envelope) && message.payload_size() > 0;
}

void describe(json::object &out, const PB::Diagnostics::DiagnosticsResponseMessage::Response &payload) {
  out["enabled"] = payload.enabled();
  out["snapshot_id"] = static_cast<std::int64_t>(payload.snapshot_id());
  out["captured_at"] = diagnostics_json::iso_time(payload.captured_at());
  out["max_rows"] = static_cast<std::int64_t>(payload.max_rows());
  out["dropped"] = static_cast<std::int64_t>(payload.dropped());
  json::array excluded;
  for (const std::string &name : payload.excluded()) excluded.emplace_back(name);
  out["excluded"] = excluded;
}

const char *const disabled_message = "Diagnostics are disabled: set capture diagnostics = true in [/settings/core/failed checks]";
}  // namespace

diagnostics_controller::diagnostics_controller(const int version, std::shared_ptr<session_manager_interface> session, const nscapi::core_wrapper *core,
                                               const unsigned int plugin_id)
    : RegexpController("/api/v" + std::to_string(version) + "/diagnostics"), session(std::move(session)), core(core), plugin_id(plugin_id) {
  addRoute("GET", "/?$", this, &diagnostics_controller::list_sections);
  addRoute("GET", "/([A-Za-z0-9_.\\-]+)/?$", this, &diagnostics_controller::get_section);
}

void diagnostics_controller::list_sections(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("diagnostics.list", request, response)) return;

  PB::Diagnostics::DiagnosticsResponseMessage message;
  json::object out;
  json::array sections;
  if (parse_envelope(core->get_diagnostics(), message)) {
    const PB::Diagnostics::DiagnosticsResponseMessage::Response &payload = message.payload(0);
    describe(out, payload);
    for (const PB::Diagnostics::Section &s : payload.snapshot().sections()) {
      json::object node = diagnostics_json::summary(s);
      node["name"] = s.name();
      node["rows"] = static_cast<std::int64_t>(s.rows_size());
      sections.push_back(node);
    }
  } else {
    PB::Diagnostics::DiagnosticsResponseMessage::Response off;
    describe(out, off);
  }
  out["count"] = static_cast<std::int64_t>(sections.size());
  out["sections"] = sections;
  response.setHeader("X-Result-Count", str::xtos(sections.size()));
  response.append(json::serialize(out));
}

void diagnostics_controller::get_section(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!session->is_logged_in("diagnostics.list", request, response)) return;
  const std::string name = what.str(1);

  PB::Diagnostics::DiagnosticsResponseMessage message;
  if (!parse_envelope(core->get_diagnostics(name), message) || !message.payload(0).enabled()) {
    response.setCodeNotFound(disabled_message);
    return;
  }
  const PB::Diagnostics::DiagnosticsResponseMessage::Response &payload = message.payload(0);
  if (payload.snapshot().sections_size() == 0) {
    response.setCodeNotFound(payload.snapshot_id() == 0 ? "No diagnostics collected yet: " + name : "Diagnostics section not found: " + name);
    return;
  }
  json::object out = diagnostics_json::section(payload.snapshot().sections(0));
  out["name"] = name;
  out["snapshot_id"] = static_cast<std::int64_t>(payload.snapshot_id());
  response.append(json::serialize(out));
}
