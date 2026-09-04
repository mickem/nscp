// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <NSCAPI.h>

#include <map>
#include <nscapi/dll_defines.hpp>
#include <string>

namespace nscapi {
class core_wrapper_impl;
class NSCAPI_EXPORT core_wrapper {
  core_wrapper_impl *pimpl;
  core_api::lpNSAPIGetApplicationName fNSAPIGetApplicationName;
  core_api::lpNSAPIGetApplicationVersionStr fNSAPIGetApplicationVersionStr;
  core_api::lpNSAPIMessage fNSAPIMessage;
  core_api::lpNSAPISimpleMessage fNSAPISimpleMessage;
  core_api::lpNSAPIInject fNSAPIInject;
  core_api::lpNSAPIExecCommand fNSAPIExecCommand;
  core_api::lpNSAPIDestroyBuffer fNSAPIDestroyBuffer;
  core_api::lpNSAPINotify fNSAPINotify;
  core_api::lpNSAPIReload fNSAPIReload;
  core_api::lpNSAPICheckLogMessages fNSAPICheckLogMessages;
  core_api::lpNSAPISettingsQuery fNSAPISettingsQuery;
  core_api::lpNSAPIExpandPath fNSAPIExpandPath;
  core_api::lpNSAPIGetLoglevel fNSAPIGetLoglevel;
  core_api::lpNSAPIRegistryQuery fNSAPIRegistryQuery;
  core_api::lpNSCAPIEmitEvent fNSCAPIEmitEvent;
  core_api::lpNSAPIStorageQuery fNSAPIStorageQuery;
  core_api::lpNSAPISetTag fNSAPISetTag;
  core_api::lpNSAPIGetTags fNSAPIGetTags;
  core_api::lpNSAPISetLogOption fNSAPISetLogOption;

 public:
  core_wrapper();
  ~core_wrapper();

  std::string expand_path(std::string value) const;

  NSCAPI::errorReturn settings_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
  bool settings_query(const std::string request, std::string &response) const;

  // Helper functions for calling into the core
  std::string getApplicationName(void) const;
  std::string getApplicationVersionString(void) const;

  void log(NSCAPI::nagiosReturn msgType, std::string file, int line, std::string message) const;
  void log(std::string message) const;
  bool should_log(NSCAPI::nagiosReturn msgType) const;
  NSCAPI::log_level::level get_loglevel() const;
  void DestroyBuffer(char **buffer) const;
  NSCAPI::nagiosReturn query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
  bool query(const std::string &request, std::string &result) const;

  NSCAPI::nagiosReturn exec_command(const char *target, const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
  bool exec_command(const std::string target, std::string request, std::string &result) const;

  NSCAPI::errorReturn submit_message(const char *channel, const char *request, const unsigned int request_len, char **response,
                                     unsigned int *response_len) const;
  NSCAPI::errorReturn emit_event(const char *request, const unsigned int request_len) const;
  NSCAPI::errorReturn emit_event(std::string &request) const;
  bool submit_message(std::string channel, std::string request, std::string &response) const;
  bool reload(std::string module) const;

  bool checkLogMessages(int type);

  NSCAPI::errorReturn registry_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
  bool registry_query(const std::string request, std::string &response) const;

  NSCAPI::errorReturn storage_query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const;
  bool storage_query(const std::string request, std::string &response) const;

  // Host tags: small key=value facts about this host kept in a central
  // repository in the core (consumed by e.g. the web UI and the fleet sync).
  // set_tag with an empty value removes the tag. Both degrade gracefully
  // (return false / "{}") on cores that predate the tag API; set_tag also
  // returns false when the core rejected the tag (oversized key/value, or
  // the repository is at its tag cap).
  bool set_tag(const std::string &key, const std::string &value) const;
  // The full tag map as a JSON object string, e.g. {"drives":"c:,d:"}. Right
  // for a passthrough consumer (the web tags controller); a module that wants
  // to read tags should prefer the typed get_tags() below.
  std::string get_tags_json() const;

  // Change one logging option at runtime; takes the same strings as the --log
  // switch (a severity, or a driver option such as "no-console"). Returns
  // false on a core that predates the call. A module that paints its own
  // console uses "no-console" so the core stops writing to stdout underneath
  // it - see CommandClient's interactive prompt.
  bool set_log_option(const std::string &option) const;
  // The full tag map, typed. Parses the JSON once here so a consuming module
  // does not have to link a JSON parser just to read what a sibling published.
  std::map<std::string, std::string> get_tags() const;
  // Parse the flat {"k":"v",...} object get_tags_json() returns into a map.
  // Deliberately not a general JSON parser: it handles exactly that shape plus
  // standard string escapes, and returns what it has parsed so far on anything
  // unexpected. Static and core-free so it can be unit tested directly.
  static std::map<std::string, std::string> parse_tags_json(const std::string &json);

  bool load_endpoints(core_api::lpNSAPILoader f);
  void set_alias(const std::string default_alias, const std::string alias);
};
}  // namespace nscapi