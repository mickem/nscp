// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/gregorian/formatters.hpp>
#include <boost/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <str/format.hpp>
#include <string>
#include <vector>

namespace elastic_bulk {

// Expand %(date) in the configured index name to today's UTC date so daily
// indices (nsclient_log-2026-08-29) roll over automatically.
inline std::string parse_index(const std::string &index) {
  const std::string date = boost::gregorian::to_iso_extended_string(boost::gregorian::day_clock::universal_day());
  return boost::algorithm::replace_all_copy(index, "%(date)", date);
}

// Build an x-ndjson _bulk request body: one action line followed by one
// document line per payload. Every document gets its own random _id -
// documents indexed with the same _id overwrite each other, so a shared id
// silently drops all but one document of a batch. `_type` is only emitted
// when a type is configured: mapping types were removed in Elasticsearch 8,
// which rejects bulk actions that still carry the parameter.
inline std::string build_payload(const std::string &index, const std::string &type, const std::vector<std::string> &payloads) {
  std::string payload;
  boost::uuids::random_generator generate_uuid;
  for (const std::string &data : payloads) {
    boost::json::object tgtidx;
    tgtidx["_index"] = parse_index(index);
    if (!type.empty()) {
      tgtidx["_type"] = type;
    }
    tgtidx["_id"] = boost::uuids::to_string(generate_uuid());

    boost::json::object header;
    header["index"] = tgtidx;

    payload += boost::json::serialize(header) + "\n";
    payload += data + "\n";
  }
  return payload;
}

// Summarize the errors in an Elasticsearch response body; empty when the
// response reports success. The body comes from the network, so no shape can
// be assumed: unexpected or non-JSON content is reported (truncated) rather
// than trusted, and nothing here throws.
inline std::string extract_errors(const std::string &body) {
  namespace json = boost::json;
  constexpr std::size_t max_snippet = 200;
  boost::system::error_code ec;
  const json::value parsed = json::parse(body, ec);
  if (ec || !parsed.is_object()) {
    return "Unrecognized response: " + body.substr(0, max_snippet);
  }
  const json::object &root = parsed.get_object();

  // Top-level error (authentication failure, bad request, ...): an object
  // carrying a "reason" in current versions, historically sometimes a string.
  if (const json::value *error = root.if_contains("error")) {
    if (const json::object *error_obj = error->if_object()) {
      if (const json::value *reason = error_obj->if_contains("reason")) {
        if (reason->is_string()) {
          return std::string(reason->get_string().c_str());
        }
      }
    }
    if (error->is_string()) {
      return std::string(error->get_string().c_str());
    }
    return json::serialize(*error).substr(0, max_snippet);
  }

  const json::value *errors_flag = root.if_contains("errors");
  if (errors_flag == nullptr || !errors_flag->is_bool() || !errors_flag->get_bool()) {
    return "";
  }

  // Per-item errors: {"errors":true,"items":[{"index":{"error":{"reason":...
  // The action key of each item mirrors the request ("index" for this client).
  std::string errors;
  if (const json::value *items = root.if_contains("items")) {
    if (const json::array *items_array = items->if_array()) {
      for (const json::value &item : *items_array) {
        const json::object *item_obj = item.if_object();
        if (item_obj == nullptr) continue;
        for (const auto &action : *item_obj) {
          const json::object *action_obj = action.value().if_object();
          if (action_obj == nullptr) continue;
          const json::value *error = action_obj->if_contains("error");
          if (error == nullptr) continue;
          if (const json::object *error_obj = error->if_object()) {
            if (const json::value *reason = error_obj->if_contains("reason")) {
              if (reason->is_string()) {
                str::format::append_list(errors, std::string(reason->get_string().c_str()));
                continue;
              }
            }
          }
          str::format::append_list(errors, json::serialize(*error).substr(0, max_snippet));
        }
      }
    }
  }
  if (errors.empty()) {
    errors = "Bulk request reported errors but no reason was found in the response";
  }
  return errors;
}

}  // namespace elastic_bulk
