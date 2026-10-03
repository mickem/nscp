// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <exception>
#include <memory>
#include <nsclient/logger/logger.hpp>
#include <set>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <threads/in_flight.hpp>
#include <utility>
#include <vector>

#include "plugin_interface.hpp"

namespace nsclient {
typedef std::shared_ptr<core::plugin_interface> plugin_type;
typedef std::map<unsigned long, plugin_type> plugin_list_type;
typedef std::set<unsigned long> plugin_id_type;

class plugins_list_exception : public std::exception {
  std::shared_ptr<std::string> what_;

 public:
  explicit plugins_list_exception(std::string error) : what_(std::make_shared<std::string>(error)) {}
  const char *what() const noexcept override { return what_ ? what_->c_str() : ""; }
};

// The modules a metrics or facts round walks. Each sits behind its own gate
// (threads::gated): a round enters a module's gate only around the call into
// that module, so a removal waits for the rounds inside *that* module and
// for nothing else - a round stuck in another module's fetchMetrics, or
// blocked on the lifecycle lock from inside one, does not hold it up. And the
// gate, not the round, owns the module, so the remover is its last holder:
// a round never drops the last reference and runs a module's destructor -
// and dlclose - on the walking thread.
struct simple_plugins_list : boost::noncopyable {
  typedef threads::gated<plugin_type> gate_type;
  struct slot {
    unsigned long id;
    std::shared_ptr<gate_type> gate;
  };
  std::vector<slot> plugins_;
  boost::shared_mutex mutex_;
  logging::log_client_accessor logger_;

  explicit simple_plugins_list(logging::log_client_accessor logger) : logger_(std::move(logger)) {}

  // A module whose gate has been closed: rounds skip it from now on, and it
  // keeps its place in the list until finish() takes it out - so a refused
  // unload reopens it where it was, in the order the rounds walk.
  class closing {
    simple_plugins_list *list_;
    std::shared_ptr<gate_type> gate_;
    std::uint64_t cutoff_;

   public:
    closing() : list_(nullptr), cutoff_(0) {}
    closing(simple_plugins_list *list, std::shared_ptr<gate_type> gate, const std::uint64_t cutoff) : list_(list), gate_(std::move(gate)), cutoff_(cutoff) {}
    explicit operator bool() const { return gate_ != nullptr; }
    // The module, for a caller that has no other handle on it. Present
    // until finish() takes it.
    plugin_type plugin() const { return gate_ ? gate_->value() : plugin_type(); }
    // Wait for the rounds inside the module when it was closed.
    bool drain(const std::chrono::milliseconds timeout) const { return !gate_ || gate_->tracker().wait_for_others_before(cutoff_, timeout); }
    // Let rounds call the module again, in its old place.
    void reopen() const {
      if (gate_) gate_->tracker().reopen();
    }
    // Done with the module. Drained and taken out of the list: handed back
    // so the caller is its last holder. Not drained - or not taken out,
    // because the list's lock could not be had - it stays in the list,
    // closed and still holding the module, so remove_all() waits for it
    // again at shutdown and reports it. The gate is never left empty while
    // its slot is still listed.
    plugin_type finish(const bool drained) const {
      if (!gate_ || !drained) return plugin_type();
      if (!list_->erase_slot(gate_)) return plugin_type();
      return gate_->take();
    }
  };

  // Drain several closings within one deadline, rather than one bound each,
  // and report each one's own outcome: a round stuck in one list says
  // nothing about the others.
  static std::vector<bool> drain_each(const std::vector<closing> &closings, const std::chrono::milliseconds timeout) {
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<bool> drained;
    drained.reserve(closings.size());
    for (const closing &c : closings) {
      const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      const std::chrono::milliseconds remaining =
          now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now) : std::chrono::milliseconds(0);
      drained.push_back(c.drain(remaining));
    }
    return drained;
  }

  bool has_valid_lock_log(const boost::unique_lock<boost::shared_mutex> &lock, const std::string &key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      return false;
    }
    return true;
  }
  bool has_valid_lock_log(const boost::shared_lock<boost::shared_mutex> &lock, const std::string &key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      return false;
    }
    return true;
  }

  void add_plugin(const plugin_type &plugin) {
    const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
    if (!has_valid_lock_log(writeLock, "plugins_list::add_plugin")) return;
    for (const slot &s : plugins_) {
      if (s.id == plugin->get_id()) {
        log_error(__FILE__, __LINE__, "Duplicate plugin id");
        return;
      }
    }
    plugins_.push_back(slot{plugin->get_id(), std::make_shared<gate_type>(plugin)});
  }

  // Close the module's gate, if it is in this list. An empty closing when it
  // is not, or when the lock could not be had - nothing to wait for or undo.
  closing close_plugin(const unsigned long id) {
    const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!has_valid_lock_log(writeLock, "plugins_list::close_plugin" + str::xtos(id))) return closing();
    for (const slot &s : plugins_) {
      if (s.id == id) return closing(this, s.gate, s.gate->tracker().close());
    }
    return closing();
  }

  // Close and drain every module at once, within one deadline, and empty the
  // list. Returns the modules a round is still inside, for the caller to
  // leave alone.
  std::vector<plugin_type> remove_all(const std::chrono::milliseconds timeout = std::chrono::seconds(30)) {
    std::vector<std::pair<std::shared_ptr<gate_type>, std::uint64_t> > closed;
    {
      const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
      if (!has_valid_lock_log(writeLock, "plugins_list::remove_all")) return std::vector<plugin_type>();
      for (const slot &s : plugins_) closed.emplace_back(s.gate, s.gate->tracker().close());
      plugins_.clear();
    }
    std::vector<plugin_type> still_walked;
    std::vector<plugin_type> released;
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout;
    for (const auto &c : closed) {
      const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
      const std::chrono::milliseconds remaining =
          now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now) : std::chrono::milliseconds(0);
      if (c.first->tracker().wait_for_others_before(c.second, remaining)) {
        released.push_back(c.first->take());
      } else {
        still_walked.push_back(c.first->value());
      }
    }
    return still_walked;
  }

  // Whether anything is registered here. A failed lock reads as empty: the
  // caller is deciding whether a feature is worth scheduling, and answering
  // "nothing registered" is the harmless way to be wrong.
  bool empty() {
    const boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!has_valid_lock_log(readLock, "plugins_list::empty")) return true;
    return plugins_.empty();
  }

  // Call `fun` on every registered module. The list is copied under the lock
  // and the calls are made outside it, as master_plugin_list::get_plugins
  // does: `fun` runs module code (fetchMetrics, submitMetrics, fetchFacts),
  // and a module that loads or unloads another from there re-enters
  // add_plugin / close_plugin, which want this mutex exclusively. The copy
  // is of the slots, not the modules: each call goes through the module's
  // gate, which covers the whole of `fun` - for facts that includes storing
  // what the module returned, which the manager's remove_owned_by must
  // follow, or the sets come back owned by a dead id.
  void do_all(const boost::function<void(plugin_type)> &fun) {
    // A vector, as master_plugin_list::get_plugins returns: one allocation
    // per round rather than a node per module.
    std::vector<slot> snapshot;
    {
      const boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
      if (!has_valid_lock_log(readLock, "plugins_list::list")) return;
      snapshot = plugins_;
    }
    for (const slot &s : snapshot) {
      // The guard is declared before the copy, so the copy goes first
      // however this ends: the gate still holds the module, so the copy is
      // never the last reference, and only then does a waiting remover wake.
      threads::in_flight::guard inside(s.gate->tracker());
      if (!inside.try_enter()) continue;  // closed for removal since the list was read
      plugin_type plugin = s.gate->value();
      fun(plugin);
      plugin.reset();
      inside.leave();
    }
  }

  std::string to_string() {
    std::string ret;
    const boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!has_valid_lock_log(readLock, "plugins_list::list")) return "";
    for (const slot &s : plugins_) {
      const plugin_type &p = s.gate->value();
      if (!p) continue;
      if (!ret.empty()) ret += ", ";
      ret += p->getName();
    }
    return ret;
  }

  void log_error(const char *file, int line, std::string error) { logger_->error("plugin", file, line, error); }
  void log_error(const char *file, int line, std::string error, std::string key) {
    logger_->error("plugin", file, line, error + " for " + utf8::cvt<std::string>(key));
  }

 private:
  // Whether the slot was found and taken out.
  bool erase_slot(const std::shared_ptr<gate_type> &gate) {
    const boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
    if (!has_valid_lock_log(writeLock, "plugins_list::erase")) return false;
    const std::vector<slot>::iterator end = std::remove_if(plugins_.begin(), plugins_.end(), [&gate](const slot &s) { return s.gate == gate; });
    const bool erased = end != plugins_.end();
    plugins_.erase(end, plugins_.end());
    return erased;
  }
};

template <class parent>
struct plugins_list : boost::noncopyable, public parent {
  plugin_list_type plugins_;
  boost::shared_mutex mutex_;
  logging::log_client_accessor logger_;

  plugins_list(logging::log_client_accessor logger) : parent(), logger_(logger) {}

  bool has_valid_lock_log(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      return false;
    }
    return true;
  }
  bool has_valid_lock_log(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      return false;
    }
    return true;
  }
  void has_valid_lock_throw(boost::unique_lock<boost::shared_mutex> &lock, std::string key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
    }
  }
  void has_valid_lock_throw(boost::shared_lock<boost::shared_mutex> &lock, std::string key) {
    if (!lock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex", key);
      throw plugins_list_exception("Failed to get mutex: " + utf8::cvt<std::string>(key));
    }
  }

  void add_plugin(plugin_type plugin) {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
    if (!has_valid_lock_log(writeLock, "plugins_list::add_plugin")) return;
    plugins_[plugin->get_id()] = plugin;
    parent::add_plugin(plugin);
  }

  void remove_all() {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(30));
    if (!has_valid_lock_log(writeLock, "plugins_list::remove_all")) return;
    plugins_.clear();
    parent::remove_all();
  }

  void remove_plugin(unsigned long id) {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!has_valid_lock_log(writeLock, "plugins_list::remove_plugin" + str::xtos(id))) return;
    auto pit = plugins_.find(id);
    if (pit != plugins_.end()) plugins_.erase(pit);
    parent::remove_plugin(id);
  }

  std::list<std::string> list() {
    std::list<std::string> lst;
    boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!has_valid_lock_log(readLock, "plugins_list::list")) return lst;
    parent::list(lst);
    return lst;
  }

  std::string to_string() {
    std::string ret;
    const std::list<std::string> lst = list();
    if (lst.empty()) {
      return "NONE" + parent::to_string();
    }
    for (const std::string &str : lst) {
      if (!ret.empty()) ret += ", ";
      ret += str;
    }
    return ret + parent::to_string();
  }

  std::string make_key(const std::string &key) { return boost::algorithm::to_lower_copy(key); }
  void log_error(const char *file, int line, std::string error) { logger_->error("plugin", file, line, error); }
  void log_error(const char *file, int line, std::string error, std::string key) {
    logger_->error("plugin", file, line, error + " for " + utf8::cvt<std::string>(key));
  }
  bool have_plugin(unsigned long plugin_id) { return !(plugins_.find(plugin_id) == plugins_.end()); }
};

struct plugins_list_listeners_impl {
  typedef std::map<std::string, plugin_id_type> listener_list_type;
  listener_list_type listeners_;

  void add_plugin(const plugin_type &plugin) {}

  void remove_all() { listeners_.clear(); }

  // Unsubscribe one plugin from every channel it registered for. Only that
  // plugin's id is removed; a channel entry is dropped only once it has no
  // subscribers left. Erasing the whole entry (which this used to do as soon
  // as any one subscriber matched) silently unsubscribed every *other* plugin
  // from that channel, and left this plugin's id behind in the channels it
  // did not erase.
  void remove_plugin(const unsigned long id) {
    auto it = listeners_.begin();
    while (it != listeners_.end()) {
      it->second.erase(id);
      if (it->second.empty())
        it = listeners_.erase(it);
      else
        ++it;
    }
  }

  void list(std::list<std::string> &lst) const {
    for (const listener_list_type::value_type &i : listeners_) {
      lst.push_back(i.first);
    }
  }
  std::string to_string() const {
    std::string ret;
    if (listeners_.empty()) {
      return ", NONE";
    }
    for (const listener_list_type::value_type &i : listeners_) {
      ret += ", ";
      ret += i.first;
    }
    return ret;
  }
};

struct plugins_list_with_listener : plugins_list<plugins_list_listeners_impl> {
  typedef plugins_list parent_type;

  explicit plugins_list_with_listener(const logging::log_client_accessor &logger) : parent_type(logger) {}

  void register_listener(unsigned long plugin_id, const std::string &channel) {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex: ", channel);
      return;
    }
    const std::string lower_case = make_key(channel);
    if (!have_plugin(plugin_id)) {
      writeLock.unlock();
      throw plugins_list_exception("Failed to find plugin: " + str::xtos(plugin_id) + ", Plugins: " + to_string());
    }
    for (const std::string &c : str::utils::split_lst(lower_case, ",")) {
      listeners_[c].insert(plugin_id);
    }
  }

  // The reverse of register_listener: drop this plugin from each channel in
  // the (comma separated) list, and a channel entry once nobody is left on it.
  // Other subscribers of the same channel keep their subscription.
  void unregister_listener(unsigned long plugin_id, const std::string &channel) {
    boost::unique_lock<boost::shared_mutex> writeLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      log_error(__FILE__, __LINE__, "Failed to get mutex: ", channel);
      return;
    }
    for (const std::string &c : str::utils::split_lst(make_key(channel), ",")) {
      const auto it = listeners_.find(c);
      if (it == listeners_.end()) continue;
      it->second.erase(plugin_id);
      if (it->second.empty()) listeners_.erase(it);
    }
  }

  // Collect the subscribers for `channel`. Runs under a SHARED lock, so
  // nothing in here may modify plugins_ - see append_subscribers.
  std::list<plugin_type> get(const std::string &channel) {
    boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    has_valid_lock_throw(readLock, "plugins_list::get:" + channel);
    std::list<plugin_type> ret;
    std::set<unsigned long> seen;
    const std::string lower_case = make_key(channel);
    const auto cit = listeners_.find(lower_case);
    if (cit != listeners_.end()) append_subscribers(cit->second, seen, ret);
    // Wildcard "*" subscribers receive every channel. Used by sinks that
    // want to capture everything (e.g. the WEB server's event drain) without
    // the operator having to enumerate every emitted event name.
    const auto wit = listeners_.find("*");
    if (wit != listeners_.end()) append_subscribers(wit->second, seen, ret);
    return ret;
  }

  listener_list_type get_listeners() {
    boost::shared_lock<boost::shared_mutex> readLock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    has_valid_lock_throw(readLock, "plugins_list::get_listeners");
    return listeners_;
  }

 private:
  // Resolve subscriber ids to plugins, skipping any that are no longer loaded.
  //
  // This used to be `ret.push_back(plugins_[id])`. std::map::operator[]
  // default-INSERTS when the key is absent, so that was a write to plugins_
  // performed while holding only a shared lock - a data race against every
  // concurrent reader of the same mutex, on a red-black tree. It also pushed
  // the default-constructed null plugin_type into the result for callers to
  // dereference. find() cannot insert and lets a missing id be skipped.
  //
  // Callers must hold the lock (shared is enough).
  void append_subscribers(const plugin_id_type &ids, std::set<unsigned long> &seen, std::list<plugin_type> &ret) const {
    for (const unsigned long id : ids) {
      const auto pit = plugins_.find(id);
      if (pit == plugins_.end() || !pit->second) continue;
      if (seen.insert(id).second) ret.push_back(pit->second);
    }
  }
};
}  // namespace nsclient