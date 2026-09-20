// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nrpe_mode.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using installer::nrpe::asked_for_mode;
using installer::nrpe::classify;
using installer::nrpe::kModeLegacy;
using installer::nrpe::kModeSecure;
using installer::nrpe::mode;
using installer::nrpe::preset;
using installer::nrpe::preset_may_be_applied;
using installer::nrpe::record_mode;
using installer::nrpe::server_security;
using installer::nrpe::setting;

namespace {

// A configuration that sets `insecure`, and nothing else.
server_security with_insecure(const std::string &value) {
  server_security ret;
  ret.has_insecure = true;
  ret.insecure = value;
  ret.section_has_keys = true;
  return ret;
}

// A configuration that sets `verify mode`, and nothing else.
server_security with_verify_mode(const std::string &value) {
  server_security ret;
  ret.has_verify_mode = true;
  ret.verify_mode = value;
  ret.section_has_keys = true;
  return ret;
}

// An NRPE listener the operator has configured without naming either mode key -
// `[/settings/NRPE/server] port = 5666` and nothing more.
server_security on_module_defaults() {
  server_security ret;
  ret.section_has_keys = true;
  return ret;
}

// A host with no NRPE listener configured at all.
server_security unconfigured() { return server_security(); }

// How the install reached the point of deciding. `dialog_mode` is a radio group
// the operator moved, `command_line_mode` the bare NRPEMODE property.
// `recorded` is whether ImportConfig got as far as recording what it found: it
// returns early when changes are not allowed, when a settings context fails to
// boot, when the target store cannot be edited and when there is no
// configuration at all, and each of those leaves DEFAULT_NRPEMODE empty.
struct install_run {
  std::string key = kModeSecure;  // KEY_NRPEMODE, from properties.wxs
  std::string recorded;           // DEFAULT_NRPEMODE, which has no default
  bool asked = false;
  std::vector<setting> written;  // what lands under /settings/NRPE/server
};

install_run run_install(const server_security &existing, bool recorded = true, const std::string &command_line_mode = "",
                        const std::string &dialog_mode = "") {
  install_run run;

  // ImportConfig, where it got as far as recording what it read.
  if (recorded) {
    const installer::nrpe::recorded_properties props = record_mode(classify(existing), run.key, !command_line_mode.empty());
    if (props.record) {
      run.key = props.key;
      run.recorded = props.default_;
    }
  }
  // applyPropertyValue(NRPEMODE) copies the bare property over KEY_NRPEMODE,
  // then the configuration dialog can move the radio group.
  if (!command_line_mode.empty()) run.key = command_line_mode;
  if (!dialog_mode.empty()) run.key = dialog_mode;

  // ScheduleWriteConfig decides only whether a mode was asked for.
  run.asked = asked_for_mode(command_line_mode, run.recorded, run.key);

  // ExecWriteConfig applies the preset against the loaded configuration.
  if (run.asked || preset_may_be_applied(classify(existing))) run.written = preset(run.key);
  return run;
}

// An install where ImportConfig bailed out before recording anything, so the
// property pair cannot say whether the mode was chosen or merely defaulted.
install_run run_unrecorded_install(const server_security &existing, const std::string &command_line_mode = "") {
  return run_install(existing, false, command_line_mode);
}

// "key=value" pairs, for assertions that read like the INI section they produce.
std::vector<std::string> rendered(const std::vector<setting> &settings) {
  std::vector<std::string> ret;
  for (const setting &s : settings) {
    ret.push_back(s.key + "=" + s.value);
  }
  return ret;
}

bool writes_key(const std::vector<setting> &settings, const std::string &key) {
  return std::any_of(settings.begin(), settings.end(), [&key](const setting &s) { return s.key == key; });
}

const std::vector<std::string> kLegacyPreset({"insecure=true", "verify mode=none"});
const std::vector<std::string> kSecurePreset({"insecure=false", "verify mode=peer-cert"});

}  // namespace

// ---------------------------------------------------------------------------
// Reading an existing configuration
// ---------------------------------------------------------------------------

TEST(nrpe_mode, a_host_with_no_nrpe_listener_is_unconfigured) { EXPECT_EQ(mode::unconfigured, classify(unconfigured())); }

TEST(nrpe_mode, insecure_is_the_legacy_preset) {
  EXPECT_EQ(mode::legacy, classify(with_insecure("true")));
  EXPECT_EQ(mode::legacy, classify(with_insecure("1")));
}

TEST(nrpe_mode, peer_cert_is_the_secure_preset) { EXPECT_EQ(mode::secure, classify(with_verify_mode("peer-cert"))); }

TEST(nrpe_mode, a_mode_key_that_is_neither_preset_is_custom) {
  // The configuration from #1558: TLS on, but no client certificates.
  EXPECT_EQ(mode::custom, classify(with_verify_mode("none")));
  // The module defaults, spelled out. Still a choice the operator made.
  EXPECT_EQ(mode::custom, classify(with_insecure("false")));
  // A verify mode the presets do not cover.
  EXPECT_EQ(mode::custom, classify(with_verify_mode("peer")));
  // Present but empty is not the same as absent.
  server_security empty_value;
  empty_value.has_verify_mode = true;
  empty_value.section_has_keys = true;
  EXPECT_EQ(mode::custom, classify(empty_value));
}

TEST(nrpe_mode, a_configured_listener_with_no_mode_key_runs_on_module_defaults) {
  EXPECT_EQ(mode::module_defaults, classify(on_module_defaults()));
}

// section_has_keys has to describe the operator's configuration and nothing
// else. ExecWriteConfig writes into this same section - a `certificate key` from
// the CERTIFICATE_KEY property - so it reads the section before applying any of
// its own keys. Were it to read afterwards, a fresh install would look like the
// listener below and be denied the preset it should get, which is why that
// snapshot must stay where it is.
TEST(nrpe_mode, a_section_holding_only_an_installer_written_key_suppresses_the_preset) {
  server_security as_the_installer_would_leave_it;
  as_the_installer_would_leave_it.section_has_keys = true;  // e.g. `certificate key`

  EXPECT_EQ(mode::module_defaults, classify(as_the_installer_would_leave_it));
  EXPECT_FALSE(preset_may_be_applied(classify(as_the_installer_would_leave_it)));
}

TEST(nrpe_mode, only_an_unconfigured_host_may_be_given_a_preset) {
  EXPECT_TRUE(preset_may_be_applied(mode::unconfigured));
  EXPECT_FALSE(preset_may_be_applied(mode::legacy));
  EXPECT_FALSE(preset_may_be_applied(mode::secure));
  EXPECT_FALSE(preset_may_be_applied(mode::custom));
  EXPECT_FALSE(preset_may_be_applied(mode::module_defaults));
}

// ---------------------------------------------------------------------------
// #1558: an imported or existing configuration keeps what it came with
// ---------------------------------------------------------------------------

// The reporter's central nsclient.ini held, under [/settings/NRPE/server],
// `tls version = tls1.3` and `verify mode = none`. Importing it with
// `msiexec /qr /i NSCP-0.21.0-x64.msi IMPORT_CONFIG=https://.../nsclient.ini`
// came back with `verify mode = peer-cert`, `tls version = tlsv1.2+`,
// `insecure = false` and `ssl options = ` - four keys the operator never asked
// for, two of which changed the listener's behaviour.
TEST(nrpe_mode, issue_1558_an_imported_verify_mode_none_is_left_alone) {
  const server_security imported = with_verify_mode("none");

  const install_run run = run_install(imported);

  EXPECT_EQ(mode::custom, classify(imported));
  EXPECT_FALSE(run.asked);
  EXPECT_TRUE(run.written.empty()) << "wrote " << rendered(run.written).size() << " NRPE keys over an imported configuration";
  EXPECT_FALSE(writes_key(run.written, "verify mode"));
  EXPECT_FALSE(writes_key(run.written, "tls version"));
  EXPECT_FALSE(writes_key(run.written, "insecure"));
  EXPECT_FALSE(writes_key(run.written, "ssl options"));
}

// An install where ImportConfig recorded nothing has no default to compare
// against, and the property pair alone then reads as a request on every run.
// The decision moved to ExecWriteConfig so this case behaves like any other.
TEST(nrpe_mode, issue_1558_holds_when_importconfig_recorded_nothing) {
  EXPECT_TRUE(run_unrecorded_install(with_verify_mode("none")).written.empty());
  EXPECT_TRUE(run_unrecorded_install(with_insecure("true")).written.empty());
  EXPECT_TRUE(run_unrecorded_install(with_verify_mode("peer-cert")).written.empty());
}

// The other half of the same ticket: even where a preset genuinely applies, it
// no longer touches `tls version` or `ssl options`, so an imported
// `tls version = tls1.3` is not quietly lowered to `tlsv1.2+`.
TEST(nrpe_mode, issue_1558_no_preset_writes_tls_version_or_ssl_options) {
  for (const std::string &m : {std::string(kModeLegacy), std::string(kModeSecure)}) {
    EXPECT_FALSE(writes_key(preset(m), "tls version")) << m;
    EXPECT_FALSE(writes_key(preset(m), "ssl options")) << m;
  }
}

// ---------------------------------------------------------------------------
// An upgrade of a listener that names no mode key
// ---------------------------------------------------------------------------

// A host running `[/settings/NRPE/server] port = 5666` and nothing else has a
// working listener on NRPEServer's own defaults, which do not verify the peer.
// Writing the secure preset over it starts demanding client certificates from
// every poller, on an upgrade that asked for nothing.
TEST(nrpe_mode, an_upgrade_of_a_listener_with_no_verify_mode_is_left_alone) {
  EXPECT_TRUE(run_install(on_module_defaults()).written.empty());
  EXPECT_TRUE(run_unrecorded_install(on_module_defaults()).written.empty());
}

TEST(nrpe_mode, that_listener_still_takes_a_mode_the_operator_names) {
  EXPECT_EQ(kSecurePreset, rendered(run_install(on_module_defaults(), true, kModeSecure).written));
  EXPECT_EQ(kLegacyPreset, rendered(run_unrecorded_install(on_module_defaults(), kModeLegacy).written));
}

// ---------------------------------------------------------------------------
// The presets themselves
// ---------------------------------------------------------------------------

TEST(nrpe_mode, the_presets_write_both_mode_keys) {
  EXPECT_EQ(kLegacyPreset, rendered(preset(kModeLegacy)));
  EXPECT_EQ(kSecurePreset, rendered(preset(kModeSecure)));
}

TEST(nrpe_mode, a_mode_that_is_neither_preset_writes_nothing) {
  EXPECT_TRUE(preset("").empty());
  EXPECT_TRUE(preset("legacy").empty());
  EXPECT_TRUE(preset("PEER-CERT").empty());
}

// ---------------------------------------------------------------------------
// The installs that still get a preset
// ---------------------------------------------------------------------------

TEST(nrpe_mode, a_fresh_install_gets_the_secure_preset) {
  EXPECT_EQ(kSecurePreset, rendered(run_install(unconfigured()).written));
  EXPECT_EQ(kSecurePreset, rendered(run_unrecorded_install(unconfigured()).written));
}

TEST(nrpe_mode, a_fresh_install_takes_the_mode_from_the_command_line) {
  EXPECT_EQ(kLegacyPreset, rendered(run_install(unconfigured(), true, kModeLegacy).written));
  EXPECT_EQ(kLegacyPreset, rendered(run_unrecorded_install(unconfigured(), kModeLegacy).written));
}

TEST(nrpe_mode, an_upgrade_keeps_the_mode_the_host_already_uses) {
  EXPECT_TRUE(run_install(with_insecure("true")).written.empty());
  EXPECT_TRUE(run_install(with_verify_mode("peer-cert")).written.empty());

  // And the radio group comes up on the mode the host uses.
  EXPECT_EQ(kModeLegacy, run_install(with_insecure("true")).key);
  EXPECT_EQ(kModeSecure, run_install(with_verify_mode("peer-cert")).key);
}

TEST(nrpe_mode, moving_the_radio_group_applies_that_preset) {
  const install_run switched = run_install(with_insecure("true"), true, "", kModeSecure);

  EXPECT_TRUE(switched.asked);
  EXPECT_EQ(kSecurePreset, rendered(switched.written));
}

TEST(nrpe_mode, a_dialog_left_on_the_detected_mode_writes_nothing) {
  EXPECT_TRUE(run_install(with_insecure("true"), true, "", kModeLegacy).written.empty());
}

TEST(nrpe_mode, a_mode_on_the_command_line_wins_over_a_custom_configuration) {
  // Either mode, including the one KEY_NRPEMODE already defaults to: an
  // operator who names it has asked for it.
  EXPECT_EQ(kLegacyPreset, rendered(run_install(with_verify_mode("none"), true, kModeLegacy).written));
  EXPECT_EQ(kSecurePreset, rendered(run_install(with_verify_mode("none"), true, kModeSecure).written));
  EXPECT_EQ(kSecurePreset, rendered(run_unrecorded_install(with_verify_mode("none"), kModeSecure).written));
}

// ---------------------------------------------------------------------------
// Whether a mode was asked for
// ---------------------------------------------------------------------------

TEST(nrpe_mode, the_command_line_property_is_always_a_request) {
  EXPECT_TRUE(asked_for_mode(kModeLegacy, "", kModeSecure));
  EXPECT_TRUE(asked_for_mode(kModeSecure, kModeSecure, kModeSecure));
}

TEST(nrpe_mode, a_radio_group_moved_off_what_was_found_is_a_request) { EXPECT_TRUE(asked_for_mode("", kModeSecure, kModeLegacy)); }

TEST(nrpe_mode, a_radio_group_left_where_it_was_found_is_not) { EXPECT_FALSE(asked_for_mode("", kModeSecure, kModeSecure)); }

// The trap this closes: KEY_NRPEMODE always holds its properties.wxs default of
// SECURE, so without the empty check every install where ImportConfig recorded
// nothing reads as a request to change the mode.
TEST(nrpe_mode, an_unrecorded_default_is_not_a_request) {
  EXPECT_FALSE(asked_for_mode("", "", kModeSecure));
  EXPECT_FALSE(asked_for_mode("", "", kModeLegacy));
}

// ---------------------------------------------------------------------------
// The property pair itself
// ---------------------------------------------------------------------------

TEST(nrpe_mode, an_unconfigured_host_leaves_both_properties_alone) { EXPECT_FALSE(record_mode(mode::unconfigured, kModeSecure, false).record); }

TEST(nrpe_mode, a_recognised_mode_records_itself_as_both_key_and_default) {
  const installer::nrpe::recorded_properties props = record_mode(mode::legacy, kModeSecure, false);

  EXPECT_TRUE(props.record);
  EXPECT_EQ(kModeLegacy, props.key);
  EXPECT_EQ(kModeLegacy, props.default_);
}

TEST(nrpe_mode, a_configuration_that_is_neither_preset_records_the_key_it_found) {
  for (const mode m : {mode::custom, mode::module_defaults}) {
    const installer::nrpe::recorded_properties props = record_mode(m, kModeSecure, false);

    EXPECT_TRUE(props.record);
    EXPECT_EQ(kModeSecure, props.key);
    EXPECT_EQ(kModeSecure, props.default_) << "DEFAULT_ must match KEY_ or moving nothing reads as a request";
  }
}

TEST(nrpe_mode, a_configuration_that_is_neither_preset_defers_to_a_named_mode) {
  const installer::nrpe::recorded_properties props = record_mode(mode::custom, kModeSecure, true);

  EXPECT_TRUE(props.record);
  EXPECT_EQ("", props.default_) << "an empty DEFAULT_ is what lets either named mode differ from it";
}
