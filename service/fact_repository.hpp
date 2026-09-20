// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <cstddef>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// The document hash is a SHA-256, which is OpenSSL's job here as everywhere
// else in the tree. OpenSSL is optional (find_package(OpenSSL), and
// build/docker/Dockerfile.no-openssl proves the build survives without it),
// so a build without it collects and serves facts as usual and simply has no
// hash to offer: the one consumer that needs it, the fleet upload, is
// OpenSSL-only anyway, and the REST ETag is a cache optimisation.
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#endif

namespace nsclient {
namespace core {

// Copy a JSON string in full.
//
// boost::json::string converts to std::string only where json::string_view is
// std::string_view; on the Boost the EL9 build uses (1.75) it is not, so the
// conversion has to be spelled out. data()/size() rather than c_str() for the
// same reason libs/onboarding/json_util.hpp gives: c_str() truncates at an
// embedded nul, which turns a hostile value into a harmless looking one.
inline std::string json_to_string(const boost::json::string &value) { return std::string(value.data(), value.size()); }

// Central repository for host "facts": the opt-in inventory document modules
// produce on a schedule (fetchFacts) and the agent serves on /api/v2/facts.
//
// Facts are to inventory what tags (tag_repository) are to group membership:
// a tag is a flat, always-present key=value a fleet selector can match on,
// while a fact set is a structured document - objects, scalars and lists of
// records - that describes what the host *is*. Nothing is collected until a
// fact set is enabled in [/settings/facts], so an empty document is the
// default and what a fresh install reports.
//
// The document is one JSON object whose first level is a fact set: `os`,
// `hardware`, `storage`, ... Each set is owned by exactly one plugin, is
// replaced whole, and is validated before it is accepted - a set that breaks
// a rule is rejected in one piece and the previous value is kept, so a
// producer that starts returning garbage cannot corrupt what the server
// already has.
//
// Thread safety as tag_repository: one mutex, copies out.
class fact_repository {
 public:
  // Result of a set(). `unchanged` is the common case - a producer returning
  // the same inventory every hour - and must not bump the revision, or the
  // fleet sync would re-upload the whole document on every round.
  enum class set_result {
    changed,    // stored or removed; revision bumped
    unchanged,  // the same value again
    rejected    // invalid document, size budget exceeded, or owned elsewhere
  };

  // Document rules (docs: the facts concepts page). They exist so a consumer
  // - the web UI, the fleet server, a later "context on failure" feature -
  // can rely on the shape without defensive parsing everywhere.
  //
  // A key is snake_case ASCII: [a-z][a-z0-9_]*, at most 64 characters.
  static constexpr std::size_t max_key_length = 64;
  // Nesting depth, counting the fact set itself as level 1.
  static constexpr std::size_t max_depth = 6;
  // Records in one list.
  static constexpr std::size_t max_list_length = 5000;
  // Serialised size of the whole document. Overridable from
  // [/settings/facts] max size.
  static constexpr std::size_t default_max_size = 1048576;

  fact_repository() { canonical_ = "{}"; }

  // Replace one fact set (a top-level key) with `value`, recording which
  // plugin owns it. Returns `rejected` without touching the stored document
  // when the name is not a valid key, the value is not an object, the
  // document rules are broken, the size budget would be exceeded, or another
  // plugin already produces this set; `error` then says why.
  set_result set(const std::string &fact_set, const unsigned int plugin_id, const boost::json::value &value, std::string &error) {
    if (!is_valid_key(fact_set)) {
      error = "'" + clip(fact_set) + "' is not a valid fact set name (snake_case, at most " + std::to_string(max_key_length) + " characters)";
      return set_result::rejected;
    }
    if (!value.is_object()) {
      error = "fact set '" + fact_set + "' must be a JSON object";
      return set_result::rejected;
    }
    if (!validate(fact_set, value, 1, error)) return set_result::rejected;

    boost::unique_lock<boost::mutex> lock(mutex_);
    const std::map<std::string, unsigned int>::const_iterator owner = owners_.find(fact_set);
    if (owner != owners_.end() && owner->second != plugin_id) {
      error = "fact set '" + fact_set + "' is already produced by another module";
      return set_result::rejected;
    }
    boost::json::object candidate = facts_;
    candidate[fact_set] = value;
    // The canonical form decides whether anything changed: it is what the
    // hash, the size budget and the fleet upload are all taken from, so a
    // producer that returns the same facts in a different key order is
    // rightly a no-op rather than a revision bump the server has to chase.
    const std::string canonical = canonicalise(candidate);
    if (canonical == canonical_) {
      owners_[fact_set] = plugin_id;
      return set_result::unchanged;
    }
    if (canonical.size() > max_size_) {
      error = "fact set '" + fact_set + "' would take the facts document past the " + std::to_string(max_size_) + " byte budget";
      return set_result::rejected;
    }
    facts_ = candidate;
    owners_[fact_set] = plugin_id;
    store(canonical);
    return set_result::changed;
  }

  // Drop one fact set, whoever owns it. A producer does this by returning an
  // explicit null for the set (the docker socket went away).
  set_result remove(const std::string &fact_set) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (!erase_locked(fact_set)) return set_result::unchanged;
    store(canonicalise(facts_));
    return set_result::changed;
  }

  // Drop every set a plugin owns. Called when a module is unloaded or
  // reloaded, so unloading CheckDocker drops `docker` rather than freezing
  // the last value it collected.
  void remove_owned_by(const unsigned int plugin_id) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    declared_.erase(plugin_id);
    std::vector<std::string> owned;
    for (const std::pair<const std::string, unsigned int> &owner : owners_) {
      if (owner.second == plugin_id) owned.push_back(owner.first);
    }
    bool changed = false;
    for (const std::string &fact_set : owned) changed = erase_locked(fact_set) || changed;
    if (changed) store(canonicalise(facts_));
  }

  // Take what one producer says it is configured to produce, and drop the
  // sets it owns that are not on that list.
  //
  // This is what turning a fact set off does: enablement lives in the
  // producing module's own configuration (the module decides what to return),
  // so the core learns a set was switched off by the module no longer
  // mentioning it. Only ever called after a round that module completed - a
  // module that failed or could not be reached keeps what it had, so a
  // transient failure never blanks the inventory.
  //
  // `produced` holds the ids the module speaks about, which are a fact set
  // (`os`) or a dotted path into one (`software.installed`), because a module
  // may split its configuration finer than the document splits its keys.
  void retain_only(const unsigned int plugin_id, const std::set<std::string> &produced) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    declared_[plugin_id] = produced;
    std::vector<std::string> owned;
    for (const std::pair<const std::string, unsigned int> &owner : owners_) {
      if (owner.second == plugin_id && !mentions(produced, owner.first)) owned.push_back(owner.first);
    }
    bool changed = false;
    for (const std::string &fact_set : owned) changed = erase_locked(fact_set) || changed;
    if (changed) store(canonicalise(facts_));
  }

  // The whole document.
  boost::json::object get_all() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return facts_;
  }

  // One subtree, addressed by a dotted path (`os`, `software.installed`).
  // None when the path is not in the document.
  boost::optional<boost::json::value> get(const std::string &path) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const boost::json::value *current = nullptr;
    std::size_t start = 0;
    while (start <= path.size()) {
      const std::size_t dot = path.find('.', start);
      const std::string segment = path.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
      if (segment.empty()) return boost::none;
      const boost::json::object &parent = current == nullptr ? facts_ : (current->is_object() ? current->as_object() : empty_object());
      current = parent.if_contains(segment);
      if (current == nullptr) return boost::none;
      if (dot == std::string::npos) break;
      start = dot + 1;
    }
    if (current == nullptr) return boost::none;
    return *current;
  }

  // sha256 of the canonical serialisation: keys sorted, no whitespace. This
  // is what the fleet server compares against and what the REST layer serves
  // as an ETag, so it is computed from bytes that do not depend on insertion
  // order. Computed lazily, so a producer replacing five sets in one round
  // costs one hash.
  std::string get_hash() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (hash_dirty_) {
      hash_ = sha256_hex(canonical_);
      hash_dirty_ = false;
    }
    return hash_;
  }

  // Whether this build can hash the document at all (see the OpenSSL note at
  // the top). Consumers that would otherwise publish an empty hash - the REST
  // ETag, the state report - ask first.
  static bool can_hash() {
#ifdef HAVE_OPENSSL
    return true;
#else
    return false;
#endif
  }

  // The canonical serialisation the hash is taken of - the bytes an upload
  // sends and a consumer can hash itself to check.
  std::string get_canonical() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return canonical_;
  }

  // Monotonic change counter: starts at 0 (empty document) and increments on
  // every effective change.
  unsigned long long get_revision() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return revision_;
  }

  // What the producers said they are configured to produce, at their last
  // completed round: the honest answer to "what is this host collecting",
  // including a set that is enabled but currently failing to collect.
  std::set<std::string> get_enabled() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    std::set<std::string> enabled;
    for (const std::pair<const unsigned int, std::set<std::string>> &entry : declared_) {
      enabled.insert(entry.second.begin(), entry.second.end());
    }
    return enabled;
  }

  // Per-set collection errors from the last round: a set that is enabled and
  // could not be collected says so here, so the UI can show
  // "software.installed: access denied" instead of silence.
  std::map<std::string, std::string> get_errors() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return errors_;
  }

  // Replace the error map. Errors belong to a round, not to a set that may
  // never be collected again, so a round reports all of them at once.
  void set_errors(const std::map<std::string, std::string> &errors) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    errors_ = errors;
  }

  // When the last round completed, as an ISO 8601 UTC string; empty until one
  // has.
  std::string get_collected() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return collected_;
  }

  void mark_collected(const std::string &timestamp) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    collected_ = timestamp;
  }

  // The serialised size budget ([/settings/facts] max size). A budget below
  // what an empty document needs is ignored.
  void set_max_size(const std::size_t max_size) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (max_size >= 2) max_size_ = max_size;
  }
  std::size_t get_max_size() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return max_size_;
  }

  // A fact set name, an object key and the first segment of a settings id all
  // follow the same rule.
  static bool is_valid_key(const std::string &key) {
    if (key.empty() || key.size() > max_key_length) return false;
    if (key[0] < 'a' || key[0] > 'z') return false;
    for (const char c : key) {
      const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
      if (!ok) return false;
    }
    return true;
  }

  // The canonical serialisation of any value: object keys sorted, no
  // whitespace. Static so the hash can be reproduced from a document a
  // consumer holds.
  static std::string canonicalise(const boost::json::value &value) {
    std::string out;
    write_canonical(value, out);
    return out;
  }

 private:
  static const boost::json::object &empty_object() {
    static const boost::json::object empty;
    return empty;
  }

  // The canonical form's digest, as 64 lowercase hex characters; empty in a
  // build without OpenSSL (see the note at the top of the file).
  static std::string sha256_hex(const std::string &bytes) {
#ifdef HAVE_OPENSSL
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest, &digest_length, EVP_sha256(), nullptr) != 1) return "";
    static const char *digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(static_cast<std::size_t>(digest_length) * 2);
    for (unsigned int i = 0; i < digest_length; ++i) {
      hex.push_back(digits[(digest[i] >> 4) & 0xf]);
      hex.push_back(digits[digest[i] & 0xf]);
    }
    return hex;
#else
    (void)bytes;
    return "";
#endif
  }

  // An id mentions a set when it is the set, or something inside it:
  // `software.installed` keeps the `software` set alive.
  static bool mentions(const std::set<std::string> &ids, const std::string &fact_set) {
    if (ids.count(fact_set) > 0) return true;
    const std::string prefix = fact_set + ".";
    for (const std::string &id : ids) {
      if (id.size() > prefix.size() && id.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
  }

  static std::string clip(const std::string &text) { return text.size() <= 64 ? text : text.substr(0, 64) + "..."; }

  static void write_canonical(const boost::json::value &value, std::string &out) {
    if (value.is_object()) {
      const boost::json::object &object = value.as_object();
      std::vector<std::string> keys;
      keys.reserve(object.size());
      for (const boost::json::key_value_pair &entry : object) keys.push_back(std::string(entry.key()));
      std::sort(keys.begin(), keys.end());
      out += "{";
      bool first = true;
      for (const std::string &key : keys) {
        if (!first) out += ",";
        first = false;
        out += boost::json::serialize(boost::json::value(key));
        out += ":";
        write_canonical(object.at(key), out);
      }
      out += "}";
      return;
    }
    if (value.is_array()) {
      out += "[";
      bool first = true;
      for (const boost::json::value &item : value.as_array()) {
        if (!first) out += ",";
        first = false;
        write_canonical(item, out);
      }
      out += "]";
      return;
    }
    out += boost::json::serialize(value);
  }

  // Walk a fact set and check it against the document rules. `path` is the
  // dotted position in the document, so the rejection message says where the
  // problem is rather than just that there is one.
  static bool validate(const std::string &path, const boost::json::value &value, const std::size_t depth, std::string &error) {
    if (value.is_null()) {
      error = "'" + path + "' is null: an unknown value is omitted, never written as null";
      return false;
    }
    if (value.is_object()) {
      if (depth > max_depth) {
        error = "'" + path + "' nests deeper than " + std::to_string(max_depth) + " levels";
        return false;
      }
      for (const boost::json::key_value_pair &entry : value.as_object()) {
        const std::string key(entry.key());
        if (!is_valid_key(key)) {
          error = "'" + path + "." + clip(key) + "' is not a valid key (snake_case, at most " + std::to_string(max_key_length) + " characters)";
          return false;
        }
        if (!validate(path + "." + key, entry.value(), depth + 1, error)) return false;
      }
      return true;
    }
    if (value.is_array()) return validate_list(path, value.as_array(), depth, error);
    // Strings, numbers and booleans are the scalars; nothing else can reach
    // here (boost::json has no other kinds).
    return true;
  }

  // A list is either a list of records - objects carrying an `id` that is
  // unique in the list and stable across runs, which is what lets the server
  // diff two uploads instead of treating every refresh as a replacement - or
  // a plain list of strings (`addresses`, `modules`). Mixing the two, or
  // listing bare numbers, leaves a consumer with no way to tell what it has.
  static bool validate_list(const std::string &path, const boost::json::array &list, const std::size_t depth, std::string &error) {
    if (list.size() > max_list_length) {
      error = "'" + path + "' holds " + std::to_string(list.size()) + " entries, more than the " + std::to_string(max_list_length) + " allowed";
      return false;
    }
    if (list.empty()) return true;
    const bool records = list.front().is_object();
    std::set<std::string> ids;
    for (const boost::json::value &item : list) {
      if (item.is_object() != records) {
        error = "'" + path + "' mixes records and plain values: a list is either records or strings";
        return false;
      }
      if (!records) {
        if (!item.is_string()) {
          error = "'" + path + "' holds a plain value that is not a string: a list is either records or strings";
          return false;
        }
        continue;
      }
      const boost::json::value *id = item.as_object().if_contains("id");
      if (id == nullptr || !id->is_string() || id->as_string().empty()) {
        error = "'" + path + "' holds a record without a non-empty string id";
        return false;
      }
      const std::string key = json_to_string(id->as_string());
      if (!ids.insert(key).second) {
        error = "'" + path + "' holds two records with the id '" + clip(key) + "'";
        return false;
      }
      if (!validate(path + "[" + key + "]", item, depth + 1, error)) return false;
    }
    return true;
  }

  // Erase one set, leaving the canonical form to the caller: a round that
  // drops several sets re-serialises once, not once per set. Callers hold the
  // lock.
  bool erase_locked(const std::string &fact_set) {
    owners_.erase(fact_set);
    if (facts_.if_contains(fact_set) == nullptr) return false;
    facts_.erase(fact_set);
    return true;
  }

  void store(const std::string &canonical) {
    canonical_ = canonical;
    hash_dirty_ = true;
    ++revision_;
  }

  mutable boost::mutex mutex_;
  boost::json::object facts_;
  // Which plugin produced each set, so unloading a module takes its sets with
  // it and two modules cannot fight over one top-level key.
  std::map<std::string, unsigned int> owners_;
  // What each producer last said it is configured to produce.
  std::map<unsigned int, std::set<std::string>> declared_;
  std::map<std::string, std::string> errors_;
  std::string collected_;
  std::string canonical_;
  mutable std::string hash_;
  mutable bool hash_dirty_ = true;
  unsigned long long revision_ = 0;
  std::size_t max_size_ = default_max_size;
};

typedef std::shared_ptr<fact_repository> fact_repository_instance;

}  // namespace core
}  // namespace nsclient
