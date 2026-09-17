// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <NSCAPI.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
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
  virtual bool exec_simple_command(const std::string target, const std::string command, const std::list<std::string> &argument,
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
  // Guards the two maps above and the dispatch bookkeeping below. Held only
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

  // The manager owns raw script_information pointers, each holding a script
  // state (a lua_State for the lua traits). Without this, dropping a manager -
  // which is what a reload does when it builds a replacement - leaked the whole
  // generation: the states, the user data and the registrations.
  ~script_manager() {
    try {
      unload_all();
    } catch (...) {
      // Nothing useful to do from a destructor, and letting it out would
      // terminate: the module is going away either way.
    }
  }
  script_manager(const script_manager &) = delete;
  script_manager &operator=(const script_manager &) = delete;

  script_information<script_trait> *add(std::string alias, std::string script) {
    script_information<script_trait> *info =
        new script_information_impl<script_trait>(this, nscp_runtime->get_settings_provider(), nscp_runtime->get_core_provider());
    info->plugin_alias = plugin_alias;
    info->plugin_id = plugin_id;
    info->script = script;
    info->script_alias = alias;
    {
      // Under the lock, like the insert below: two concurrent
      // `nscp lua execute` calls otherwise handed out the same id and one of
      // the two entries was silently overwritten and leaked. The id has to be
      // final before create_user_data, which reads it, so this cannot simply
      // move into the insert.
      boost::lock_guard<boost::mutex> lock(mutex_);
      info->script_id = script_id++;
    }
    script_runtime->create_user_data(info);
    {
      boost::lock_guard<boost::mutex> lock(mutex_);
      scripts_[info->script_id] = info;
    }
    return info;
  }

  script_information<script_trait> *add_and_load(std::string alias, std::string script) {
    script_information<script_trait> *instance = add(alias, script);
    script_runtime->load(instance);
    return instance;
  }

  void load_all() {
    // TODO: locked
    for (typename script_list_type::value_type &entry : scripts_) {
      script_runtime->load(entry.second);
    }
  }
  void start_all() {
    // TODO: locked
    for (typename script_list_type::value_type &entry : scripts_) {
      script_runtime->start(entry.second);
    }
  }
  void unload_all() {
    script_list_type doomed;
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
      while (dispatchers_.size() != dispatchers_.count(self)) {
        if (!idle_.timed_wait(lock, deadline)) break;
      }
      commands.clear();
      doomed.swap(scripts_);
      unloading_ = false;
    }
    // Outside the lock: unload() runs script code, which can call back in.
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
  bool empty() const {
    boost::lock_guard<boost::mutex> lock(mutex_);
    return scripts_.empty();
  }
};
}  // namespace scripts
