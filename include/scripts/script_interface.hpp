// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <NSCAPI.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace scripts {
struct sample_trait {
  struct user_data {
    std::string foo;
  };
  typedef user_data user_data_type;

  struct function {
    std::string name;
    std::string object;
    void *instance;
  };
  typedef function function_type;
};

struct settings_provider;
struct core_provider;
template <class script_trait>
class regitration_provider;
template <class script_trait>
struct script_information {
  int plugin_id;
  int script_id;
  std::string plugin_alias;
  std::string script_alias;
  std::string script;
  typename script_trait::user_data_type user_data;
  virtual ~script_information() {}
  virtual std::shared_ptr<settings_provider> get_settings_provider() = 0;
  virtual std::shared_ptr<core_provider> get_core_provider() = 0;
  virtual void register_command(const std::string type, const std::string &command, const std::string &description,
                                typename script_trait::function_type function) = 0;
};

struct core_provider {
  virtual bool submit_simple_message(const std::string channel, const std::string command, const NSCAPI::nagiosReturn code, const std::string &message,
                                     const std::string &perf, std::string &response) = 0;
  virtual NSCAPI::nagiosReturn simple_query(const std::string &command, const std::list<std::string> &argument, std::string &msg, std::string &perf) = 0;
  // Same as simple_query but routed to a specific target (sets the request header
  // recipient/destination), so relay modules (NRPE/NSCA/NSCP client targets) pick it up.
  virtual NSCAPI::nagiosReturn simple_query(const std::string &target, const std::string &command, const std::list<std::string> &argument, std::string &msg,
                                            std::string &perf) = 0;
  // Forward a command verbatim to a relay target via the header "<protocol>_forward"
  // command (e.g. "nrpe_forward"). Unlike simple_query(target, ...) the inner payload
  // command + arguments are sent on the wire untouched, so a remote handler actually
  // receives them. Returns the flattened (code, msg, perf) of the relayed response.
  virtual NSCAPI::nagiosReturn query_forward(const std::string &forward_command, const std::string &target, const std::string &command,
                                             const std::list<std::string> &argument, std::string &msg, std::string &perf) = 0;
  virtual NSCAPI::nagiosReturn exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument,
                                                   std::list<std::string> &result) = 0;
  virtual bool exec_command(const std::string target, const std::string &request, std::string &response) = 0;
  virtual bool query(const std::string &request, std::string &response) = 0;
  virtual bool submit(const std::string target, const std::string &request, std::string &response) = 0;
  virtual bool reload(const std::string module) = 0;
  virtual void log(NSCAPI::log_level::level, const std::string file, int line, const std::string message) = 0;
};

struct settings_provider {
  virtual std::list<std::string> get_section(std::string section) = 0;
  virtual std::string get_string(std::string path, std::string key, std::string value) = 0;
  virtual void set_string(std::string path, std::string key, std::string value) = 0;
  virtual bool get_bool(std::string path, std::string key, bool value) = 0;
  virtual void set_bool(std::string path, std::string key, bool value) = 0;
  virtual int get_int(std::string path, std::string key, int value) = 0;
  virtual void set_int(std::string path, std::string key, int value) = 0;
  virtual void save() = 0;

  virtual void register_path(std::string path, std::string title, std::string description, bool advanced) = 0;
  virtual void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defaultValue) = 0;
};

template <class script_trait>
struct script_runtime_interface {
  virtual void load(scripts::script_information<script_trait> *info) = 0;
  virtual void start(scripts::script_information<script_trait> *info) = 0;
  virtual void unload(scripts::script_information<script_trait> *info) = 0;
  virtual void create_user_data(scripts::script_information<script_trait> *info) = 0;
};

struct nscp_runtime_interface {
  virtual void register_command(const std::string type, const std::string &command, const std::string &description) = 0;
  // Takes back what register_command published. A no-op by default, for
  // runtimes whose registrations never outlive the module.
  virtual void unregister_command(const std::string /*type*/, const std::string & /*command*/) {}
  virtual std::shared_ptr<settings_provider> get_settings_provider() = 0;
  virtual std::shared_ptr<core_provider> get_core_provider() = 0;
};

template <class script_trait>
struct command_definition {
  command_definition() {}
  command_definition(script_information<script_trait> *information) : information(information) {}
  command_definition(const command_definition &other) : function(other.function), information(other.information), type(other.type), command(other.command) {}
  command_definition &operator=(const command_definition &other) {
    function = other.function;
    information = other.information;
    type = other.type;
    command = other.command;
    return *this;
  }

  typename script_trait::function_type function;
  script_information<script_trait> *information;
  std::string type;
  std::string command;
};

template <class script_trait>
struct script_manager;

template <class script_trait>
struct script_information_impl : script_information<script_trait> {
  script_manager<script_trait> *reg;
  std::shared_ptr<settings_provider> settings;
  std::shared_ptr<core_provider> core;

  script_information_impl(script_manager<script_trait> *reg, std::shared_ptr<settings_provider> settings, std::shared_ptr<core_provider> core)
      : reg(reg), settings(settings), core(core) {}
  virtual std::shared_ptr<settings_provider> get_settings_provider() { return settings; }
  virtual std::shared_ptr<core_provider> get_core_provider() { return core; }

  void register_command(const std::string type, const std::string &command, const std::string &description, typename script_trait::function_type function) {
    reg->register_command(this, type, command, description, function);
  }
};

template <class script_trait>
struct script_manager {
 private:
  std::shared_ptr<script_runtime_interface<script_trait> > script_runtime;
  std::shared_ptr<nscp_runtime_interface> nscp_runtime;
  int plugin_id;
  int script_id;
  std::string plugin_alias;
  typedef std::map<int, script_information<script_trait> *> script_list_type;
  typedef std::map<std::string, command_definition<script_trait> > command_list_type;
  script_list_type scripts_;
  command_list_type commands;
  // Scripts an unload_all() took out while another thread was still running
  // one of them (see there): not reachable any more, not yet safe to free.
  // The next unload_all() that finds nothing running frees them, as does the
  // destructor.
  script_list_type parked_;
  // Guards the maps above and the dispatch bookkeeping below. Held only
  // for the lookups and mutations themselves, never across a running script.
  mutable boost::mutex mutex_;
  // The threads currently running a script, as a count rather than a reader
  // lock. A script calling back into a command its own module serves arrives
  // here a second time on the same thread, and boost::shared_mutex is
  // writer-preferring: with a writer waiting (another thread registering a
  // function, or `nscp lua execute` adding a script) the inner lock_shared
  // blocked behind it while it waited for the outer one, and neither thread
  // ever moved again. A count has no such ordering, and unload_all can still
  // tell whether the scripts it is about to delete are in use.
  mutable boost::condition_variable idle_;
  mutable std::multiset<boost::thread::id> dispatchers_;
  bool unloading_ = false;

 public:
  // Marks this thread as running a script for as long as it lives. Never
  // blocks; entering is refused once unload_all has started, which callers
  // check through entered().
  class dispatch_guard {
    const script_manager &owner_;
    bool entered_;

   public:
    explicit dispatch_guard(const script_manager &owner) : owner_(owner), entered_(false) {
      boost::lock_guard<boost::mutex> lock(owner_.mutex_);
      const boost::thread::id self = boost::this_thread::get_id();
      // A thread already running a script may re-enter even once an unload has
      // started: it is one of the calls unload_all is waiting for, so nothing
      // it touches can be deleted before it returns.
      if (owner_.unloading_ && owner_.dispatchers_.find(self) == owner_.dispatchers_.end()) return;
      owner_.dispatchers_.insert(self);
      entered_ = true;
    }
    ~dispatch_guard() {
      if (!entered_) return;
      {
        boost::lock_guard<boost::mutex> lock(owner_.mutex_);
        // One entry, not every entry for this thread: a nested dispatch leaves
        // the outer one still running.
        const std::multiset<boost::thread::id>::iterator it = owner_.dispatchers_.find(boost::this_thread::get_id());
        if (it != owner_.dispatchers_.end()) owner_.dispatchers_.erase(it);
      }
      owner_.idle_.notify_all();
    }
    bool entered() const { return entered_; }
    dispatch_guard(const dispatch_guard &) = delete;
    dispatch_guard &operator=(const dispatch_guard &) = delete;
  };

  script_manager(std::shared_ptr<script_runtime_interface<script_trait> > script_runtime_, std::shared_ptr<nscp_runtime_interface> nscp_runtime, int plugin_id,
                 std::string plugin_alias)
      : script_runtime(script_runtime_), nscp_runtime(nscp_runtime), plugin_id(plugin_id), script_id(0), plugin_alias(plugin_alias) {}
  // The scripts are owned raw, each with its interpreter state, so a manager
  // that goes away still holding some would leak every one of them. A module
  // normally calls unload_all() itself, and this is then a no-op.
  ~script_manager() {
    try {
      unload_all();
    } catch (...) {
      // A script's unload hook failing must not escape a destructor.
    }
  }
  script_manager(const script_manager &) = delete;
  script_manager &operator=(const script_manager &) = delete;
  // Whether the calling thread is running one of these scripts. unload_all()
  // reached from such a thread deletes the script - and the interpreter
  // state - that the thread returns into, so a caller about to unload asks
  // this first.
  bool is_dispatching_on_this_thread() const {
    boost::lock_guard<boost::mutex> lock(mutex_);
    return dispatchers_.count(boost::this_thread::get_id()) > 0;
  }
  script_information<script_trait> *add(std::string alias, std::string script) {
    script_information<script_trait> *info =
        new script_information_impl<script_trait>(this, nscp_runtime->get_settings_provider(), nscp_runtime->get_core_provider());
    info->plugin_alias = plugin_alias;
    info->plugin_id = plugin_id;
    info->script = script;
    info->script_alias = alias;
    script_runtime->create_user_data(info);
    {
      // The id is drawn under the same lock as the insert: two concurrent
      // adds (`nscp lua execute` from two callers) could otherwise draw the
      // same id, and the second insert would silently replace - and leak -
      // the first script.
      boost::lock_guard<boost::mutex> lock(mutex_);
      info->script_id = script_id++;
      scripts_[info->script_id] = info;
    }
    return info;
  }

  script_information<script_trait> *add_and_load(std::string alias, std::string script) {
    script_information<script_trait> *instance = add(alias, script);
    script_runtime->load(instance);
    return instance;
  }

  // A copy of scripts_ taken under the lock, for the walks that run script
  // code: add() inserts concurrently, and load() / start() call back in
  // (register_command takes mutex_), so they cannot run with it held.
  script_list_type snapshot_scripts() const {
    boost::lock_guard<boost::mutex> lock(mutex_);
    return scripts_;
  }

  // Called with the script and what went wrong when loading or starting one
  // script fails.
  typedef std::function<void(const std::string &script, const std::string &error)> error_reporter;

  // The snapshot holds raw pointers, so each walk also holds a dispatch_guard:
  // unload_all() waits for it before deleting anything, and a walk that starts
  // once an unload is under way does nothing.
  //
  // Without a reporter the first script that fails stops the walk and the
  // exception reaches the caller. With one, a failure is reported and the
  // remaining scripts are loaded (or started) anyway: one script that does not
  // parse must not take every other script of the module down with it.
  void load_all(const error_reporter &report = error_reporter()) {
    const dispatch_guard guard(*this);
    if (!guard.entered()) return;
    for (typename script_list_type::value_type &entry : snapshot_scripts()) {
      run_reported(report, entry.second, [&] { script_runtime->load(entry.second); });
    }
  }
  void start_all(const error_reporter &report = error_reporter()) {
    const dispatch_guard guard(*this);
    if (!guard.entered()) return;
    for (typename script_list_type::value_type &entry : snapshot_scripts()) {
      run_reported(report, entry.second, [&] { script_runtime->start(entry.second); });
    }
  }
  // With `unregister`, every query and channel the scripts registered is
  // taken back from the core as well. A reload needs that: the generation
  // that follows registers what its scripts still declare, and anything a
  // removed script declared would otherwise stay listed in the core, routed
  // to a command this manager no longer has.
  void unload_all(const bool unregister = false) {
    script_list_type doomed;
    command_list_type dropped;
    bool parked = false;
    {
      boost::unique_lock<boost::mutex> lock(mutex_);
      // Close the door, then wait for the scripts that are running to finish -
      // the command map below points at the very objects we are about to
      // delete. Calls made by this thread are not waited for: unload_all
      // reached from inside a script is further up this stack and can never
      // leave first.
      unloading_ = true;
      const boost::thread::id self = boost::this_thread::get_id();
      const boost::system_time deadline = boost::get_system_time() + boost::posix_time::seconds(5);
      bool still_running = false;
      while (dispatchers_.size() != dispatchers_.count(self)) {
        if (!idle_.timed_wait(lock, deadline)) {
          still_running = dispatchers_.size() != dispatchers_.count(self);
          break;
        }
      }
      dropped.swap(commands);
      unloading_ = false;
      if (still_running) {
        // Another thread is still inside a script - a check that runs long,
        // or load_all()/start_all() on a slow script - and freeing the
        // scripts would pull them out from under it. Park them instead: they
        // are out of scripts_, so nothing new reaches them (register_command
        // ignores them too), and they are freed by the next unload_all() that
        // finds nothing running.
        parked_.insert(scripts_.begin(), scripts_.end());
        scripts_.clear();
        parked = true;
      } else {
        doomed.swap(scripts_);
        doomed.insert(parked_.begin(), parked_.end());
        parked_.clear();
      }
    }
    // Outside the lock: both call into the core, and unload() runs script
    // code, which can call back in.
    if (unregister) {
      for (const typename command_list_type::value_type &entry : dropped) {
        nscp_runtime->unregister_command(entry.second.type, entry.second.command);
      }
    }
    if (parked) return;
    for (typename script_list_type::value_type &entry : doomed) {
      script_information<script_trait> *info = entry.second;
      script_runtime->unload(info);
      delete info;
    }
  }

  void register_command(script_information<script_trait> *information, const std::string type, const std::string &command, const std::string &description,
                        typename script_trait::function_type function) {
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      // A script that is no longer in scripts_ was unloaded (or parked) while
      // still being loaded or started on another thread. Registering for it
      // would refill the command map unload_all() just cleared, pointing at a
      // script about to be freed, and register the command with the core
      // after the unload.
      const typename script_list_type::const_iterator current = scripts_.find(information->script_id);
      if (current == scripts_.end() || current->second != information) return;
      command_definition<script_trait> cmd(information);
      cmd.function = function;
      cmd.command = command;
      cmd.type = type;
      commands[type + "$$" + command] = cmd;
    }
    // Outside the lock: this calls into the core, which can dispatch straight
    // back into this module.
    nscp_runtime->register_command(type, command, description);
  }

  // Returns a copy. Callers that go on to run the command hold a
  // dispatch_guard for the duration, which is what keeps the script alive.
  boost::optional<command_definition<script_trait> > find_command(std::string type, std::string command) const {
    boost::lock_guard<boost::mutex> lock(mutex_);
    typename command_list_type::const_iterator it = commands.find(type + "$$" + command);
    if (it == commands.end()) {
      return boost::optional<command_definition<script_trait> >();
    }
    return boost::optional<command_definition<script_trait> >((*it).second);
  }
  /*
                  virtual NSCAPI::errorReturn execute_command(const std::string &type, const std::string &command, const std::string &request, std::string
     &response) { boost::optional<command_definition<script_trait> > cmd = find_command(type, command); if (!cmd) return NSCAPI::returnIgnored;
                          nscp_runtime->execute(type, command, description);
                  }
  */
  template <class F>
  static void run_reported(const error_reporter &report, const script_information<script_trait> *info, F step) {
    if (!report) return step();
    try {
      step();
    } catch (const std::exception &e) {
      report(info->script, e.what());
    } catch (...) {
      report(info->script, "Unknown exception");
    }
  }

  bool empty() const {
    boost::lock_guard<boost::mutex> lock(mutex_);
    return scripts_.empty();
  }
};
}  // namespace scripts
