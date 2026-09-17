// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <memory>
#include <nscapi/dll_defines.hpp>
#include <string>

#ifdef WIN32
#pragma warning(disable : 4251)
#endif

namespace nscapi {
class command_proxy;
namespace command_helper {

typedef std::shared_ptr<nscapi::command_proxy> command_proxy_ptr;

// Whether a command's interface is still expected to change. This is a type
// of its own rather than a bool because the registration helper is overloaded
// on (command, description) and (command, alias, description): a trailing bool
// would win over the three-string overload for a call like
// ("check_x", "alias", "description"), since a const char* converts to bool.
enum class stability { stable, experimental };

struct command_info {
  std::string name;
  std::string description;
  std::list<std::string> aliases;
  // The command is experimental: it works, but its options, keywords or output
  // may still change. Declared as "experimental": true in module.json and
  // carried to the registry so the CLI, web UI and docs can say so.
  bool experimental;

  command_info(std::string name, std::string description_, stability stability_ = stability::stable)
      : name(name), description(description_), experimental(stability_ == stability::experimental) {}

  command_info(const command_info& obj) : name(obj.name), description(obj.description), aliases(obj.aliases), experimental(obj.experimental) {}
  command_info& operator=(const command_info& obj) {
    name = obj.name;
    description = obj.description;
    aliases = obj.aliases;
    experimental = obj.experimental;
    return *this;
  }

  void add_alias(std::string alias) { aliases.push_back(alias); }
};

class command_registry;
class NSCAPI_EXPORT register_command_helper {
 public:
  register_command_helper(command_registry* owner_) : owner(owner_) {}
  virtual ~register_command_helper() {}

  register_command_helper& operator()(std::string command, std::string description, stability stability_ = stability::stable) {
    add(std::shared_ptr<command_info>(new command_info(command, description, stability_)));
    return *this;
  }

  register_command_helper& operator()(std::string command, std::string alias, std::string description, stability stability_ = stability::stable) {
    std::shared_ptr<command_info> d = std::shared_ptr<command_info>(new command_info(command, description, stability_));
    d->add_alias(alias);
    add(d);
    return *this;
  }

  void add(std::shared_ptr<command_info> d);

 private:
  command_registry* owner;
};

class add_metadata_helper {
 public:
  add_metadata_helper(std::string command, command_registry* owner_) : command(command), owner(owner_) {}
  virtual ~add_metadata_helper() {}

  add_metadata_helper& operator()(std::string key, std::string value) {
    add(key, value);
    return *this;
  }

  void add(std::string key, std::string value);

 private:
  std::string command;
  command_registry* owner;
};

class NSCAPI_EXPORT command_registry {
  typedef std::list<std::shared_ptr<command_info> > command_list;
  command_list commands;
  command_proxy_ptr core_;
  std::list<std::string> errors;

 public:
  command_registry(command_proxy_ptr core) : core_(core) {}
  virtual ~command_registry() {}
  void add(std::shared_ptr<command_info> info) { commands.push_back(info); }
  void set(std::string key, std::string value) {
    // TODO
  }

  register_command_helper command() { return register_command_helper(this); }
  add_metadata_helper add_metadata(std::string command) { return add_metadata_helper(command, this); }

  void register_all();
  void clear() { commands.clear(); }
};
}  // namespace command_helper
}  // namespace nscapi
