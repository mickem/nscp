// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// A stub core for unit-testing module classes.
//
// A module class (nscapi::impl::simple_plugin) is not testable without a core:
// loadModuleEx() registers and reads its settings through
// nscapi::settings_proxy and registers its commands through
// nscapi::core_helper, and both of those serialise a protobuf message into a
// core entry point. None of them needs a *real* core though - answering the
// settings queries out of a map, and acknowledging everything else, is enough
// to drive the whole of loadModuleEx and every dispatch method in front of a
// check.
//
// Typical use, in a test binary that has defined nscapi::plugin_singleton:
//
//   class MyModule : public ::testing::Test {
//    protected:
//     void SetUp() override { core().reset(); module_.set_id(42); }
//     nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }
//     MyModule module_;
//   };
//
//   TEST_F(MyModule, ClampsTheInterval) {
//     core().set_setting("interval", "0s");
//     ASSERT_TRUE(module_.loadModuleEx("alias", NSCAPI::dontStart));
//     ...
//   }
//
// Always load with NSCAPI::dontStart: every module that starts a thread, a
// socket or an interpreter guards it on `mode == NSCAPI::normalStart`, so
// dontStart parses all the settings without any of that happening. (LUAScript
// is the one module that does not guard, so it cannot use this yet.)
//
// The core entry points are plain C function pointers with no user data, so
// the stub state is necessarily process-wide; reset() between tests.

#include <NSCAPI.h>

#include <cstring>
#include <map>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <nscapi/protobuf/settings.hpp>
#include <string>
#include <utility>
#include <vector>

namespace nscapi {
namespace test_helpers {

// One settings key as the module registered it. Recorded so a test can assert
// on the key/default matrix a module publishes - a renamed key or a changed
// default silently breaks every existing configuration, and nothing else in
// the build notices.
struct registered_key {
  std::string path;
  std::string key;
  std::string type;
  std::string default_value;
  std::string title;
  std::string description;
  bool advanced = false;
  bool sample = false;
  bool sensitive = false;
};

class stub_core {
 public:
  // The stub wired into the core that nscapi::plugin_singleton hands out -
  // which is what simple_plugin::get_core() and the NSC_LOG_* macros resolve
  // to. Loading the endpoints happens once, on the first call.
  static stub_core &instance() {
    static stub_core core;
    return core;
  }

  // Forget everything recorded and configured. Call from the fixture's SetUp:
  // the state is process-wide, so a leftover value would leak into the next
  // test.
  void reset() {
    settings_.clear();
    version_.clear();
    keys_.clear();
    sections_.clear();
    registered_keys_.clear();
    registered_paths_.clear();
    registrations_.clear();
    updated_settings_.clear();
  }

  // ----- configuring what the module reads ---------------------------------

  // Value for a settings key, matched on the key name alone. Which alias path
  // a module registers under is rarely what a test is about, and matching on
  // it only makes the test brittle against a rename of the section.
  void set_setting(const std::string &key, const std::string &value) { settings_[key] = value; }
  // Same, but bound to one path - for the modules that read the same key name
  // from more than one section.
  void set_setting(const std::string &path, const std::string &key, const std::string &value) { settings_[path + "\x1f" + key] = value; }

  // Keys present under a path, with their values. This is what a settings
  // *path* handler (sh::fun_values_path, string_map_path - how every client
  // module reads its `targets` and `handlers` sections) enumerates, so it is
  // what makes add_target()/add_command() run at all.
  //
  // `path` may be the tail of the real path ("targets" matches
  // "/settings/graphite/client/targets"), so a test does not have to know how
  // the module spells its settings root.
  void set_keys(const std::string &path, const std::vector<std::pair<std::string, std::string> > &keys) {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < keys.size(); ++i) {
      names.push_back(keys[i].first);
      set_setting(path, keys[i].first, keys[i].second);
    }
    keys_[path] = names;
  }
  // Subsections under a path. A path handler stores these with an empty value
  // (the section itself carries the configuration).
  void set_sections(const std::string &path, const std::vector<std::string> &sections) { sections_[path] = sections; }

  // What the core reports as the running version. Left unset the query fails,
  // which is what a module sees from a core that cannot answer.
  void set_application_version(const std::string &version) { version_ = version; }

  // ----- what the module registered ----------------------------------------

  const std::vector<registered_key> &registered_keys() const { return registered_keys_; }
  const std::vector<std::string> &registered_paths() const { return registered_paths_; }
  // Queries and query aliases the module registered, in order.
  std::vector<std::string> registered_commands() const { return names_of(PB::Registry::ItemType::QUERY, PB::Registry::ItemType::QUERY_ALIAS); }
  // Channels (submission handlers) the module registered.
  std::vector<std::string> registered_channels() const { return names_of(PB::Registry::ItemType::HANDLER, PB::Registry::ItemType::HANDLER); }
  // Event subscriptions the module registered.
  std::vector<std::string> registered_events() const { return names_of(PB::Registry::ItemType::EVENT, PB::Registry::ItemType::EVENT); }

  bool has_key(const std::string &path, const std::string &key) const { return find_key(path, key) != nullptr; }
  bool has_command(const std::string &name) const { return contains(registered_commands(), name); }
  bool has_channel(const std::string &name) const { return contains(registered_channels(), name); }
  bool has_event(const std::string &name) const { return contains(registered_events(), name); }

  // Settings the module wrote back, in order - what `nscp <module> install`
  // and friends persist to nsclient.ini.
  const std::vector<registered_key> &updated_settings() const { return updated_settings_; }
  // The value written for a key, or "" if it was never written.
  std::string updated_value(const std::string &key) const {
    for (const registered_key &k : updated_settings_) {
      if (k.key == key) return k.default_value;
    }
    return std::string();
  }
  bool was_updated(const std::string &key) const {
    for (const registered_key &k : updated_settings_) {
      if (k.key == key) return true;
    }
    return false;
  }

  // Whether a key was registered as holding a credential. The core redacts
  // those in the REST settings API, so the flag is security-relevant and
  // worth pinning in a test.
  bool is_sensitive(const std::string &key) const {
    for (const registered_key &k : registered_keys_) {
      if (k.key == key) return k.sensitive;
    }
    return false;
  }
  // The default a key was registered with, or "" if it was never registered.
  std::string default_for(const std::string &path, const std::string &key) const {
    const registered_key *k = find_key(path, key);
    return k ? k->default_value : std::string();
  }
  // Convenience for the common case of a single settings section: first key
  // with this name, whatever the path.
  std::string default_for(const std::string &key) const {
    for (const registered_key &k : registered_keys_) {
      if (k.key == key) return k.default_value;
    }
    return std::string();
  }

 private:
  stub_core() { nscapi::plugin_singleton->get_core()->load_endpoints(&load_endpoint); }

  static bool contains(const std::vector<std::string> &haystack, const std::string &needle) {
    for (std::size_t i = 0; i < haystack.size(); ++i) {
      if (haystack[i] == needle) return true;
    }
    return false;
  }

  std::vector<std::string> names_of(const int type_a, const int type_b) const {
    std::vector<std::string> ret;
    for (std::size_t i = 0; i < registrations_.size(); ++i) {
      if (registrations_[i].first == type_a || registrations_[i].first == type_b) ret.push_back(registrations_[i].second);
    }
    return ret;
  }

  const registered_key *find_key(const std::string &path, const std::string &key) const {
    for (const registered_key &k : registered_keys_) {
      if (k.path == path && k.key == key) return &k;
    }
    return nullptr;
  }

  std::string lookup(const std::string &path, const std::string &key, const std::string &def) const {
    std::map<std::string, std::string>::const_iterator it = settings_.find(path + "\x1f" + key);
    if (it != settings_.end()) return it->second;
    for (it = settings_.begin(); it != settings_.end(); ++it) {
      const std::size_t sep = it->first.find('\x1f');
      if (sep == std::string::npos) continue;
      if (it->first.compare(sep + 1, std::string::npos, key) == 0 && is_path_suffix(path, it->first.substr(0, sep))) return it->second;
    }
    it = settings_.find(key);
    if (it != settings_.end()) return it->second;
    return def;
  }

  // "targets" and "/settings/x/targets" both match "/settings/x/targets";
  // lets a test name a section without spelling out the module's whole root.
  static bool is_path_suffix(const std::string &path, const std::string &pattern) {
    if (path == pattern) return true;
    if (pattern.size() >= path.size()) return false;
    return path.compare(path.size() - pattern.size(), pattern.size(), pattern) == 0 && path[path.size() - pattern.size() - 1] == '/';
  }

  const std::vector<std::string> *find_list(const std::map<std::string, std::vector<std::string> > &from, const std::string &path) const {
    std::map<std::string, std::vector<std::string> >::const_iterator it = from.find(path);
    if (it != from.end()) return &it->second;
    for (it = from.begin(); it != from.end(); ++it) {
      if (is_path_suffix(path, it->first)) return &it->second;
    }
    return nullptr;
  }

  // ----- the core entry points ---------------------------------------------

  static void write_response(const std::string &payload, char **response, unsigned int *response_len) {
    *response_len = static_cast<unsigned int>(payload.size());
    *response = new char[payload.size() + 1];
    std::memcpy(*response, payload.data(), payload.size());
    (*response)[payload.size()] = '\0';
  }

  static NSCAPI::errorReturn settings_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) {
    stub_core &self = instance();
    PB::Settings::SettingsRequestMessage req;
    if (!req.ParseFromArray(request, static_cast<int>(request_len))) return NSCAPI::api_return_codes::hasFailed;

    PB::Settings::SettingsResponseMessage resp;
    for (int i = 0; i < req.payload_size(); ++i) {
      const PB::Settings::SettingsRequestMessage::Request &in = req.payload(i);
      PB::Settings::SettingsResponseMessage::Response *out = resp.add_payload();
      out->mutable_result()->set_code(PB::Common::Result::STATUS_OK);

      if (in.has_registration()) {
        const PB::Settings::SettingsRequestMessage::Request::Registration &reg = in.registration();
        if (reg.node().key().empty()) {
          self.registered_paths_.push_back(reg.node().path());
        } else {
          registered_key key;
          key.path = reg.node().path();
          key.key = reg.node().key();
          key.type = reg.info().type();
          key.default_value = reg.info().default_value();
          key.title = reg.info().title();
          key.description = reg.info().description();
          key.advanced = reg.info().advanced();
          key.sample = reg.info().sample();
          key.sensitive = reg.info().is_sensitive();
          self.registered_keys_.push_back(key);
        }
      } else if (in.has_query()) {
        const PB::Settings::SettingsRequestMessage::Request::Query &query = in.query();
        const std::string path = query.node().path();
        if (query.include_keys()) {
          // get_keys(path): the key names under it, in `nodes`.
          const std::vector<std::string> *keys = self.find_list(self.keys_, path);
          if (keys != nullptr) {
            for (const std::string &key : *keys) {
              out->mutable_query()->add_nodes()->set_key(key);
            }
          }
        } else if (query.recursive()) {
          // get_sections(path): subsection paths, in `nodes`.
          const std::vector<std::string> *sections = self.find_list(self.sections_, path);
          if (sections != nullptr) {
            for (const std::string &section : *sections) {
              out->mutable_query()->add_nodes()->set_path(path + "/" + section);
            }
          }
        } else {
          PB::Settings::Node *node = out->mutable_query()->mutable_node();
          node->set_path(path);
          node->set_key(query.node().key());
          node->set_value(self.lookup(path, query.node().key(), query.default_value()));
        }
      } else if (in.has_update()) {
        registered_key update;
        update.path = in.update().node().path();
        update.key = in.update().node().key();
        update.default_value = in.update().node().value();
        self.updated_settings_.push_back(update);
      }
      // Control (save/load) and everything else: an OK result is all the
      // caller reads.
    }
    write_response(resp.SerializeAsString(), response, response_len);
    return NSCAPI::api_return_codes::isSuccess;
  }

  static NSCAPI::errorReturn registry_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) {
    stub_core &self = instance();
    PB::Registry::RegistryRequestMessage req;
    if (!req.ParseFromArray(request, static_cast<int>(request_len))) return NSCAPI::api_return_codes::hasFailed;

    PB::Registry::RegistryResponseMessage resp;
    for (int i = 0; i < req.payload_size(); ++i) {
      PB::Registry::RegistryResponseMessage::Response *out = resp.add_payload();
      out->mutable_result()->set_code(PB::Common::Result::STATUS_OK);
      if (!req.payload(i).has_registration()) continue;
      const PB::Registry::RegistryRequestMessage::Request::Registration &reg = req.payload(i).registration();
      if (reg.unregister()) continue;
      self.registrations_.push_back(std::make_pair(static_cast<int>(reg.type()), reg.name()));
      for (int a = 0; a < reg.alias_size(); ++a) {
        self.registrations_.push_back(std::make_pair(static_cast<int>(reg.type()), reg.alias(a)));
      }
    }
    write_response(resp.SerializeAsString(), response, response_len);
    return NSCAPI::api_return_codes::isSuccess;
  }

  // Storage is only read to restore state a unit test has none of; an empty
  // response parses to zero payloads, which every caller treats as "nothing
  // stored".
  static NSCAPI::errorReturn empty_query(const char *, const unsigned int, char **response, unsigned int *response_len) {
    write_response(std::string(), response, response_len);
    return NSCAPI::api_return_codes::isSuccess;
  }

  // ${base-path} and friends expand to themselves: a test that cares about a
  // path passes an absolute one.
  static NSCAPI::errorReturn expand_path(const char *value, char *buffer, const unsigned int buf_len) {
    const std::string in(value == nullptr ? "" : value);
    if (in.size() >= buf_len) return NSCAPI::api_return_codes::hasFailed;
    std::memcpy(buffer, in.data(), in.size());
    buffer[in.size()] = '\0';
    return NSCAPI::api_return_codes::isSuccess;
  }

  static NSCAPI::errorReturn application_version(char *buffer, const unsigned int buf_len) {
    const std::string &version = instance().version_;
    if (version.empty() || version.size() >= buf_len) return NSCAPI::api_return_codes::hasFailed;
    std::memcpy(buffer, version.data(), version.size());
    buffer[version.size()] = '\0';
    return NSCAPI::api_return_codes::isSuccess;
  }

  static void destroy_buffer(char **buffer) {
    delete[] *buffer;
    *buffer = nullptr;
  }

  static nscapi::core_api::FUNPTR load_endpoint(const char *name) {
    const std::string n(name == nullptr ? "" : name);
    if (n == "NSAPISettingsQuery") return reinterpret_cast<nscapi::core_api::FUNPTR>(&settings_query);
    if (n == "NSAPIRegistryQuery") return reinterpret_cast<nscapi::core_api::FUNPTR>(&registry_query);
    if (n == "NSAPIStorageQuery") return reinterpret_cast<nscapi::core_api::FUNPTR>(&empty_query);
    if (n == "NSAPIExpandPath") return reinterpret_cast<nscapi::core_api::FUNPTR>(&expand_path);
    if (n == "NSAPIGetApplicationVersionStr") return reinterpret_cast<nscapi::core_api::FUNPTR>(&application_version);
    if (n == "NSAPIDestroyBuffer") return reinterpret_cast<nscapi::core_api::FUNPTR>(&destroy_buffer);
    // Everything else (logging above all) stays null, which the wrapper
    // treats as a no-op.
    return nullptr;
  }

  std::string version_;
  std::map<std::string, std::string> settings_;
  std::map<std::string, std::vector<std::string> > keys_;
  std::map<std::string, std::vector<std::string> > sections_;
  std::vector<registered_key> registered_keys_;
  std::vector<registered_key> updated_settings_;
  std::vector<std::string> registered_paths_;
  // (PB::Registry::ItemType, name) in registration order.
  std::vector<std::pair<int, std::string> > registrations_;
};

}  // namespace test_helpers
}  // namespace nscapi
