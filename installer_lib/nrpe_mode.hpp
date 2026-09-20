// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

// The installer's NRPE transport-security preset, as a decision free of MSI and
// of the settings core so it can be unit tested (see nrpe_mode_test.cpp).
//
// Two custom actions share it. ImportConfig reads the configuration this host
// already has (or the one IMPORT_CONFIG named) and records what it found as
// DEFAULT_NRPEMODE; ScheduleWriteConfig then writes the preset only when
// KEY_NRPEMODE differs from that. That difference is the installer's standing
// rule for "the operator asked for this" - see the prefix comments in keys.hpp.
//
// The rule only works if DEFAULT_ is an honest record of what the configuration
// says. Recording something the operator cannot have chosen (an empty string,
// say) makes every install look like a request to change the mode, which is how
// an imported configuration ended up with four NRPE keys it never asked for
// (#1558).
namespace installer {
namespace nrpe {

// The values of the NRPEMODE radio group, from installers/ui/ConfigureDlg.wxs.
// KEY_NRPEMODE defaults to kModeSecure in properties.wxs; the bare NRPEMODE
// property has no default at all, which is what lets the caller tell an
// operator who named a mode from one who never mentioned it.
const char *const kModeLegacy = "LEGACY";
const char *const kModeSecure = "SECURE";

// What a configuration says about the NRPE server's transport security.
enum class mode {
  // Nothing at all: a fresh install, or an imported configuration that leaves
  // the NRPE transport to the module's own defaults. The preset applies.
  unconfigured,
  // `insecure = true`: the relaxed-cipher mode for ancient check_nrpe builds.
  legacy,
  // `verify mode = peer-cert`: client certificates required.
  secure,
  // Configured, but as neither preset - `verify mode = none` on a TLS listener,
  // for instance. The operator chose those values, so the installer keeps its
  // hands off them.
  custom,
};

// The two keys that decide the mode, as the configuration holds them. The
// presence flags mirror the settings store's has_key(): a key that is present
// but empty still means the operator wrote something there, so it does not
// count as unconfigured.
struct server_security {
  bool has_insecure = false;
  bool has_verify_mode = false;
  std::string insecure;
  std::string verify_mode;
};

// `true` and `1` are the two spellings the installer itself writes, and the
// only ones recognised here. Any other spelling of a boolean lands in `custom`,
// which is the safe answer rather than a wrong one: it leaves the section as
// the operator wrote it.
inline mode classify(const server_security &config) {
  if (!config.has_insecure && !config.has_verify_mode) return mode::unconfigured;
  if (config.insecure == "true" || config.insecure == "1") return mode::legacy;
  if (config.verify_mode == "peer-cert") return mode::secure;
  return mode::custom;
}

// What ImportConfig records for the NRPEMODE property pair.
struct recorded_properties {
  // False leaves both properties untouched, so the MSI default stands.
  bool record = false;
  std::string key;       // KEY_NRPEMODE
  std::string default_;  // DEFAULT_NRPEMODE
};

// `current_key` is KEY_NRPEMODE as it stands when ImportConfig runs, which is
// the properties.wxs default: the action runs long before the configuration
// dialog, and the bare NRPEMODE property is only copied over KEY_NRPEMODE after
// this (applyPropertyValue). `operator_named_a_mode` is whether that bare
// property was set, i.e. whether the command line asked for a mode.
inline recorded_properties record_mode(mode detected, const std::string &current_key, bool operator_named_a_mode) {
  recorded_properties ret;
  switch (detected) {
    case mode::legacy:
    case mode::secure: {
      // Preselect the radio group on what the host already uses and record the
      // same value, so nothing is written unless the mode is actually moved -
      // by the dialog or by the command line, both of which reach KEY_NRPEMODE
      // after this point.
      const std::string found = detected == mode::legacy ? kModeLegacy : kModeSecure;
      ret.record = true;
      ret.key = found;
      ret.default_ = found;
      return ret;
    }
    case mode::custom:
      ret.record = true;
      ret.key = current_key;
      // Nothing to preselect, and nothing of ours to apply. Recording the
      // value KEY_NRPEMODE already holds makes the pair read "unchanged", so
      // the operator's own keys survive the install (#1558). An operator who
      // names a mode on the command line still gets it: DEFAULT_ is left empty
      // so their value differs from it whichever mode they picked.
      ret.default_ = operator_named_a_mode ? std::string() : current_key;
      return ret;
    case mode::unconfigured:
    default:
      // No operator configuration to preserve, so leave the properties alone:
      // KEY_NRPEMODE keeps its MSI default and DEFAULT_NRPEMODE stays empty,
      // which is what gets a preset written onto a fresh install.
      return ret;
  }
}

// A key the installer writes under /settings/NRPE/server.
struct setting {
  std::string key;
  std::string value;
};

// The preset ScheduleWriteConfig writes for a mode, empty for a value that is
// neither preset (so a typo in NRPEMODE no longer silently applies SECURE).
//
// Both keys go in for either mode, deliberately: they are mutually exclusive,
// so switching modes has to undo the other one.
//
// What is *not* written any more is `tls version` and `ssl options`. Both were
// set to the value the NRPE module already defaults to (`tlsv1.2+` and empty,
// from socket_helpers::settings_helper::add_ssl_server_opts), so they added
// nothing to a fresh install - while on an import they replaced an operator's
// `tls version = tls1.3` with a lower one and wiped any ssl options (#1558).
inline std::vector<setting> preset(const std::string &wanted_mode) {
  std::vector<setting> ret;
  if (wanted_mode == kModeLegacy) {
    ret.push_back({"insecure", "true"});
    ret.push_back({"verify mode", "none"});
  } else if (wanted_mode == kModeSecure) {
    ret.push_back({"insecure", "false"});
    ret.push_back({"verify mode", "peer-cert"});
  }
  return ret;
}

}  // namespace nrpe
}  // namespace installer
