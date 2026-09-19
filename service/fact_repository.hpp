// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithms/sha256.hpp>

#include <algorithm>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace nsclient {
namespace core {

// Central repository for host "facts": structured, hierarchical inventory
// about this host - which volumes exist, what the hardware is, which software
// is installed - contributed by modules through fetchFacts() and read by the
// REST API, the web UI and the fleet sync.
//
// Facts are to inventory what tags (tag_repository.hpp) are to group
// membership. A tag is a flat, cheap, always-present `key=value` string that a
// fleet selector can match on; a fact set is a JSON subtree with lists of
// records in it. The two do not replace each other and nothing here touches
// the tag repository.
//
// The unit of storage is a **fact set**, named by the same dotted id the
// settings, the docs and the console speak about: a depth-1 id (`os`,
// `hardware`) or a depth-2 one (`software.installed`) when a module splits
// enablement finer than its top-level key. Each set records which plugin owns
// it, so unloading a module drops what it published rather than freezing it.
// get_all() assembles the sets into one document by placing each at its path,
// which is how `software.installed` and `software.hotfixes` become one
// `software` object without either producer knowing about the other.
//
// Nothing is stored unless the core asked for it: the enabled list in
// [/settings/facts] is resolved by the plugin manager before a producer is
// called, and retain_only() drops what a settings reload turned off.
//
// Thread safety as tag_repository: one mutex, copies out.
class fact_repository {
 public:
  enum class set_result {
    changed,    // stored or removed; revision bumped
    unchanged,  // no-op: the same value again, or removing an absent set
    rejected    // the value broke a document rule; the previous value is kept
  };

  // Document rules (section 2.1 of docs/plans/facts.md). A producer that
  // breaks one has its whole set rejected - a half-applied set would give the
  // server an inventory that never existed on the host.
  static const std::size_t max_key_length = 64;
  static const std::size_t max_depth = 6;
  static const std::size_t max_list_length = 5000;
  static const std::size_t default_max_size = 1048576;

  fact_repository() = default;

  // The serialised-document budget. Settable so [/settings/facts] max size
  // can lower it; a set that would push the document past it is rejected.
  void set_max_size(const std::size_t max_size) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    max_size_ = max_size;
  }
  std::size_t get_max_size() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return max_size_;
  }

  // Replace one fact set with `value`, recording which plugin owns it.
  // Validates the id, the keys, the depth, the list records and the size
  // budget; on `rejected` the previous value is left in place and `error`
  // says why.
  set_result set(const std::string &fact_set, const unsigned int plugin_id, const boost::json::value &value, std::string &error) {
    std::vector<std::string> path;
    if (!split_id(fact_set, path, error)) return set_result::rejected;
    if (value.is_null()) {
      error = "a fact set may not be null (remove it instead)";
      return set_result::rejected;
    }
    // The set's own depth counts against the document budget: a depth-2 id
    // leaves four levels for the producer, not six.
    if (!validate(value, path.size(), error)) return set_result::rejected;

    boost::unique_lock<boost::mutex> lock(mutex_);
    const auto it = sets_.find(fact_set);
    if (it != sets_.end() && it->second.value == value) {
      // Re-publishing the same value is what every producer does on every
      // round; only the owner is refreshed so a reloaded module keeps the set.
      it->second.plugin_id = plugin_id;
      return set_result::unchanged;
    }
    const std::size_t budget = size_without(fact_set) + serialize_size(fact_set, value);
    if (budget > max_size_) {
      error = "the facts document would grow to " + std::to_string(budget) + " bytes, past the " + std::to_string(max_size_) + " byte limit";
      return set_result::rejected;
    }
    owned_set &target = sets_[fact_set];
    target.value = value;
    target.plugin_id = plugin_id;
    bump();
    return set_result::changed;
  }

  set_result remove(const std::string &fact_set) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const auto it = sets_.find(fact_set);
    if (it == sets_.end()) return set_result::unchanged;
    sets_.erase(it);
    bump();
    return set_result::changed;
  }

  // Drop every set a plugin published. Called when a module is unloaded or
  // reloaded, so `docker` disappears with CheckDocker instead of freezing at
  // whatever the socket last said.
  void remove_owned_by(const unsigned int plugin_id) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    bool changed = false;
    for (auto it = sets_.begin(); it != sets_.end();) {
      if (it->second.plugin_id == plugin_id) {
        it = sets_.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    if (changed) bump();
    for (auto it = errors_.begin(); it != errors_.end();) {
      it = it->second.plugin_id == plugin_id ? errors_.erase(it) : std::next(it);
    }
  }

  // Keep only the sets that are still enabled. A set that was turned off in
  // the settings is removed by the core, not by its producer: a producer that
  // is no longer asked for a set simply stops answering, and "not answered"
  // deliberately means "unchanged" (a transient collection failure must not
  // blank the inventory).
  void retain_only(const std::set<std::string> &enabled) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    bool changed = false;
    for (auto it = sets_.begin(); it != sets_.end();) {
      if (enabled.find(it->first) == enabled.end()) {
        it = sets_.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    if (changed) bump();
    for (auto it = errors_.begin(); it != errors_.end();) {
      it = enabled.find(it->first) == enabled.end() ? errors_.erase(it) : std::next(it);
    }
  }

  // The whole document, with every set placed at its dotted path.
  boost::json::object get_all() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return build_document();
  }

  // The subtree at a dotted path: a fact set id (`os`, `software.installed`)
  // or any path inside one. boost::none when nothing is there.
  boost::optional<boost::json::value> get(const std::string &path) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const boost::json::object document = build_document();
    if (path.empty()) return boost::json::value(document);
    const boost::json::value *at = document.if_contains(first_component(path));
    if (at == nullptr) return boost::none;
    std::string rest = remainder(path);
    while (!rest.empty()) {
      if (!at->is_object()) return boost::none;
      const boost::json::value *next = at->get_object().if_contains(first_component(rest));
      if (next == nullptr) return boost::none;
      at = next;
      rest = remainder(rest);
    }
    return *at;
  }

  // sha256 of the canonical serialisation. Recomputed lazily on read after a
  // change, so a producer replacing five sets in one round costs one hash.
  std::string get_hash() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (!hash_) hash_ = algorithms::sha256_hex(canonical(boost::json::value(build_document())));
    return hash_.value();
  }

  // The canonical serialisation itself: keys sorted, no whitespace. This is
  // what is hashed and what is uploaded, so the server digests the same bytes.
  std::string get_document() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return canonical(boost::json::value(build_document()));
  }

  // Monotonic change counter: starts at 0 (empty repository) and increments on
  // every effective change. Pollers compare it instead of diffing.
  unsigned long long get_revision() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return revision_;
  }

  // The ids of the sets currently held, in document order.
  std::set<std::string> get_set_ids() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    std::set<std::string> ids;
    for (const auto &entry : sets_) ids.insert(entry.first);
    return ids;
  }

  // Per-set collection errors from the last round. A set that could not be
  // collected says so here (and on /api/v2/facts) instead of being silently
  // absent.
  void set_error(const std::string &fact_set, const unsigned int plugin_id, const std::string &error) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (error.empty()) {
      errors_.erase(fact_set);
      return;
    }
    collection_error &target = errors_[fact_set];
    target.message = error;
    target.plugin_id = plugin_id;
  }

  void clear_error(const std::string &fact_set) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    errors_.erase(fact_set);
  }

  std::map<std::string, std::string> get_errors() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    std::map<std::string, std::string> out;
    for (const auto &entry : errors_) out[entry.first] = entry.second.message;
    return out;
  }

  // Canonical JSON: object keys sorted, no whitespace, numbers in their
  // shortest round-trip form. Public and static so a test can pin the exact
  // bytes - the hash is what the fleet server compares against, so the
  // serialisation is part of the wire contract, not an implementation detail.
  static std::string canonical(const boost::json::value &value) {
    std::string out;
    write_canonical(value, out);
    return out;
  }

  // Whether `id` is a well-formed fact set id: one or two snake_case
  // components, and not one of the two knobs [/settings/facts] carries
  // alongside the sets. Shared with the settings registration so an id that
  // cannot be stored cannot be enabled either.
  static bool is_valid_id(const std::string &id) {
    if (is_reserved_key(id)) return false;
    std::vector<std::string> path;
    std::string error;
    return split_id(id, path, error);
  }

  // The keys in [/settings/facts] that configure the collection rather than
  // name a fact set. `interval` is a well-formed id, so the two are reserved
  // here, once, rather than filtered by every reader of the section.
  static const char *interval_key() { return "interval"; }
  static const char *max_size_key() { return "max size"; }
  static bool is_reserved_key(const std::string &key) { return key == interval_key() || key == max_size_key(); }

  // The core itself owns a fact set (the `agent` one) and registers the two
  // keys above, under the reserved plugin id every core registration uses.
  static unsigned int core_plugin_id() { return 0xffff; }

 private:
  struct owned_set {
    boost::json::value value;
    unsigned int plugin_id = 0;
  };
  struct collection_error {
    std::string message;
    unsigned int plugin_id = 0;
  };

  mutable boost::mutex mutex_;
  // std::map, not unordered: the document is assembled in key order, which is
  // also the canonical order, so the assembly is deterministic for free.
  std::map<std::string, owned_set> sets_;
  std::map<std::string, collection_error> errors_;
  unsigned long long revision_ = 0;
  std::size_t max_size_ = default_max_size;
  mutable boost::optional<std::string> hash_;

  // Callers hold the lock.
  void bump() {
    ++revision_;
    hash_ = boost::none;
  }

  boost::json::object build_document() const {
    boost::json::object root;
    for (const auto &entry : sets_) {
      std::vector<std::string> path;
      std::string error;
      if (!split_id(entry.first, path, error)) continue;  // cannot happen: set() validated it
      if (path.size() == 1) {
        // A depth-1 set may share its top-level key with nothing else, but it
        // can arrive after a depth-2 sibling was placed. Merge rather than
        // overwrite so the order sets_ is walked in cannot lose data.
        merge_into(root, path[0], entry.second.value);
      } else {
        boost::json::value &parent = root[path[0]];
        if (!parent.is_object()) parent = boost::json::object();
        merge_into(parent.get_object(), path[1], entry.second.value);
      }
    }
    return root;
  }

  static void merge_into(boost::json::object &parent, const std::string &key, const boost::json::value &value) {
    const boost::json::value *existing = parent.if_contains(key);
    if (existing != nullptr && existing->is_object() && value.is_object()) {
      boost::json::object merged = existing->get_object();
      for (const auto &field : value.get_object()) merged[field.key()] = field.value();
      parent[key] = merged;
      return;
    }
    parent[key] = value;
  }

  // The serialised size of the document without `fact_set`, so a replacement
  // is budgeted against what it replaces rather than on top of it.
  std::size_t size_without(const std::string &fact_set) const {
    boost::json::object root;
    for (const auto &entry : sets_) {
      if (entry.first == fact_set) continue;
      std::vector<std::string> path;
      std::string error;
      if (!split_id(entry.first, path, error)) continue;
      if (path.size() == 1) {
        merge_into(root, path[0], entry.second.value);
      } else {
        boost::json::value &parent = root[path[0]];
        if (!parent.is_object()) parent = boost::json::object();
        merge_into(parent.get_object(), path[1], entry.second.value);
      }
    }
    return canonical(boost::json::value(root)).size();
  }

  // What adding `value` at `fact_set` costs. An approximation on purpose: the
  // exact document is only known once every set is merged, and re-serialising
  // the whole thing per candidate would make a 5000-record list quadratic.
  // Over-estimating by the key and the punctuation is the safe direction.
  static std::size_t serialize_size(const std::string &fact_set, const boost::json::value &value) {
    return canonical(value).size() + fact_set.size() + 8;
  }

  static std::string first_component(const std::string &path) {
    const std::string::size_type dot = path.find('.');
    return dot == std::string::npos ? path : path.substr(0, dot);
  }
  static std::string remainder(const std::string &path) {
    const std::string::size_type dot = path.find('.');
    return dot == std::string::npos ? std::string() : path.substr(dot + 1);
  }

  static bool is_valid_key(const std::string &key) {
    if (key.empty() || key.size() > max_key_length) return false;
    if (key[0] < 'a' || key[0] > 'z') return false;
    for (const char c : key) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
      if (!ok) return false;
    }
    return true;
  }

  static bool split_id(const std::string &id, std::vector<std::string> &path, std::string &error) {
    path.clear();
    std::string::size_type start = 0;
    while (true) {
      const std::string::size_type dot = id.find('.', start);
      path.push_back(dot == std::string::npos ? id.substr(start) : id.substr(start, dot - start));
      if (dot == std::string::npos) break;
      start = dot + 1;
    }
    if (path.size() > 2) {
      error = "'" + id.substr(0, 128) + "' is not a fact set id: at most two dotted components";
      return false;
    }
    for (const std::string &component : path) {
      if (!is_valid_key(component)) {
        error = "'" + id.substr(0, 128) + "' is not a fact set id: components are snake_case, at most " + std::to_string(max_key_length) + " characters";
        return false;
      }
    }
    return true;
  }

  static bool validate(const boost::json::value &value, const std::size_t depth, std::string &error) {
    if (depth > max_depth) {
      error = "nested deeper than " + std::to_string(max_depth) + " levels";
      return false;
    }
    if (value.is_null()) {
      error = "contains a null; an unknown value is omitted rather than written as null";
      return false;
    }
    if (value.is_object()) {
      for (const auto &field : value.get_object()) {
        const std::string key(field.key());
        if (!is_valid_key(key)) {
          error = "'" + key.substr(0, 128) + "' is not a valid key: snake_case ASCII, at most " + std::to_string(max_key_length) + " characters";
          return false;
        }
        if (!validate(field.value(), depth + 1, error)) {
          error = key + ": " + error;
          return false;
        }
      }
      return true;
    }
    if (value.is_array()) return validate_list(value.get_array(), depth, error);
    return true;  // string, number, bool
  }

  static bool validate_list(const boost::json::array &list, const std::size_t depth, std::string &error) {
    if (list.size() > max_list_length) {
      error = "holds " + std::to_string(list.size()) + " entries, past the " + std::to_string(max_list_length) + " entry limit";
      return false;
    }
    // Either a list of records - every entry an object with a unique `id` the
    // server can diff on - or a plain list of strings (`addresses`,
    // `modules`). A mixture is neither, and a record list without ids makes
    // every refresh look like a full replacement on the server.
    bool any_object = false;
    for (const auto &entry : list) any_object = any_object || entry.is_object();
    if (!any_object) {
      for (const auto &entry : list) {
        if (!entry.is_string()) {
          error = "is a list of neither records nor strings";
          return false;
        }
      }
      return true;
    }
    std::set<std::string> ids;
    for (const auto &entry : list) {
      if (!entry.is_object()) {
        error = "mixes records with plain values";
        return false;
      }
      const boost::json::value *id = entry.get_object().if_contains("id");
      if (id == nullptr || !id->is_string() || id->get_string().empty()) {
        error = "has a record without a non-empty string 'id'";
        return false;
      }
      const std::string id_text(id->get_string());
      if (!ids.insert(id_text).second) {
        error = "has two records with id '" + id_text.substr(0, 128) + "'";
        return false;
      }
      if (!validate(entry, depth + 1, error)) {
        error = "record '" + id_text.substr(0, 128) + "': " + error;
        return false;
      }
    }
    return true;
  }

  static void write_canonical(const boost::json::value &value, std::string &out) {
    switch (value.kind()) {
      case boost::json::kind::object: {
        const boost::json::object &object = value.get_object();
        std::vector<std::string> keys;
        keys.reserve(object.size());
        for (const auto &field : object) keys.emplace_back(field.key());
        std::sort(keys.begin(), keys.end());
        out += '{';
        for (std::size_t i = 0; i < keys.size(); ++i) {
          if (i > 0) out += ',';
          write_canonical(boost::json::value(keys[i]), out);
          out += ':';
          write_canonical(object.at(keys[i]), out);
        }
        out += '}';
        return;
      }
      case boost::json::kind::array: {
        // List order is the producer's, not ours: it is what the check that
        // owns the instance enumerates, and re-sorting it here would make the
        // hash disagree with the document the server was sent.
        out += '[';
        bool first = true;
        for (const auto &entry : value.get_array()) {
          if (!first) out += ',';
          first = false;
          write_canonical(entry, out);
        }
        out += ']';
        return;
      }
      default:
        // Boost.JSON already emits the shortest round-trip form for doubles
        // and no whitespace for scalars, so a scalar is canonical as it
        // serialises. Only the object key order needed fixing.
        out += boost::json::serialize(value);
        return;
    }
  }
};

typedef std::shared_ptr<fact_repository> fact_repository_instance;

}  // namespace core
}  // namespace nsclient
