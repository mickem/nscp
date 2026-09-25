// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <cstddef>
#include <ctime>
#include <map>
#include <memory>
#include <nscapi/protobuf/facts.hpp>
#include <set>
#include <string>
#include <vector>

// The upload hash is a SHA-256, which is OpenSSL's job here as everywhere
// else in the tree. OpenSSL is optional (find_package(OpenSSL), and
// build/docker/Dockerfile.no-openssl proves the build survives without it),
// so a build without it collects and serves facts as usual and simply has no
// hash to offer: the one consumer that needs it, the fleet upload, is
// OpenSSL-only anyway.
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#endif

namespace nsclient {
namespace core {

// Central repository for host "facts": the opt-in inventory document modules
// produce on a schedule (fetchFacts) and the agent serves on /api/v2/facts.
//
// Facts are to inventory what tags (tag_repository) are to group membership:
// a tag is a flat, always-present key=value a fleet selector can match on,
// while a fact set is a structured document - objects, scalars and lists of
// records - that describes what the host *is*. Nothing is collected until a
// fact set is enabled in the configuration of the module that produces it, so
// an empty document is the default and what a fresh install reports.
//
// The document is one PB::Facts::Object whose first level is a fact set:
// `os`, `hardware`, `storage`, ... Each set is owned by exactly one plugin, is
// replaced whole, and is validated before it is accepted - a set that breaks a
// rule is rejected in one piece and the previous value is kept, so a producer
// that starts returning garbage cannot corrupt what the server already has.
//
// Protobuf, like every other payload that crosses the plugin ABI here. JSON
// enters the picture once, at the far end: to_json() renders the document the
// way the fleet upload sends it, and get_hash() is the digest of exactly those
// bytes. That is the one boundary where a different implementation has to
// agree on an encoding byte for byte, and JSON is the format that has a
// canonical form to agree on - protobuf deliberately does not define one.
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
  // Encoded size of the whole document. Overridable from [/settings/facts]
  // max size.
  static constexpr std::size_t default_max_size = 1048576;
  // The owner id of the sets the core produces itself (`agent`). Plugin ids
  // count up from 0, so the top of the range is one no module will ever be
  // handed.
  static constexpr unsigned int core_owner = 0xffffffffu;

  // Replace one fact set (a top-level key) with `value`, recording which
  // plugin owns it. Returns `rejected` without touching the stored document
  // when the name is not a valid key, the document rules are broken, the size
  // budget would be exceeded, or another plugin already produces this set;
  // `error` then says why.
  set_result set(const std::string &fact_set, const unsigned int plugin_id, const PB::Facts::Object &value, std::string &error) {
    if (!is_valid_key(fact_set)) {
      error = "'" + clip(fact_set) + "' is not a valid fact set name (snake_case, at most " + std::to_string(max_key_length) + " characters)";
      return set_result::rejected;
    }
    if (!validate_object(fact_set, value, 1, error)) return set_result::rejected;

    // Sorted as it is accepted, which is what makes the comparison below a
    // byte comparison: a producer that returns the same facts in a different
    // key order is rightly a no-op rather than a revision bump the fleet
    // server has to chase.
    PB::Facts::Object candidate = value;
    nscapi::facts::tree::sort_fields(&candidate);
    const std::string encoded = candidate.SerializeAsString();

    boost::unique_lock<boost::mutex> lock(mutex_);
    const std::map<std::string, unsigned int>::const_iterator owner = owners_.find(fact_set);
    if (owner != owners_.end() && owner->second != plugin_id) {
      error = "fact set '" + fact_set + "' is already produced by another module";
      return set_result::rejected;
    }
    const std::map<std::string, std::string>::const_iterator stored = encoded_.find(fact_set);
    if (stored != encoded_.end() && stored->second == encoded) {
      owners_[fact_set] = plugin_id;
      return set_result::unchanged;
    }
    const std::size_t would_be = size_ - (stored == encoded_.end() ? 0 : stored->second.size()) + encoded.size();
    if (would_be > max_size_) {
      error = "fact set '" + fact_set + "' would take the facts document past the " + std::to_string(max_size_) + " byte budget";
      return set_result::rejected;
    }
    sets_[fact_set] = candidate;
    encoded_[fact_set] = encoded;
    size_ = would_be;
    owners_[fact_set] = plugin_id;
    touch();
    return set_result::changed;
  }

  // Drop one fact set, whoever owns it. A producer does this by marking the
  // set removed (the docker socket went away).
  set_result remove(const std::string &fact_set) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (!erase_locked(fact_set)) return set_result::unchanged;
    touch();
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
    if (changed) touch();
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
    if (changed) touch();
  }

  // The whole document, one field per fact set. Sorted, because the sets live
  // in a std::map and every object below them was sorted on the way in.
  PB::Facts::Object get_all() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return build_document_locked();
  }

  // One subtree, addressed by a dotted path (`os`, `software.installed`).
  // None when the path is not in the document.
  boost::optional<PB::Facts::Value> get(const std::string &path) const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    const std::size_t dot = path.find('.');
    const std::string head = path.substr(0, dot);
    if (head.empty()) return boost::none;
    const std::map<std::string, PB::Facts::Object>::const_iterator it = sets_.find(head);
    if (it == sets_.end()) return boost::none;
    PB::Facts::Value current;
    *current.mutable_object_value() = it->second;
    std::size_t start = dot;
    while (start != std::string::npos) {
      ++start;
      const std::size_t next = path.find('.', start);
      const std::string segment = path.substr(start, next == std::string::npos ? std::string::npos : next - start);
      if (segment.empty()) return boost::none;
      if (!current.has_object_value()) return boost::none;
      const PB::Facts::Value *child = nscapi::facts::tree::get(current.object_value(), segment);
      if (child == nullptr) return boost::none;
      const PB::Facts::Value found = *child;
      current = found;
      start = next;
    }
    return current;
  }

  // The document as the fleet upload sends it: canonical JSON, no whitespace,
  // object keys sorted. Static-friendly, so a consumer holding the same tree
  // can reproduce the bytes and check the hash itself.
  std::string to_json() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return nscapi::facts::tree::to_json(build_document_locked());
  }

  // sha256 of to_json(). Computed on demand rather than kept up to date: only
  // the fleet upload needs it, once per round it actually uploads, while the
  // agent's own reads and writes never look at it.
  std::string get_hash() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (hash_dirty_) {
      hash_ = sha256_hex(nscapi::facts::tree::to_json(build_document_locked()));
      hash_dirty_ = false;
    }
    return hash_;
  }

  // The document, its hash and its revision read under one lock, so the bytes
  // an upload sends and the hash it claims for them cannot straddle a round
  // that lands between two separate reads.
  struct snapshot {
    std::string json;
    std::string hash;
    unsigned long long revision = 0;
    // The encoded size of each set, largest first: what a rejected upload
    // names so the operator knows which set to turn off.
    std::vector<std::pair<std::string, std::size_t>> set_sizes;
  };
  snapshot get_snapshot() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    snapshot result;
    const PB::Facts::Object document = build_document_locked();
    result.json = nscapi::facts::tree::to_json(document);
    if (hash_dirty_) {
      hash_ = sha256_hex(result.json);
      hash_dirty_ = false;
    }
    result.hash = hash_;
    result.revision = revision_;
    for (const PB::Facts::Field &field : document.fields()) {
      result.set_sizes.emplace_back(field.key(), nscapi::facts::tree::to_json(field.value()).size());
    }
    std::stable_sort(result.set_sizes.begin(), result.set_sizes.end(),
                     [](const std::pair<std::string, std::size_t> &a, const std::pair<std::string, std::size_t> &b) { return a.second > b.second; });
    return result;
  }

  // Whether this build can hash the document at all (see the OpenSSL note at
  // the top). Consumers that would otherwise publish an empty hash - the
  // state report - ask first.
  static bool can_hash() {
#ifdef HAVE_OPENSSL
    return true;
#else
    return false;
#endif
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

  // When each stored set's values were gathered. Only the sets that are
  // actually held: a timestamp for a set nobody produces would outlive the
  // data it describes.
  std::map<std::string, std::string> get_gathered() const {
    boost::unique_lock<boost::mutex> lock(mutex_);
    return gathered_;
  }

  // Record when a set's values were read off the machine. Separate from
  // set(), and called whatever set() returned, because a producer handing
  // back an unchanged snapshot still has something true to say about its age
  // - and set() stops at `unchanged` without touching anything.
  //
  // An empty timestamp means the producer did not say; the set then has no
  // gathered time and a consumer falls back to the round.
  void mark_gathered(const std::string &fact_set, const std::string &timestamp) {
    boost::unique_lock<boost::mutex> lock(mutex_);
    // Only for a set we actually hold, so this cannot accumulate entries for
    // sets that were rejected or never stored.
    if (sets_.find(fact_set) == sets_.end()) return;
    if (timestamp.empty()) {
      gathered_.erase(fact_set);
      return;
    }
    gathered_[fact_set] = timestamp;
  }

  // The encoded size budget ([/settings/facts] max size). A budget below what
  // a single empty set needs is ignored.
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

 private:
  // The document, assembled from the sets. Callers hold the lock.
  PB::Facts::Object build_document_locked() const {
    PB::Facts::Object document;
    for (const std::pair<const std::string, PB::Facts::Object> &entry : sets_) {
      PB::Facts::Field *field = document.add_fields();
      field->set_key(entry.first);
      *field->mutable_value()->mutable_object_value() = entry.second;
    }
    return document;
  }

  // The digest of `bytes`, as 64 lowercase hex characters; empty in a build
  // without OpenSSL (see the note at the top of the file).
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

  // Walk a fact set and check it against the document rules. `path` is the
  // dotted position in the document, so the rejection message says where the
  // problem is rather than just that there is one.
  static bool validate_object(const std::string &path, const PB::Facts::Object &object, const std::size_t depth, std::string &error) {
    if (depth > max_depth) {
      error = "'" + path + "' nests deeper than " + std::to_string(max_depth) + " levels";
      return false;
    }
    std::set<std::string> keys;
    for (const PB::Facts::Field &field : object.fields()) {
      if (!is_valid_key(field.key())) {
        error = "'" + path + "." + clip(field.key()) + "' is not a valid key (snake_case, at most " + std::to_string(max_key_length) + " characters)";
        return false;
      }
      // A repeated field can carry the same key twice, which an object cannot
      // mean: the document is a map at every level, and the second one would
      // silently win or lose depending on the reader.
      if (!keys.insert(field.key()).second) {
        error = "'" + path + "' carries the key '" + clip(field.key()) + "' twice";
        return false;
      }
      if (!validate(path + "." + field.key(), field.value(), depth + 1, error)) return false;
    }
    return true;
  }

  static bool validate(const std::string &path, const PB::Facts::Value &value, const std::size_t depth, std::string &error) {
    switch (value.kind_case()) {
      case PB::Facts::Value::kObjectValue:
        return validate_object(path, value.object_value(), depth, error);
      case PB::Facts::Value::kListValue:
        return validate_list(path, value.list_value(), depth, error);
      case PB::Facts::Value::KIND_NOT_SET:
        // The protobuf equivalent of a null: a Value that says nothing. An
        // unknown value is omitted from the document, never stored as an
        // empty one.
        error = "'" + path + "' carries no value: an unknown value is omitted, never stored empty";
        return false;
      default:
        // Strings, numbers and booleans are the scalars.
        return true;
    }
  }

  // A list is either a list of records - objects carrying an `id` that is
  // unique in the list and stable across runs, which is what lets the server
  // diff two uploads instead of treating every refresh as a replacement - or
  // a plain list of strings (`addresses`, `modules`). Mixing the two, or
  // listing bare numbers, leaves a consumer with no way to tell what it has.
  static bool validate_list(const std::string &path, const PB::Facts::List &list, const std::size_t depth, std::string &error) {
    if (static_cast<std::size_t>(list.values_size()) > max_list_length) {
      error = "'" + path + "' holds " + std::to_string(list.values_size()) + " entries, more than the " + std::to_string(max_list_length) + " allowed";
      return false;
    }
    if (list.values_size() == 0) return true;
    const bool records = list.values(0).has_object_value();
    std::set<std::string> ids;
    for (const PB::Facts::Value &item : list.values()) {
      if (item.has_object_value() != records) {
        error = "'" + path + "' mixes records and plain values: a list is either records or strings";
        return false;
      }
      if (!records) {
        if (item.kind_case() != PB::Facts::Value::kStringValue) {
          error = "'" + path + "' holds a plain value that is not a string: a list is either records or strings";
          return false;
        }
        continue;
      }
      const PB::Facts::Value *id = nscapi::facts::tree::get(item.object_value(), "id");
      if (id == nullptr || id->kind_case() != PB::Facts::Value::kStringValue || id->string_value().empty()) {
        error = "'" + path + "' holds a record without a non-empty string id";
        return false;
      }
      if (!ids.insert(id->string_value()).second) {
        error = "'" + path + "' holds two records with the id '" + clip(id->string_value()) + "'";
        return false;
      }
      if (!validate_object(path + "[" + id->string_value() + "]", item.object_value(), depth + 1, error)) return false;
    }
    return true;
  }

  // Erase one set. Callers hold the lock and call touch() themselves, so a
  // round that drops several sets counts as one change.
  bool erase_locked(const std::string &fact_set) {
    owners_.erase(fact_set);
    const std::map<std::string, std::string>::iterator encoded = encoded_.find(fact_set);
    if (encoded == encoded_.end()) return false;
    size_ -= encoded->second.size();
    encoded_.erase(encoded);
    sets_.erase(fact_set);
    gathered_.erase(fact_set);
    return true;
  }

  void touch() {
    hash_dirty_ = true;
    ++revision_;
  }

  mutable boost::mutex mutex_;
  // The document, one entry per fact set, each sorted by key. A std::map, so
  // the first level is sorted too and the document has one encoding.
  std::map<std::string, PB::Facts::Object> sets_;
  // The encoded bytes of each set, kept beside it: they are what decides
  // whether a round changed anything, and what the size budget counts.
  std::map<std::string, std::string> encoded_;
  std::size_t size_ = 0;
  // Which plugin produced each set, so unloading a module takes its sets with
  // it and two modules cannot fight over one top-level key.
  std::map<std::string, unsigned int> owners_;
  // What each producer last said it is configured to produce.
  std::map<unsigned int, std::set<std::string>> declared_;
  std::map<std::string, std::string> errors_;
  // When each set's values were read off the machine, as the producer
  // reported it. Kept beside the set rather than in it: `set()` decides
  // "changed" by comparing the encoded document, so a timestamp inside would
  // bump the revision on every round and make an unchanging inventory look
  // like it churns.
  std::map<std::string, std::string> gathered_;
  std::string collected_;
  mutable std::string hash_;
  mutable bool hash_dirty_ = true;
  unsigned long long revision_ = 0;
  std::size_t max_size_ = default_max_size;
};

typedef std::shared_ptr<fact_repository> fact_repository_instance;

}  // namespace core
}  // namespace nsclient
