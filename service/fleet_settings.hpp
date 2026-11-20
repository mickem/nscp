// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <config.h>

#include <exception>
#include <functional>
#include <net/socket/socket_helpers.hpp>
#include <settings/settings_interface.hpp>
#include <stdexcept>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <string>
#include <vector>

#include "fleet_sync.hpp"
#include "path_manager.hpp"

namespace nsclient {
namespace core {

// [/settings/fleet], turned into the configuration the fleet sync runs with.
//
// Every key is declared and read in one place, here, so its description sits
// next to what is made of its value. `key` is how that reaches the settings:
// in the service it registers the key and returns what the operator set; in a
// unit test it looks the name up in a map. Whatever would have been logged
// about a value that cannot be used comes back in `errors`, and the default
// stays in force for that key.
//
// Not everything in fleet_config is a setting. The version, the plaintext
// opt-in from boot.ini and the local-configuration probe are the caller's to
// fill in.
namespace fleet_settings {

constexpr const char *const section = "/settings/fleet";

// Declare one key and return its value. `type` is the settings type ("string"
// or "bool"); the value always comes back as text.
typedef std::function<std::string(const char *key, const char *type, const char *title, const char *description, const char *fallback)> key_reader;

inline fleet_config read(const key_reader &key, path_manager &paths, std::vector<std::string> &errors) {
  fleet_config config;
  config.state_file = paths.expand_path(key("state file", "string", "State file",
                                            "The enrollment manifest written by `nscp enroll` (certificates, keys and server urls). "
                                            "Fleet sync only runs when this file exists.",
                                            DEFAULT_FLEET_STATE_LOCATION));
  // Rooted at ${fleet-folder}: the sync *writes* this tree - the rendered
  // fleet.ini, the staged scripts, the bundle cache - so a relative value
  // would scatter it wherever the service happened to be started from.
  config.managed_path = paths.resolve_path(
      key("managed path", "string", "Managed path",
          "Directory where the synced configuration (fleet.ini), scripts and the bundle cache are kept. A relative name is taken relative to the "
          "fleet folder; an absolute path is used as given.",
          "${" FLEET_FOLDER_KEY "}"),
      "${" FLEET_FOLDER_KEY "}");
  config.hostname = socket_helpers::expand_hostname(
      key("hostname", "string", "Hostname", "Hostname reported as a tag to the fleet server. Set to auto (default) to use this machine's hostname.", "auto"));
  config.tls_version = key("tls version", "string", "TLS version", "The TLS version used when connecting to the fleet server.", "tlsv1.2+");

  const std::string timeout = key("timeout", "string", "Request timeout",
                                  "How long a single request to the fleet server may take before it is abandoned and retried. A server that "
                                  "accepts the connection and then stops responding would otherwise stall the sync loop indefinitely.",
                                  "60s");
  try {
    // Zero would be a deadline every request misses before it is sent.
    const long seconds = str::format::stox_as_time_sec<long>(timeout, "s");
    if (seconds < 1) throw std::invalid_argument("must be at least 1s");
    config.timeout_seconds = static_cast<unsigned int>(seconds);
  } catch (const std::exception &e) {
    errors.push_back("Invalid fleet 'timeout' value '" + timeout + "', falling back to 60s: " + utf8::utf8_from_native(e.what()));
  }

  config.failure_context = settings::settings_interface::string_to_bool(
      key("failure context", "bool", "Send failure context",
          "Send the fleet server a failed check from [/settings/core/failed checks], with the metrics snapshot it was recorded with when capture "
          "metrics is on there, and a summary of the other failures since the last report. Only to a server that asks for it, and at most once "
          "per `failure context interval`. Read when the service starts.",
          "true"));
  const std::string failure_interval = key("failure context interval", "string", "Failure report interval",
                                           "The least time between two failed-check reports to the fleet server (at least 5s; the server can ask "
                                           "for longer). Failures in between are summarised in the next report. Read when the service starts.",
                                           "1h");
  try {
    config.failure_context_interval = static_cast<unsigned long>(str::format::stox_as_time_sec<long>(failure_interval, "s"));
  } catch (const std::exception &e) {
    errors.push_back("Invalid fleet 'failure context interval' value '" + failure_interval + "', falling back to 1h: " + utf8::utf8_from_native(e.what()));
  }

  // The three poll interval keys share one shape: a duration, at least a
  // second, falling back to the default with an error when it does not parse.
  const auto interval = [&key, &errors](const char *name, const char *title, const char *description, const char *fallback, unsigned long &target) {
    const std::string value = key(name, "string", title, description, fallback);
    try {
      const long seconds = str::format::stox_as_time_sec<long>(value, "s");
      if (seconds < 1) throw std::invalid_argument("must be at least 1s");
      target = static_cast<unsigned long>(seconds);
    } catch (const std::exception &e) {
      errors.push_back(std::string("Invalid fleet '") + name + "' value '" + value + "', falling back to " + fallback + ": " +
                       utf8::utf8_from_native(e.what()));
    }
  };
  interval("poll interval", "Poll interval",
           "How often to ask the fleet server for this host's configuration until the server names an interval of its own, which it does in "
           "every answer. Read when the service starts.",
           "60s", config.poll_interval);
  interval("min poll interval", "Shortest poll interval",
           "The shortest interval the fleet server may ask this host to poll at: a shorter one is raised to this. Use it to keep a host on a "
           "metered or slow link from polling as often as the rest of the fleet. Read when the service starts.",
           "1s", config.min_poll_interval);
  interval("max poll interval", "Longest poll interval",
           "The longest interval the fleet server may ask this host to poll at: a longer one is lowered to this, so a configuration change "
           "reaches the host within it. Raised to `min poll interval` when set below it. Read when the service starts.",
           "1d", config.max_poll_interval);
  return config;
}

}  // namespace fleet_settings
}  // namespace core
}  // namespace nsclient
