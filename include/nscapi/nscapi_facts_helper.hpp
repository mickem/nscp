// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/json.hpp>
#include <cstdint>
#include <ctime>
#include <map>
#include <memory>
#include <nscapi/dll_defines.hpp>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// How a module produces host facts.
//
// Which fact sets a module produces is the module's own configuration, read in
// its loadModuleEx like everything else it is configured with. The core asks
// every producer on a schedule and takes what it gets: the sets in the
// response are what this module is configured to produce, so a set it stops
// returning is dropped from the inventory, and a set it is configured to
// produce but could not collect says so through `error()` and keeps the value
// the core already has.
//
//   using namespace nscapi::facts;
//   void CheckSystem::fetchFacts(const request &req, response &out) {
//     if (facts_os_) {
//       section os = out.set("os");
//       os.value("family", "windows").value("name", name).value("version", version);
//       os.time("boot_time", boot_time);
//     }
//     if (facts_software_installed_) {
//       record_list installed = out.set("software").list("installed");
//       for (const auto &app : apps)
//         installed.record(app.name).value("version", app.version).date("installed", app.installed_at);
//     }
//     if (facts_hardware_) {
//       try { /* ... */ } catch (const std::exception &e) { out.error("hardware", e.what()); }
//     }
//   }
//
// `req.reason()` is startup, scheduled, reload or manual, so a producer whose
// collection is expensive can hand back its last snapshot instead of
// collecting again.
//
// What the builder is for, beyond saving typing:
//
//  - `record(id)` writes the record's `id` field, so a list of records without
//    ids cannot be built. The server diffs lists by id; without one every
//    refresh looks like a full replacement.
//  - An empty string is not a value: the document rules say an unknown value
//    is omitted, never written as "" or null, so `value(key, "")` is a no-op
//    and a producer does not have to guard every field it may not have.
//  - Keys are checked against the document rules (snake_case ASCII, at most 64
//    characters) as they are written. A key that breaks them is dropped and
//    the set carries an error saying so, rather than the core rejecting the
//    whole set at the far end of the ABI.
//  - `time()` and `date()` format timestamps the one way the document allows
//    (ISO 8601 UTC, and YYYY-MM-DD for a date without a time).
//
// A record's `id` is a value, not a key, so it is whatever the check that
// reports on that instance calls it: a drive letter, an interface name, a
// service name. That equality is the hook a later "context on failure"
// feature hangs on, and the producers' unit tests are where it is enforced.
namespace nscapi {
namespace facts {

// Copy a JSON string in full.
//
// boost::json::string converts to std::string only where json::string_view is
// std::string_view; on the Boost the EL9 build uses (1.75) it is not, so the
// conversion has to be spelled out. data()/size() rather than c_str() for the
// same reason libs/onboarding/json_util.hpp gives: c_str() truncates at an
// embedded nul, which turns a hostile value into a harmless looking one.
inline std::string json_to_string(const boost::json::string &value) { return std::string(value.data(), value.size()); }

// The document rules the builder enforces as it writes. They mirror
// nsclient::core::fact_repository, which enforces them again when it accepts
// a set - the core cannot trust a module to have used this builder.
NSCAPI_EXPORT bool is_valid_key(const std::string &key);
// ISO 8601 UTC ("2026-09-19T14:03:11Z") and a plain date ("2026-09-19").
NSCAPI_EXPORT std::string format_time(std::time_t when);
NSCAPI_EXPORT std::string format_date(std::time_t when);
// The `facts` member of the envelope core_wrapper::get_facts_json() returns,
// for a consumer that wants the tree rather than the string. An empty object
// when the envelope has none (a core without the API, an absent subtree).
NSCAPI_EXPORT boost::json::value parse_document(const std::string &envelope);

namespace detail {

// One object or array under construction.
//
// The tree is built out of separately allocated nodes rather than straight
// into a boost::json::object because a producer holds handles to what it is
// filling in: `section os = out.set("os")` stays alive while the next set is
// added, and inserting into a boost::json::object can rehash and invalidate
// every reference into it. A node, once allocated, does not move.
class node {
 public:
  enum class kind { object, array };

  node(const kind node_kind, std::string set, std::shared_ptr<std::map<std::string, std::string>> problems)
      : kind_(node_kind), set_(std::move(set)), problems_(std::move(problems)) {}

  bool is_array() const { return kind_ == kind::array; }
  const std::string &set() const { return set_; }

  // Record a producer error against the fact set this node belongs to. The
  // set is still shipped: a dropped key is worth reporting, not worth losing
  // the rest of the inventory over.
  void problem(const std::string &message) const {
    if (!problems_) return;
    (*problems_)[set_] = message;
  }

  void add_value(const std::string &key, boost::json::value value) {
    if (!accepts(key)) return;
    values_.emplace_back(key, std::move(value));
  }

  std::shared_ptr<node> add_child(const std::string &key, const kind child_kind) {
    std::shared_ptr<node> child = std::make_shared<node>(child_kind, set_, problems_);
    if (!accepts(key)) return child;  // dropped, but the caller still gets a usable handle
    children_.emplace_back(key, child);
    return child;
  }

  std::shared_ptr<node> add_item() {
    std::shared_ptr<node> item = std::make_shared<node>(kind::object, set_, problems_);
    items_.push_back(item);
    return item;
  }

  void add_string(std::string value) { strings_.push_back(std::move(value)); }

  boost::json::value build() const {
    if (kind_ == kind::array) {
      boost::json::array array;
      for (const std::shared_ptr<node> &item : items_) array.push_back(item->build());
      for (const std::string &value : strings_) array.push_back(boost::json::value(value));
      return array;
    }
    boost::json::object object;
    for (const std::pair<std::string, boost::json::value> &value : values_) object[value.first] = value.second;
    for (const std::pair<std::string, std::shared_ptr<node>> &child : children_) object[child.first] = child.second->build();
    return object;
  }

 private:
  bool accepts(const std::string &key) {
    if (is_valid_key(key)) return true;
    problem("key '" + (key.size() > 64 ? key.substr(0, 64) + "..." : key) + "' is not a valid fact key and was dropped");
    return false;
  }

  kind kind_;
  std::string set_;
  std::shared_ptr<std::map<std::string, std::string>> problems_;
  std::vector<std::pair<std::string, boost::json::value>> values_;
  std::vector<std::pair<std::string, std::shared_ptr<node>>> children_;
  std::vector<std::shared_ptr<node>> items_;
  std::vector<std::string> strings_;
};

typedef std::shared_ptr<node> node_ptr;

}  // namespace detail

class record_list;

// An object in the document: a fact set, a nested object, or one record of a
// list. A handle, cheap to copy and safe to keep while siblings are added.
class section {
 public:
  explicit section(detail::node_ptr node) : node_(std::move(node)) {}

  // A string value. An empty string is not written: an unknown value is
  // omitted from the document, never written as "" or null.
  section &value(const std::string &key, const std::string &text) {
    if (!text.empty()) node_->add_value(key, boost::json::value(text));
    return *this;
  }
  section &value(const std::string &key, const char *text) { return value(key, std::string(text == nullptr ? "" : text)); }
  section &value(const std::string &key, const bool flag) {
    node_->add_value(key, boost::json::value(flag));
    return *this;
  }
  section &value(const std::string &key, const double number) {
    node_->add_value(key, boost::json::value(number));
    return *this;
  }
  // Every integral type a producer is likely to hold (int, long, size_t, a
  // Windows DWORD), without an overload per spelling and without bool
  // silently becoming 1.
  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value>::type>
  section &value(const std::string &key, const T number) {
    if (std::is_signed<T>::value) {
      node_->add_value(key, boost::json::value(static_cast<std::int64_t>(number)));
    } else {
      node_->add_value(key, boost::json::value(static_cast<std::uint64_t>(number)));
    }
    return *this;
  }

  // A point in time, as ISO 8601 UTC. Epoch 0 is "not known", and is skipped
  // the way an empty string is.
  section &time(const std::string &key, const std::time_t when) {
    if (when > 0) node_->add_value(key, boost::json::value(format_time(when)));
    return *this;
  }
  // A date without a time (an install date), as YYYY-MM-DD.
  section &date(const std::string &key, const std::time_t when) {
    if (when > 0) node_->add_value(key, boost::json::value(format_date(when)));
    return *this;
  }

  // A plain list of strings: addresses, module names. Anything with more than
  // one field per entry is a list of records instead.
  section &strings(const std::string &key, const std::vector<std::string> &values) {
    const detail::node_ptr array = node_->add_child(key, detail::node::kind::array);
    for (const std::string &value : values) {
      if (!value.empty()) array->add_string(value);
    }
    return *this;
  }

  // A nested object.
  section object(const std::string &key) { return section(node_->add_child(key, detail::node::kind::object)); }

  // A list of records. The type is spelled record_list because the member
  // that opens one is called list(), which is how it reads at a call site.
  inline record_list list(const std::string &key);

 private:
  detail::node_ptr node_;
};

// A list of records. Every record carries an id that is unique within the
// list and stable across runs - that is what lets the fleet server say "this
// package appeared on Tuesday" instead of re-reading the whole list.
class record_list {
 public:
  explicit record_list(detail::node_ptr node) : node_(std::move(node)) {}

  section record(const std::string &id) {
    const detail::node_ptr item = node_->add_item();
    section record_section(item);
    if (id.empty()) {
      node_->problem("a record was added without an id");
    } else {
      record_section.value("id", id);
    }
    return record_section;
  }

 private:
  detail::node_ptr node_;
};

inline record_list section::list(const std::string &key) { return record_list(node_->add_child(key, detail::node::kind::array)); }

// Why the core is asking, this round.
class request {
 public:
  // Parses the core's request; an unparseable one leaves the reason empty,
  // which reads as an ordinary scheduled round.
  NSCAPI_EXPORT explicit request(const std::string &json);

  // startup, scheduled, reload or manual.
  const std::string &reason() const { return reason_; }

 private:
  std::string reason_;
};

// What the module produces this round.
class response {
 public:
  response() : problems_(std::make_shared<std::map<std::string, std::string>>()) {}

  // Start (or continue) a fact set. Calling it twice for the same set returns
  // the same section, so a producer may fill `software.installed` and
  // `software.hotfixes` from two places.
  section set(const std::string &name) {
    const std::map<std::string, detail::node_ptr>::const_iterator it = sets_.find(name);
    if (it != sets_.end()) return section(it->second);
    const detail::node_ptr node = std::make_shared<detail::node>(detail::node::kind::object, name, problems_);
    sets_[name] = node;
    order_.push_back(name);
    return section(node);
  }

  // Drop a set the core may be holding: the docker socket went away, the
  // module no longer produces it. Distinct from not returning it, which
  // leaves the last known value in place so a transient failure never blanks
  // the inventory.
  void remove(const std::string &name) {
    sets_.erase(name);
    order_.erase(std::remove(order_.begin(), order_.end(), name), order_.end());
    removed_.insert(name);
  }

  // Why a set that was asked for is not here. Reported per set on
  // /api/v2/facts, so a consumer can show "software.installed: access denied"
  // rather than silence.
  void error(const std::string &id, const std::string &message) { (*problems_)[id] = message; }

  // This round failed outright: the producer threw, or could not run at all.
  // Reported at the top level rather than per set, because the core must not
  // read "I failed" as "I no longer produce any of this" and drop what it
  // already has. The generated module glue calls this when a producer throws.
  void failed(const std::string &message) { failure_ = message; }

  NSCAPI_EXPORT std::string to_json() const;

 private:
  std::map<std::string, detail::node_ptr> sets_;
  // Insertion order, so the JSON a producer's unit test sees does not depend
  // on how the set names happen to sort.
  std::vector<std::string> order_;
  std::set<std::string> removed_;
  std::string failure_;
  std::shared_ptr<std::map<std::string, std::string>> problems_;
};

}  // namespace facts
}  // namespace nscapi
