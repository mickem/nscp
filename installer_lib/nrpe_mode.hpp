// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>
#include <vector>

// The installer's NRPE transport-security preset, as a decision free of MSI and
// of the settings core so it can be unit tested (see nrpe_mode_test.cpp).
//
// Three custom actions share it.
//
// ImportConfig reads the configuration this host already has (or the one
// IMPORT_CONFIG named) and records what it found as DEFAULT_NRPEMODE, which is
// what preselects the dialog's radio group and lets a later move of it be
// recognised as a request. That recording has to be honest: writing something
// the operator cannot have chosen (an empty string, say) makes every install
// look like a request to change the mode, which is how an imported
// configuration ended up with four NRPE keys it never asked for (#1558).
//
// That record is not always there to compare against, though: ImportConfig
// returns early on several paths - configuration changes not allowed, a settings
// context that failed to boot, a target store that cannot be edited, or simply
// no configuration found - and each of those leaves DEFAULT_NRPEMODE empty while
// KEY_NRPEMODE still holds its properties.wxs default. So the preset is not
// decided from the property pair at all. ScheduleWriteConfig works out only
// whether the operator *asked* for a mode (asked_for_mode below) and hands that
// to ExecWriteConfig, which is deferred, has the configuration loaded, and
// applies the preset only where nothing in that configuration would be
// overwritten. The rule then reads off the configuration itself rather than off
// a property whose absence means two different things.
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
  // No NRPE listener configured at all - the section holds no keys. A fresh
  // install, or an imported configuration that never mentions NRPE. This is the
  // only state the installer's preset is allowed to fill in.
  unconfigured,
  // `insecure = true`: the relaxed-cipher mode for ancient check_nrpe builds.
  legacy,
  // `verify mode = peer-cert`: client certificates required.
  secure,
  // Configured, but as neither preset - `verify mode = none` on a TLS listener,
  // for instance. The operator chose those values, so the installer keeps its
  // hands off them.
  custom,
  // A listener the operator has configured (the section has keys) without
  // naming either mode key, so it runs on whatever NRPEServer itself defaults
  // to. That is a working setup and a choice like any other: imposing
  // `verify mode = peer-cert` on it at upgrade time would start requiring
  // client certificates from a listener that never asked for them.
  module_defaults,
};

// What the configuration holds under /settings/NRPE/server. The presence flags
// mirror the settings store's has_key(): a key that is present but empty still
// means the operator wrote something there, so it does not count as
// unconfigured.
struct server_security {
  bool has_insecure = false;
  bool has_verify_mode = false;
  std::string insecure;
  std::string verify_mode;
  // Whether the section holds any key at all - `port`, `allowed hosts`, a
  // certificate path, anything. This is what separates a host that has an NRPE
  // listener configured from one that has never had it set up, which is the
  // only case where the installer's preset is the operator's own wish.
  bool section_has_keys = false;
};

// `true` and `1` are the two spellings the installer itself writes, and the
// only ones recognised here. Any other spelling of a boolean lands in `custom`,
// which is the safe answer rather than a wrong one: it leaves the section as
// the operator wrote it.
inline mode classify(const server_security &config) {
  if (!config.has_insecure && !config.has_verify_mode) {
    return config.section_has_keys ? mode::module_defaults : mode::unconfigured;
  }
  if (config.insecure == "true" || config.insecure == "1") return mode::legacy;
  if (config.verify_mode == "peer-cert") return mode::secure;
  return mode::custom;
}

// Whether the installer may write its preset over this configuration. Only a
// host with no NRPE listener configured at all gets one; every other answer is
// a setup that already works and that the operator, not the installer, owns.
inline bool preset_may_be_applied(mode detected) { return detected == mode::unconfigured; }

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
    case mode::module_defaults:
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

// Whether the operator asked for an NRPE mode, which is the one thing that
// overrides the configuration already on the host.
//
// `bare_property` is the NRPEMODE property. It has no default in
// properties.wxs, so a value there can only have come from the command line.
//
// `recorded_default` and `current_key` are DEFAULT_NRPEMODE and KEY_NRPEMODE.
// A difference between them means the mode was moved off what ImportConfig
// found - but only once ImportConfig has actually recorded something, which is
// why an empty recorded_default is not a request. Without that guard every
// install where ImportConfig recorded nothing looks like one, since
// KEY_NRPEMODE always holds its properties.wxs default of SECURE.
inline bool asked_for_mode(const std::string &bare_property, const std::string &recorded_default, const std::string &current_key) {
  if (!bare_property.empty()) return true;
  return !recorded_default.empty() && recorded_default != current_key;
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
