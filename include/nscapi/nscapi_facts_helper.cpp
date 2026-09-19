// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/date_time/posix_time/posix_time.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

namespace nscapi {
namespace facts {

bool is_valid_key(const std::string &key) {
  if (key.empty() || key.size() > 64) return false;
  if (key[0] < 'a' || key[0] > 'z') return false;
  for (const char c : key) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok) return false;
  }
  return true;
}

std::string format_time(const std::time_t when) {
  const boost::posix_time::ptime moment = boost::posix_time::from_time_t(when);
  // to_iso_extended_string renders fractional seconds when the ptime has any;
  // from_time_t never does, so this is always "YYYY-MM-DDTHH:MM:SS".
  return boost::posix_time::to_iso_extended_string(moment) + "Z";
}

std::string format_date(const std::time_t when) {
  const boost::gregorian::date day = boost::posix_time::from_time_t(when).date();
  return boost::gregorian::to_iso_extended_string(day);
}

boost::json::value parse_document(const std::string &envelope) {
  try {
    const boost::json::value parsed = boost::json::parse(envelope);
    const boost::json::object *root = parsed.if_object();
    if (root == nullptr) return boost::json::object();
    const boost::json::value *document = root->if_contains("facts");
    if (document == nullptr) return boost::json::object();
    return *document;
  } catch (const std::exception &) {
    // The envelope comes from the core, so this is a core that does not know
    // the call (it returns "{}") or a truncated buffer. Either way the
    // consumer gets an empty document rather than an exception it cannot act
    // on.
    return boost::json::object();
  }
}

request::request(const std::string &json) {
  try {
    const boost::json::value parsed = boost::json::parse(json);
    const boost::json::object *root = parsed.if_object();
    if (root == nullptr) return;
    const boost::json::value *enabled = root->if_contains("enabled");
    if (enabled != nullptr && enabled->is_array()) {
      for (const boost::json::value &id : enabled->as_array()) {
        if (id.is_string()) enabled_.insert(std::string(id.as_string()));
      }
    }
    const boost::json::value *reason = root->if_contains("reason");
    if (reason != nullptr && reason->is_string()) reason_ = std::string(reason->as_string());
  } catch (const std::exception &) {
    // An unparseable request means this round asks for nothing, which is the
    // safe reading: facts are opt-in, so producing nothing is never wrong.
    enabled_.clear();
  }
}

bool request::wants(const std::string &id) const {
  if (enabled_.count(id) > 0) return true;
  // `wants("software")` is true when `software.installed` is enabled: a
  // producer gates the whole set first and then decides per part.
  const std::string prefix = id + ".";
  for (const std::string &candidate : enabled_) {
    if (candidate.size() > prefix.size() && candidate.compare(0, prefix.size(), prefix) == 0) return true;
  }
  return false;
}

std::string response::to_json() const {
  boost::json::object sets;
  for (const std::string &name : order_) {
    const std::map<std::string, detail::node_ptr>::const_iterator it = sets_.find(name);
    if (it == sets_.end()) continue;
    sets[name] = it->second->build();
  }
  // An explicit null removes the set, as opposed to not mentioning it, which
  // leaves what the core already has.
  for (const std::string &name : removed_) sets[name] = boost::json::value(nullptr);

  boost::json::object root;
  root["sets"] = sets;
  if (!problems_->empty()) {
    boost::json::object errors;
    for (const std::pair<const std::string, std::string> &problem : *problems_) errors[problem.first] = problem.second;
    root["errors"] = errors;
  }
  return boost::json::serialize(root);
}

}  // namespace facts
}  // namespace nscapi
