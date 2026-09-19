// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <ctime>
#include <memory>
#include <nscapi/dll_defines.hpp>
#include <string>
#include <utility>
#include <vector>

// How a module produces facts.
//
// A fact is structured inventory about the host - which volumes exist, what
// the hardware is, which software is installed - as opposed to a tag, which is
// one flat string a fleet selector matches on. The core pulls facts: it calls
// `fetchFacts()` on a schedule with the list of **fact set ids** the operator
// enabled, exactly as it calls `fetchMetrics()`, and the module answers with
// the sets it was asked for and can produce:
//
//   void CheckSystem::fetchFacts(const nscapi::facts::request &req, nscapi::facts::response &out) {
//     if (req.wants("os")) {
//       nscapi::facts::section os = out.set("os");
//       os.value("family", "windows").value("name", name).value("version", version);
//       os.time("boot_time", boot_time);            // ISO 8601 UTC
//       os.value("pending_reboot", pending);
//     }
//     if (req.wants("software.installed")) {
//       nscapi::facts::list installed = out.set("software").list("installed");
//       for (const auto &app : apps)
//         installed.record(app.name).value("version", app.version).date("installed", app.install_date);
//     }
//     if (req.wants("hardware")) {
//       try { gather_hardware(out.set("hardware")); }
//       catch (const std::exception &e) { out.error("hardware", e.what()); }
//     }
//   }
//
// The rules the core enforces when it accepts a set (service/fact_repository.hpp)
// are built into the shape of this builder, so a producer does not have to
// remember them:
//
// - `record(id)` writes the `id` field itself, so a list of records without
//   ids cannot be built. The server diffs lists by `id`; without one, every
//   refresh looks like a full replacement.
// - An empty string is dropped rather than written as `""`: an unknown value
//   is omitted. A key that is not `snake_case` ASCII, on the other hand, goes
//   through unchanged and the core rejects the whole set naming it: quietly
//   rewriting a producer's key would ship an inventory under a name nobody
//   wrote, and every consumer reads these keys.
// - `time()` renders ISO 8601 UTC and `date()` renders `YYYY-MM-DD`, so two
//   producers cannot disagree about how a timestamp looks.
//
// A set the module is not asked for must not be produced: `wants()` is the
// only thing that decides, and the core ignores a set that is not enabled.
// A set the module *was* asked for but could not collect is either left out
// (the core keeps the previous value, so a transient failure does not blank
// the inventory) or reported with `error()`. `remove()` is the third case:
// the data is gone for good - the docker socket disappeared - and the set
// should go with it.
//
// Deliberately free of Boost.JSON. Every module links the plugin API library,
// only some of them link Boost.JSON, and the builder writes one small document
// and parses one small fixed-shape request; the same reasoning keeps
// `core_wrapper::parse_tags_json` hand-written.
namespace nscapi {
namespace facts {

namespace detail {
// One node of the document under construction. Held by the response; the
// handles below point at nodes and stay valid for the response's lifetime.
struct node;
}  // namespace detail

class list;

// A JSON object being filled in: a fact set, a nested object inside one, or
// one record of a list.
class NSCAPI_EXPORT section {
 public:
  explicit section(detail::node *target) : node_(target) {}

  // Scalars. An empty string writes nothing at all.
  section &value(const std::string &key, const std::string &text);
  section &value(const std::string &key, const char *text);
  section &value(const std::string &key, int number);
  section &value(const std::string &key, long long number);
  section &value(const std::string &key, unsigned long long number);
  section &value(const std::string &key, double number);
  section &value(const std::string &key, bool flag);

  // A point in time, as ISO 8601 UTC (`2026-09-01T04:12:09Z`). The overload
  // taking a string passes an already-formatted timestamp through, so a
  // producer whose source hands it text does not have to parse and re-render
  // it; an empty one writes nothing.
  section &time(const std::string &key, std::time_t when);
  section &time(const std::string &key, const std::string &iso8601);

  // A calendar day, as `YYYY-MM-DD`. What an install date is: the hour a
  // package manager recorded is noise the server would diff on.
  section &date(const std::string &key, std::time_t when);
  section &date(const std::string &key, const std::string &yyyy_mm_dd);

  // A nested object, e.g. `hardware.cpu`.
  section sub(const std::string &key);

  // A list of records. Every entry is created through `record(id)`.
  ::nscapi::facts::list list(const std::string &key);

  // A plain list of strings, for the handful of places a record would be
  // ceremony: an interface's addresses, the agent's loaded modules. Empty
  // entries are dropped; an empty list still writes `[]`, which is the
  // difference between "none" and "not collected".
  section &strings(const std::string &key, const std::vector<std::string> &values);

 private:
  detail::node *node_;
};

// A list of records inside a section.
class NSCAPI_EXPORT list {
 public:
  explicit list(detail::node *target) : node_(target) {}

  // Append a record and return it, with its `id` already written. An empty id
  // is a producer bug (the server could not diff the list), so the record is
  // appended with the id left out and the core rejects the set with a message
  // naming it - louder than a list that silently loses an entry.
  section record(const std::string &id);

  std::size_t size() const;

 private:
  detail::node *node_;
};

// What the core is asking for this round.
class NSCAPI_EXPORT request {
 public:
  request() = default;
  // Parse the request the core sent over the ABI:
  //   { "enabled": ["os", "software.installed"], "reason": "scheduled" }
  // A request that does not parse asks for nothing, which is the safe
  // direction: a producer that cannot tell what was enabled publishes nothing.
  static request parse(const std::string &json);

  // Whether this round wants that fact set. The only thing a producer should
  // branch on - a module never reads [/settings/facts] itself.
  bool wants(const std::string &fact_set) const;
  const std::vector<std::string> &enabled() const { return enabled_; }

  // `startup`, `scheduled`, `reload` or `manual`. A producer with an expensive
  // collection can use it to skip work - re-reading the installed-software
  // hives on a `reload` when nothing they depend on moved.
  const std::string &reason() const { return reason_; }
  bool is_manual() const { return reason_ == "manual"; }

  void add_enabled(const std::string &fact_set) { enabled_.push_back(fact_set); }
  void set_reason(const std::string &reason) { reason_ = reason; }

 private:
  std::vector<std::string> enabled_;
  std::string reason_;
};

// What the module produces this round.
class NSCAPI_EXPORT response {
 public:
  response();
  ~response();
  response(const response &) = delete;
  response &operator=(const response &) = delete;

  // The fact set (or the top-level key of a dotted one) to fill in. Calling it
  // twice with the same name returns the same section, so two dotted sets can
  // be produced independently under one key.
  section set(const std::string &name);

  // The data behind this set is gone for good - not "failed this round", which
  // is what leaving the set out means. Writes an explicit null, which the core
  // reads as a removal.
  void remove(const std::string &fact_set);

  // This set could not be collected, and why. The core keeps the previous
  // value, logs the reason once and reports it on /api/v2/facts, so the UI
  // shows "software.installed: access denied" instead of an absence.
  void error(const std::string &fact_set, const std::string &message);

  // The ABI response body: { "sets": { ... }, "errors": { ... } }.
  std::string serialize() const;

 private:
  std::unique_ptr<detail::node> sets_;
  std::vector<std::pair<std::string, std::string> > errors_;
};

// Escape and quote a string as a JSON string literal. Exposed for the tests
// and for the generated glue, which reports a producer's exception as one.
NSCAPI_EXPORT std::string quote(const std::string &text);

// `2026-09-01T04:12:09Z` / `2026-09-01` from a time_t, in UTC. Exposed so a
// producer that has to put a timestamp somewhere other than a section (a
// record's id) renders it the same way.
NSCAPI_EXPORT std::string to_iso8601(std::time_t when);
NSCAPI_EXPORT std::string to_date(std::time_t when);

}  // namespace facts
}  // namespace nscapi
