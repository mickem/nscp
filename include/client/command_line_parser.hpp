// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/program_options.hpp>
#include <boost/unordered_map.hpp>
#include <cctype>
#include <net/net.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <set>
#include <utility>

namespace client {
struct cli_exception final : std::exception {
  std::string error_;

 public:
  explicit cli_exception(std::string error) : error_(std::move(error)) {}
  ~cli_exception() noexcept override = default;
  const char *what() const noexcept override { return error_.c_str(); }
};

// Keys whose values are credentials (password / token style secrets). Used to
// mask them when a container is logged, and to decide whether a configured
// target "carries credentials" for the host-override guard below.
bool is_sensitive_key(const std::string &key);
// Whether a value carries a credential in its own text, whatever its key is
// called. An address can be `https://h/submit.php?token=SECRET` or
// `https://user:pass@h/`, where the key ("address") says nothing about it.
bool value_carries_credentials(const std::string &value);
// Keys that decide where the request ends up.
bool is_address_key(const std::string &key);
// Keys that decide which way the request travels to get there: an HTTP proxy
// is handed the whole request, credentials included, so a caller choosing one
// moves the credentials as effectively as a caller choosing the host.
bool is_route_key(const std::string &key);

struct destination_container {
  typedef std::map<std::string, std::string> data_map;

  net::url address;
  int timeout;
  int retry;
  data_map data;

  // Where the container was loaded from, when it came from a configured
  // target object rather than being built up purely from a command line: the
  // target's alias, the destination that target named, which of its
  // credentials are still the ones the target configured, and whether it
  // opted in to `allow host override`.
  //
  // A configured target is an address *and* the credentials for that address,
  // as one unit. A request can still name a different --host/--port/--address,
  // but not while carrying credentials it did not supply itself: otherwise
  // anyone able to run the module's submit_*/check_* commands (a REST user
  // with queries.execute, an NRPE peer with `allow arguments`) could point the
  // configured secret at a host of their choosing. A request that supplies its
  // own password/token is free to send it wherever it likes - there is no
  // longer a configured secret in play. See
  // configuration::check_host_override().
  std::string configured_target;
  std::string configured_address;
  std::set<std::string> inherited_credentials;
  bool allow_host_override;
  // Whether the destination currently in `address` was last set by the
  // request rather than by settings. Tracked the same way as
  // inherited_credentials: set_string_data() marks it, and apply() unmarks it
  // straight afterwards for the values that came from a target object.
  bool address_from_request;
  // The route keys (`proxy`, `no proxy`) as the target configured them. A
  // proxy is not part of the address, so the comparison above does not see
  // it, yet `proxy=http://attacker/` hands the request - token and all - to a
  // host of the caller's choosing while the destination stays exactly what
  // the target named. apply() records the configured value here alongside
  // the copy in `data`; a request that sets the key changes only `data`, so
  // the two differing is what "the request changed it" means.
  data_map configured_route;

  destination_container() : timeout(10), retry(2), allow_host_override(false), address_from_request(false) {}

  void apply(const nscapi::settings_objects::object_instance &obj) {
    // Targets layer: --target applies its object on top of the default one
    // without clearing what is already here, so a credential loaded earlier is
    // still in `data` and still counts. The name kept is the target that
    // contributed the credentials, which is the one a refusal should cite.
    if (configured_target.empty()) configured_target = obj->get_alias();
    for (const auto &k : obj->get_options()) {
      if (k.first == "allow host override") {
        allow_host_override = to_permissive_bool(k.second);
        continue;
      }
      set_string_data(k.first, k.second);
      // After set_string_data(), which drops the key from the inherited set:
      // this value came from settings, so it goes (back) in. A value that
      // carries the credential in its own text counts too, however its key is
      // named - an address can be `https://h/submit.php?token=SECRET`.
      if ((is_sensitive_key(k.first) || value_carries_credentials(k.second)) && !k.second.empty()) {
        inherited_credentials.insert(k.first);
        configured_target = obj->get_alias();
      }
      if (is_address_key(k.first)) {
        // The destination this target named. Compared against the address the
        // request ends up with, which is how an override is detected whichever
        // way it arrived - a command-line option or a header host entry.
        //
        // Recorded only for keys the object actually supplies. Taking
        // address.to_string() unconditionally at the end of apply() recorded
        // whatever was already in the container, and --host is parsed before
        // target= is applied: a target that named no address of its own then
        // had the caller's host written down as its "configured" one, and the
        // guard compared that host against itself and let it through.
        address_from_request = false;
        configured_address = address.to_string();
      }
      if (is_route_key(k.first)) configured_route[k.first] = k.second;
    }
  }

  // Whether any credential still in this container is one a configured target
  // supplied, rather than one the request brought with it.
  bool has_inherited_credentials() const { return !inherited_credentials.empty(); }

  // The route keys whose current value is not what the target configured
  // (an unset key on either side reads as empty). A request that repeats the
  // configured proxy changes nothing, like a --host naming the address the
  // target already had.
  std::set<std::string> route_changes() const {
    static const char *const route_keys[] = {"proxy", "no proxy"};
    std::set<std::string> changed;
    for (const char *key : route_keys) {
      if (get_string_data(key) != lookup(configured_route, key)) changed.insert(key);
    }
    return changed;
  }

  void apply(const std::string &key, const PB::Common::Header &header) {
    for (const PB::Common::Host &host : header.hosts()) {
      if (host.id() == key) {
        apply_host(host);
      }
    }
  }
  void apply_host(const PB::Common::Host &host) {
    if (!host.address().empty()) {
      set_string_data("address", host.address());
    }
    for (const PB::Common::KeyValue &kvp : host.metadata()) {
      set_string_data(kvp.key(), kvp.value());
    }
  }

  void set_host(std::string value) { address.host = std::move(value); }
  std::string get_host() const { return address.host; }
  void set_address(const std::string &value) { address = net::parse(value); }
  void set_port(const std::string &value) { address.port = str::stox<unsigned int>(value); }
  std::string get_protocol() const { return address.protocol; }
  bool has_protocol() const { return !address.protocol.empty(); }

  static bool to_bool(const std::string &value, const bool def = false) {
    if (value.empty()) return def;
    if (value == "true" || value == "1" || value == "True") return true;
    return false;
  }

  // to_bool() for a value that decides whether a protection is on. The
  // settings layer normalises a registered bool key to "true"/"false" before
  // it reaches the option map, but a value that arrives another way (a
  // template, or a caller writing the property directly) is raw - and there
  // "yes" silently reading as false would turn the guard *on* where the
  // operator asked for it off, which is confusing rather than dangerous but
  // still wrong. Accepts what settings_value::to_bool accepts.
  static bool to_permissive_bool(const std::string &value, const bool def = false) {
    if (value.empty()) return def;
    std::string v;
    v.reserve(value.size());
    for (const char c : value) v.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    return v == "true" || v == "1" || v == "yes" || v == "y" || v == "on" || v == "enabled";
  }
  static int to_int(const std::string &value, const int def = 0) {
    if (value.empty()) return def;
    try {
      return boost::lexical_cast<int>(value);
    } catch (...) {
      return def;
    }
  }

  int get_int_data(const std::string &key, const int def = 0) { return to_int(data[key], def); }
  bool get_bool_data(const std::string &key, const bool def = false) { return to_bool(data[key], def); }
  static std::string lookup(const data_map &map, const std::string &key, const std::string &def = std::string()) {
    const data_map::const_iterator it = map.find(key);
    if (it == map.end()) return def;
    return it->second;
  }
  std::string get_string_data(const std::string &key, const std::string &def = std::string()) const { return lookup(data, key, def); }
  bool has_data(const std::string &key) const { return data.find(key) != data.end(); }

  void set_string_data(const std::string &key, const std::string &value) {
    // Whatever the source, this value is no longer the configured target's.
    // apply() re-marks the keys it sets straight afterwards, so only a value
    // that came from the request (an option, or a header host entry) clears
    // the mark for good.
    inherited_credentials.erase(key);
    if (is_address_key(key)) address_from_request = true;
    if (key == "host")
      set_host(value);
    else if (key == "address")
      set_address(value);
    else if (key == "port")
      address.port = to_int(value, static_cast<int>(address.port));
    else if (key == "timeout")
      timeout = to_int(value, timeout);
    else if (key == "retry")
      retry = to_int(value, retry);
    else
      data[key] = value;
  }
  void set_int_data(const std::string &key, const int value) { set_string_data(key, str::xtos(value)); }
  void set_bool_data(const std::string &key, const bool value) { set_string_data(key, value ? "true" : "false"); }

  std::string to_string() const;
};

struct command_container {
  std::string command;
  std::string key;
  std::list<std::string> arguments;

  command_container() = default;
  command_container(const command_container &other) = default;
  command_container &operator=(const command_container &other) = default;
};

struct nscp_clp_data {
  std::string target_id;
  std::string command;
  std::string command_line;
  std::string message;
  std::string result;
  std::vector<std::string> arguments;

  destination_container host_self;
  std::list<destination_container> targets;

  explicit nscp_clp_data(const nscapi::targets::target_object &parent) {}
  std::string to_string() const {
    std::stringstream ss;
    ss << "Command: " << command;
    ss << ", target: " << target_id;
    ss << ", self: {" << host_self.to_string() << "}";
    for (const client::destination_container &r : targets) {
      ss << ", target: {" << r.to_string() << "}";
    }
    ss << ", message: " << message;
    ss << ", result: " << result;
    int i = 0;
    for (const std::string &a : arguments) {
      ss << ", argument[" << i++ << "]: " << a;
    }
    return ss.str();
  }
};

// Which of the two containers a host-naming option writes into.
//
// A client call carries two of them: the destination it connects to and the
// source it reports on behalf of. Both are destination_containers, and
// set_string_data() routes the well-known "host" key into the typed address
// field of whichever one it is handed - so a host option bound to the wrong
// container does not fail, it silently rewrites the other one's address.
// Saying which role the options play makes that binding a decision rather
// than something to get right by accident.
enum class host_role {
  target,  // the server being connected to: --host / --port / --address
  source,  // the agent the result speaks for: --source-host / --sender-host
};

// Register the host-naming options for `role` against the container that role
// belongs to.
//
// add_common_options() already registers both roles for every client module,
// so a module's own options_reader should not register them a second time:
// program_options treats a repeated long name as ambiguous and refuses to
// parse it, which makes the option unusable rather than merely duplicated.
void add_host_options(boost::program_options::options_description &desc, destination_container &container, host_role role);

struct options_reader_interface : nscapi::settings_objects::object_factory_interface<nscapi::settings_objects::object_instance_interface> {
  virtual void process(boost::program_options::options_description &desc, destination_container &source, destination_container &destination) = 0;
  void add_ssl_options(boost::program_options::options_description &desc, client::destination_container &data);
};
typedef std::shared_ptr<options_reader_interface> options_reader_type;

struct handler_interface {
  virtual ~handler_interface() = default;
  virtual bool query(client::destination_container sender, client::destination_container target, const PB::Commands::QueryRequestMessage &request_message,
                     PB::Commands::QueryResponseMessage &response_message) = 0;
  virtual bool submit(client::destination_container sender, client::destination_container target, const PB::Commands::SubmitRequestMessage &request_message,
                      PB::Commands::SubmitResponseMessage &response_message) = 0;
  virtual bool exec(client::destination_container sender, client::destination_container target, const PB::Commands::ExecuteRequestMessage &request_message,
                    PB::Commands::ExecuteResponseMessage &response_message) = 0;
  virtual bool metrics(client::destination_container sender, client::destination_container target, const PB::Metrics::MetricsMessage &request_message) = 0;
};
typedef std::shared_ptr<handler_interface> handler_type;

struct configuration : public boost::noncopyable {
  typedef boost::unordered_map<std::string, command_container> command_type;

  typedef nscapi::settings_objects::object_handler<nscapi::settings_objects::object_instance_interface, options_reader_interface> object_handler_type;
  handler_type handler;
  options_reader_type reader;
  // The live tables, read by the query, submission and metrics threads.
  object_handler_type targets;

  std::string title;
  std::string default_command;
  std::string default_sender;
  command_type commands;

  configuration(const std::string &caption, handler_type handler, const options_reader_type &reader)
      : handler(std::move(handler)), reader(reader), targets(reader), staging_targets(reader) {}

  std::string to_string() {
    std::stringstream ss;
    ss << "Title: " << title;
    ss << ", targets: " + targets.to_string();
    return ss.str();
  }

  // Starts a rebuild as well as setting the path: every client module calls
  // this once at the top of loadModuleEx, before walking the targets.
  //
  // The walk fills the staging tables, and finalize() publishes them. Filling
  // the live ones instead meant a reload was either lossy or racy, depending
  // on the module: the three that clear() first emptied the target table and
  // then repopulated it key by key, so a request in that window got an empty
  // destination ("connect to : failed"), while the ones that do not clear
  // never re-read an existing target at all, so a changed address, port or
  // password stayed stale until the agent was restarted.
  void set_path(const std::string &path) {
    boost::unique_lock<boost::shared_mutex> lock(tables_mutex_);
    targets.set_path(path);
    staging_targets.set_path(path);
    staging_targets.clear();
    staging_commands.clear();
  }

  // Written at the end of loadModuleEx - which the core also runs with
  // reloadStart on the live module - while channel, scheduler and metrics
  // threads are inside do_submit / do_metrics reading it. Same table, same
  // lock as `commands` and `targets`.
  void set_sender(const std::string &_sender) {
    boost::unique_lock<boost::shared_mutex> lock(tables_mutex_);
    default_sender = _sender;
  }

  destination_container get_target(const std::string &name) const;
  destination_container get_sender() const;

  void add_target(const std::shared_ptr<nscapi::settings_proxy> &proxy, const std::string &key, const std::string &value) {
    // Into the generation being built. The lock is held across object->read()
    // - a settings query - but only workers looking at the *live* tables would
    // care, and they do not take it for this.
    boost::unique_lock<boost::shared_mutex> lock(tables_mutex_);
    staging_targets.add(proxy, key, value);
  }
  std::string add_command(const std::string &name, const std::string &args);

  // Swap the generation built since set_path() into the live tables.
  //
  // finalize() calls this once it has added the samples and the default
  // target; it is public so a test can publish without a settings store.
  void publish() {
    boost::unique_lock<boost::shared_mutex> lock(tables_mutex_);
    targets.swap_objects(staging_targets);
    commands.swap(staging_commands);
    staging_targets.clear();
    staging_commands.clear();
  }
  // Drops both generations. Only unloadModule wants this; a reload goes
  // through set_path() + finalize() instead.
  void clear() {
    boost::unique_lock<boost::shared_mutex> lock(tables_mutex_);
    targets.clear();
    commands.clear();
    staging_targets.clear();
    staging_commands.clear();
  }
  void finalize(const std::shared_ptr<nscapi::settings_proxy> &settings);
  // Locked lookups: a settings reload rewrites `commands` and `targets` on
  // the module thread while queries, submissions and the metrics task read
  // them from workers.
  boost::optional<command_container> find_command(const std::string &command) const {
    boost::shared_lock<boost::shared_mutex> lock(tables_mutex_);
    const command_type::const_iterator cit = commands.find(command);
    if (cit == commands.end()) return boost::none;
    return cit->second;
  }
  object_handler_type::object_instance find_target(const std::string &name) const {
    boost::shared_lock<boost::shared_mutex> lock(tables_mutex_);
    return targets.find_object(name);
  }

  void do_query(const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response);
  bool do_exec(const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response, const std::string &default_command_arg);
  void do_submit(const PB::Commands::SubmitRequestMessage &request, PB::Commands::SubmitResponseMessage &response);

  void do_submit_item(const PB::Commands::SubmitRequestMessage &request, const destination_container &s, destination_container d,
                      PB::Commands::SubmitResponseMessage &response);

  void do_metrics(const PB::Metrics::MetricsMessage &request) const;

  typedef boost::function<boost::program_options::options_description(client::destination_container &source, client::destination_container &destination)>
      client_desc_fun;
  typedef boost::function<bool(client::destination_container &source, client::destination_container &destination)> client_pre_fun;
  client_desc_fun client_desc;
  client_pre_fun client_pre;

 private:
  // The generation loadModuleEx is building. Nothing outside loadModuleEx
  // reads these; finalize() swaps them into the live tables above in one step,
  // so a request sees either the whole old configuration or the whole new one.
  object_handler_type staging_targets;
  command_type staging_commands;

  mutable boost::shared_mutex tables_mutex_;
  boost::program_options::options_description create_descriptor(const std::string &command, client::destination_container &source,
                                                                client::destination_container &destination) const;
  // The host-override guard: given the options the command line actually
  // supplied and the destination as it stands after they were applied, either
  // an empty string (proceed) or the reason the call must be refused.
  static std::string check_host_override(const boost::program_options::variables_map &vm, const destination_container &d);
  void i_do_query(destination_container &s, destination_container &d, std::string command, const PB::Commands::QueryRequestMessage &request,
                  PB::Commands::QueryResponseMessage &response, bool use_header);
  bool i_do_exec(destination_container &s, destination_container &d, std::string command, const PB::Commands::ExecuteRequestMessage &request,
                 PB::Commands::ExecuteResponseMessage &response, bool use_header);
  void i_do_submit(const destination_container &s, destination_container &d, std::string command, const PB::Commands::SubmitRequestMessage &request,
                   PB::Commands::SubmitResponseMessage &response, bool use_header);
};
}  // namespace client