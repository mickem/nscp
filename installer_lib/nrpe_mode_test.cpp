// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nrpe_mode.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using installer::nrpe::classify;
using installer::nrpe::kModeLegacy;
using installer::nrpe::kModeSecure;
using installer::nrpe::mode;
using installer::nrpe::preset;
using installer::nrpe::record_mode;
using installer::nrpe::server_security;
using installer::nrpe::setting;

namespace {

// A configuration that sets `insecure`.
server_security with_insecure(const std::string &value) {
  server_security ret;
  ret.has_insecure = true;
  ret.insecure = value;
  return ret;
}

// A configuration that sets `verify mode`.
server_security with_verify_mode(const std::string &value) {
  server_security ret;
  ret.has_verify_mode = true;
  ret.verify_mode = value;
  return ret;
}

// The two custom actions composed, so a test can state what an install does to
// a configuration rather than what each half of the property dance returns.
//
// The ordering mirrors Product.wxs: ImportConfig runs early (After ApplyTool),
// then applyPropertyValue copies a command-line NRPEMODE over KEY_NRPEMODE,
// then the configuration dialog can move the radio group, and only afterwards
// does ScheduleWriteConfig diff KEY_ against DEFAULT_.
struct install_run {
  std::string key = kModeSecure;  // KEY_NRPEMODE, from properties.wxs
  std::string recorded;           // DEFAULT_NRPEMODE, which has no default
  std::vector<setting> written;   // what lands under /settings/NRPE/server
};

install_run run_install(const server_security &existing, const std::string &command_line_mode = "", const std::string &dialog_mode = "") {
  install_run run;

  const installer::nrpe::recorded_properties props = record_mode(classify(existing), run.key, !command_line_mode.empty());
  if (props.record) {
    run.key = props.key;
    run.recorded = props.default_;
  }
  if (!command_line_mode.empty()) run.key = command_line_mode;
  if (!dialog_mode.empty()) run.key = dialog_mode;

  if (run.key != run.recorded) run.written = preset(run.key);
  return run;
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

}  // namespace

// ---------------------------------------------------------------------------
// Reading an existing configuration
// ---------------------------------------------------------------------------

TEST(nrpe_mode, a_configuration_that_says_nothing_is_unconfigured) { EXPECT_EQ(mode::unconfigured, classify(server_security())); }

TEST(nrpe_mode, insecure_is_the_legacy_preset) {
  EXPECT_EQ(mode::legacy, classify(with_insecure("true")));
  EXPECT_EQ(mode::legacy, classify(with_insecure("1")));
}

TEST(nrpe_mode, peer_cert_is_the_secure_preset) { EXPECT_EQ(mode::secure, classify(with_verify_mode("peer-cert"))); }

TEST(nrpe_mode, anything_else_is_custom) {
  // The configuration from #1558: TLS on, but no client certificates.
  EXPECT_EQ(mode::custom, classify(with_verify_mode("none")));
  // The module defaults, spelled out. Still a choice the operator made.
  EXPECT_EQ(mode::custom, classify(with_insecure("false")));
  // A verify mode the presets do not cover.
  EXPECT_EQ(mode::custom, classify(with_verify_mode("peer")));
  // Present but empty is not the same as absent.
  server_security empty_value;
  empty_value.has_verify_mode = true;
  EXPECT_EQ(mode::custom, classify(empty_value));
}

// ---------------------------------------------------------------------------
// #1558: an imported configuration keeps the transport security it came with
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
  EXPECT_TRUE(run.written.empty()) << "wrote " << rendered(run.written).size() << " NRPE keys over an imported configuration";
  EXPECT_FALSE(writes_key(run.written, "verify mode"));
  EXPECT_FALSE(writes_key(run.written, "tls version"));
  EXPECT_FALSE(writes_key(run.written, "insecure"));
  EXPECT_FALSE(writes_key(run.written, "ssl options"));
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
// The presets themselves
// ---------------------------------------------------------------------------

TEST(nrpe_mode, the_presets_write_both_mode_keys) {
  EXPECT_EQ(std::vector<std::string>({"insecure=true", "verify mode=none"}), rendered(preset(kModeLegacy)));
  EXPECT_EQ(std::vector<std::string>({"insecure=false", "verify mode=peer-cert"}), rendered(preset(kModeSecure)));
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
  const install_run run = run_install(server_security());

  EXPECT_EQ(std::vector<std::string>({"insecure=false", "verify mode=peer-cert"}), rendered(run.written));
}

TEST(nrpe_mode, an_upgrade_keeps_the_mode_the_host_already_uses) {
  // Both of these used to be rewritten with the same values they already had,
  // plus a tls version and an ssl options line.
  EXPECT_TRUE(run_install(with_insecure("true")).written.empty());
  EXPECT_TRUE(run_install(with_verify_mode("peer-cert")).written.empty());

  // And the radio group comes up on the mode the host uses.
  EXPECT_EQ(kModeLegacy, run_install(with_insecure("true")).key);
  EXPECT_EQ(kModeSecure, run_install(with_verify_mode("peer-cert")).key);
}

TEST(nrpe_mode, moving_the_radio_group_applies_that_preset) {
  const install_run switched = run_install(with_insecure("true"), "", kModeSecure);

  EXPECT_EQ(std::vector<std::string>({"insecure=false", "verify mode=peer-cert"}), rendered(switched.written));
}

TEST(nrpe_mode, a_dialog_left_on_the_detected_mode_writes_nothing) {
  EXPECT_TRUE(run_install(with_insecure("true"), "", kModeLegacy).written.empty());
}

TEST(nrpe_mode, a_mode_on_the_command_line_wins_over_a_custom_configuration) {
  // Either mode, including the one KEY_NRPEMODE already defaults to: an
  // operator who names it has asked for it.
  EXPECT_EQ(std::vector<std::string>({"insecure=true", "verify mode=none"}), rendered(run_install(with_verify_mode("none"), kModeLegacy).written));
  EXPECT_EQ(std::vector<std::string>({"insecure=false", "verify mode=peer-cert"}), rendered(run_install(with_verify_mode("none"), kModeSecure).written));
}

TEST(nrpe_mode, a_mode_on_the_command_line_that_matches_the_host_writes_nothing) {
  EXPECT_TRUE(run_install(with_insecure("true"), kModeLegacy).written.empty());
  EXPECT_TRUE(run_install(with_verify_mode("peer-cert"), kModeSecure).written.empty());
}

// ---------------------------------------------------------------------------
// The property pair itself
// ---------------------------------------------------------------------------

TEST(nrpe_mode, an_unconfigured_host_leaves_both_properties_alone) {
  const installer::nrpe::recorded_properties props = record_mode(mode::unconfigured, kModeSecure, false);

  EXPECT_FALSE(props.record);
}

TEST(nrpe_mode, a_recognised_mode_records_itself_as_both_key_and_default) {
  const installer::nrpe::recorded_properties props = record_mode(mode::legacy, kModeSecure, false);

  EXPECT_TRUE(props.record);
  EXPECT_EQ(kModeLegacy, props.key);
  EXPECT_EQ(kModeLegacy, props.default_);
}

TEST(nrpe_mode, a_custom_configuration_records_the_key_it_found) {
  const installer::nrpe::recorded_properties props = record_mode(mode::custom, kModeSecure, false);

  EXPECT_TRUE(props.record);
  EXPECT_EQ(kModeSecure, props.key);
  EXPECT_EQ(kModeSecure, props.default_) << "DEFAULT_ must match KEY_ or the preset is written over the operator's keys";
}

TEST(nrpe_mode, a_custom_configuration_defers_to_a_named_mode) {
  const installer::nrpe::recorded_properties props = record_mode(mode::custom, kModeSecure, true);

  EXPECT_TRUE(props.record);
  EXPECT_EQ("", props.default_) << "an empty DEFAULT_ is what lets either named mode differ from it";
}
