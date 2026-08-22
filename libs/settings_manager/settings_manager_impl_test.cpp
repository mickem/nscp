// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for libs/settings_manager/settings_manager_impl.{h,cpp}.
//
// Focus:
//  - expand_context: pure mapping of short protocol names to default URLs.
//  - boot(): parsing of boot.ini sections, in particular the [paths] section
//    added by Phase 1.5 of the shared-folder migration. We build a tiny
//    boot.ini in a temp dir, point a mock provider_interface at it, and
//    assert the right callbacks fire with the right values.

#include "settings_manager_impl.h"

#include <config.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <boost/asio/ip/host_name.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <settings/test_helpers.hpp>
#include <str/utils.hpp>
#include <string>
#include <vector>

namespace {

// Mock provider that
//  - hands back a caller-controlled path for any expand_path call (the test
//    arranges this to be a temp boot.ini), and
//  - records every apply_path_overrides invocation so tests can assert on it.
class recording_provider : public settings_manager::provider_interface {
 public:
  explicit recording_provider(std::string boot_ini_path) : boot_ini_path_(std::move(boot_ini_path)), logger_(settings_test::make_null_logger()) {}

  std::string expand_path(std::string) override { return boot_ini_path_; }
  nsclient::logging::logger_instance get_logger() const override { return logger_; }
  void apply_path_overrides(std::map<std::string, std::string> overrides) override {
    overrides_ = std::move(overrides);
    apply_count_++;
    calls_.push_back("paths");
  }
  void apply_layout(const std::string &mode) override {
    layout_mode_ = mode;
    layout_count_++;
    calls_.push_back("layout");
  }
  void prepare_shared_folder() override { calls_.push_back("shared-folder"); }

  const std::map<std::string, std::string> &overrides() const { return overrides_; }
  int apply_count() const { return apply_count_; }
  const std::string &layout_mode() const { return layout_mode_; }
  int layout_count() const { return layout_count_; }
  // The order the hooks fired in, which is load-bearing: see the ordering tests.
  const std::vector<std::string> &calls() const { return calls_; }

 private:
  std::string boot_ini_path_;
  nsclient::logging::logger_instance logger_;
  std::map<std::string, std::string> overrides_;
  int apply_count_ = 0;
  std::string layout_mode_;
  int layout_count_ = 0;
  std::vector<std::string> calls_;
};

// Most boot() tests want a single boot.ini under a unique temp dir plus a
// matching provider. The fixture also wraps each test in destroy_settings to
// keep the static settings_impl pointer clean across tests.
class SettingsManagerBootTest : public ::testing::Test {
 protected:
  settings_test::temp_dir dir_;
  boost::filesystem::path boot_ini_;
  std::unique_ptr<recording_provider> provider_;

  void SetUp() override {
    boot_ini_ = dir_.file("boot.ini");
    provider_ = std::make_unique<recording_provider>(boot_ini_.string());
  }

  void TearDown() override { settings_manager::destroy_settings(); }

  // Write a boot.ini with the given body. Tests append a `[settings] 1=dummy`
  // pointer so boot() can complete via the no-op dummy backend without
  // touching any real configuration files on disk.
  void write_boot_ini(const std::string &body) { settings_test::write_file(boot_ini_, body + "\n[settings]\n1=dummy\n"); }
};

// ---------------------------------------------------------------------------
// expand_context
// ---------------------------------------------------------------------------

TEST(SettingsManagerExpandContext, IniMapsToDefaultIniLocation) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  EXPECT_EQ(impl.expand_context("ini"), DEFAULT_CONF_INI_LOCATION);
}

TEST(SettingsManagerExpandContext, DummyMapsToDummyUrl) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  EXPECT_EQ(impl.expand_context("dummy"), "dummy://");
}

TEST(SettingsManagerExpandContext, UnknownKeyPassesThrough) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  // Anything not recognised - including fully-formed URLs - should round-trip.
  EXPECT_EQ(impl.expand_context("ini://D:/somewhere.ini"), "ini://D:/somewhere.ini");
  EXPECT_EQ(impl.expand_context(""), "");
}

// --- issue #458: host name placeholders in a settings context ---------------

TEST(SettingsManagerExpandContext, ExpandsHostNamePlaceholders) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  // What [/includes] hands to create_instance, so one shared configuration can
  // pull in a per-host file.
  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;
  EXPECT_EQ(impl.expand_context("${host}-nsclient.ini"), host + "-nsclient.ini");
  EXPECT_EQ(impl.expand_context("${hostname}.ini"), boost::asio::ip::host_name() + ".ini");
  EXPECT_EQ(impl.expand_context("ini://${shared-path}/${host}.ini"), "ini://${shared-path}/" + host + ".ini");
}

TEST(SettingsManagerExpandContext, LeavesPathTokensToThePathManager) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  // The two kinds of placeholder share a syntax; this pass must not consume
  // a token that expand_path is going to resolve later.
  EXPECT_EQ(impl.expand_context("ini://${shared-path}/nsclient.ini"), "ini://${shared-path}/nsclient.ini");
}

TEST(SettingsManagerExpandContext, DoesNotResolveTheAutoShorthand) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  // A context named "auto" is a name. Only the submit clients' host name specs
  // give "auto" its other meaning, which is why this does not go through
  // expand_hostname.
  EXPECT_EQ(impl.expand_context("auto"), "auto");
}

#ifdef WIN32
TEST(SettingsManagerExpandContext, WindowsAliases) {
  settings_test::temp_dir dir;
  const auto boot_ini = dir.file("boot.ini");
  recording_provider p(boot_ini.string());
  settings_manager::NSCSettingsImpl impl(&p);

  EXPECT_EQ(impl.expand_context("old"), DEFAULT_CONF_OLD_LOCATION);
  EXPECT_EQ(impl.expand_context("registry"), DEFAULT_CONF_REG_LOCATION);
  EXPECT_EQ(impl.expand_context("reg"), DEFAULT_CONF_REG_LOCATION);
}
#endif

// ---------------------------------------------------------------------------
// boot() - [paths] section (Phase 1.5)
// ---------------------------------------------------------------------------

TEST_F(SettingsManagerBootTest, MissingBootIniDoesNotApplyOverrides) {
  // No file written. boot() should complete without calling apply_path_overrides.
  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("dummy");

  EXPECT_EQ(provider_->apply_count(), 0);
  EXPECT_TRUE(provider_->overrides().empty());
}

TEST_F(SettingsManagerBootTest, EmptyPathsSectionDoesNotApplyOverrides) {
  // boot.ini exists but has no [paths] entries. Empty maps are a waste of an
  // apply call - the implementation must skip the push.
  write_boot_ini("");
  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->apply_count(), 0);
}

TEST_F(SettingsManagerBootTest, PathsSectionAppliesAllEntries) {
  write_boot_ini(
      "[paths]\n"
      "shared-path=D:\\custom\\nscp\n"
      "log-path=E:\\logs\\nscp\n"
      "certificate-path=D:\\custom\\nscp\\security\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->apply_count(), 1);
  const auto &ov = provider_->overrides();
  EXPECT_EQ(ov.size(), 3u);
  EXPECT_EQ(ov.at("shared-path"), "D:\\custom\\nscp");
  EXPECT_EQ(ov.at("log-path"), "E:\\logs\\nscp");
  EXPECT_EQ(ov.at("certificate-path"), "D:\\custom\\nscp\\security");
}

TEST_F(SettingsManagerBootTest, PathsSectionSkipsEmptyValues) {
  // An empty value in [paths] is treated as "not overridden" - it must not
  // appear in the map handed to the provider, otherwise downstream lookups
  // would return an empty string instead of falling back to the default.
  write_boot_ini(
      "[paths]\n"
      "shared-path=D:\\nscp\n"
      "log-path=\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->apply_count(), 1);
  const auto &ov = provider_->overrides();
  EXPECT_EQ(ov.size(), 1u);
  EXPECT_EQ(ov.count("log-path"), 0u);
  EXPECT_EQ(ov.at("shared-path"), "D:\\nscp");
}

TEST_F(SettingsManagerBootTest, PathsSectionAcceptsTemplatedValues) {
  // boot.ini values may contain ${var} references - they are passed through
  // verbatim and expanded later by the path resolver. The parser itself does
  // not interpret them.
  write_boot_ini(
      "[paths]\n"
      "certificate-path=${shared-path}/security\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->overrides().at("certificate-path"), "${shared-path}/security");
}

// ---------------------------------------------------------------------------
// boot() - [tls] section
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// [layout] - which on-disk layout this installation uses
// ---------------------------------------------------------------------------

TEST_F(SettingsManagerBootTest, LayoutModeIsHandedToTheProvider) {
  write_boot_ini("[layout]\nmode=modern\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->layout_count(), 1);
  EXPECT_EQ(provider_->layout_mode(), "modern");
}

TEST_F(SettingsManagerBootTest, MissingLayoutSectionAsksForNoParticularLayout) {
  // Every installation that predates the setting. An empty mode means "keep
  // whatever this host already uses" - boot() must not invent "legacy" and
  // hand that down as though the operator had asked for it.
  write_boot_ini("[paths]\nshared-path=/tmp/x\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->layout_count(), 1) << "the hook still fires, with an empty mode";
  EXPECT_EQ(provider_->layout_mode(), "");
}

TEST_F(SettingsManagerBootTest, UnknownLayoutModeIsPassedThroughVerbatim) {
  // boot() warns but does not rewrite: deciding what an unrecognised mode means
  // belongs to one place (nscp::paths::parse_layout), not two.
  write_boot_ini("[layout]\nmode=moderne\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->layout_mode(), "moderne");
}

TEST_F(SettingsManagerBootTest, EmptyLayoutModeIsTreatedAsAbsent) {
  write_boot_ini("[layout]\nmode=\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->layout_mode(), "");
}

TEST_F(SettingsManagerBootTest, LayoutIsAppliedBeforePathOverrides) {
  // The layout decides what ${shared-path} defaults to; an explicit [paths]
  // entry overrides that default. Applying them the other way round would let
  // the layout land on top of the operator's explicit choice.
  write_boot_ini("[layout]\nmode=modern\n[paths]\nshared-path=/tmp/explicit\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  const auto &calls = provider_->calls();
  const auto layout = std::find(calls.begin(), calls.end(), "layout");
  const auto paths = std::find(calls.begin(), calls.end(), "paths");
  ASSERT_NE(layout, calls.end());
  ASSERT_NE(paths, calls.end());
  EXPECT_LT(layout - calls.begin(), paths - calls.begin()) << "layout must be applied before the path overrides";
}

TEST_F(SettingsManagerBootTest, SharedFolderIsPreparedAfterPathOverridesAndBeforeTheStoreOpens) {
  // The folder has to be created and locked down before anything writes into
  // it, and against the operator's *final* paths - so after [paths], and before
  // the settings store is opened (the trust store export writes there).
  write_boot_ini("[layout]\nmode=modern\n[paths]\nshared-path=/tmp/explicit\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  const auto &calls = provider_->calls();
  const auto paths = std::find(calls.begin(), calls.end(), "paths");
  const auto shared = std::find(calls.begin(), calls.end(), "shared-folder");
  ASSERT_NE(paths, calls.end());
  ASSERT_NE(shared, calls.end());
  EXPECT_LT(paths - calls.begin(), shared - calls.begin()) << "the shared folder must be prepared against the final paths";
}

TEST_F(SettingsManagerBootTest, MissingBootIniStillPreparesTheSharedFolder) {
  // A host with no boot.ini gets the default layout, but the folder that layout
  // points at still has to exist and be locked down.
  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("dummy");

  const auto &calls = provider_->calls();
  EXPECT_NE(std::find(calls.begin(), calls.end(), "shared-folder"), calls.end());
}

TEST_F(SettingsManagerBootTest, TlsSectionPopulatesAccessors) {
  write_boot_ini(
      "[tls]\n"
      "version=tlsv1.3\n"
      "verify mode=peer-cert\n"
      "ca=D:\\certs\\root.pem\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_tls_version(), "tlsv1.3");
  EXPECT_EQ(impl.get_tls_verify_mode(), "peer-cert");
  EXPECT_EQ(impl.get_tls_ca(), "D:\\certs\\root.pem");
}

TEST_F(SettingsManagerBootTest, TlsSectionDefaultsWhenAbsent) {
  // A boot.ini with no [tls] section must leave the constructor defaults
  // intact. Those defaults verify the peer against the platform CA bundle:
  // the transport they govern delivers this agent's whole configuration, so
  // "don't check who we are talking to" is not an acceptable default.
  write_boot_ini("");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_tls_version(), "1.3");
  EXPECT_EQ(impl.get_tls_verify_mode(), "peer");
  EXPECT_EQ(impl.get_tls_ca(), "${ca-path}");
}

TEST_F(SettingsManagerBootTest, TlsVerificationCanStillBeDisabledExplicitly) {
  // The insecure mode remains reachable, but only by writing it out - which is
  // the point: `none` can no longer be arrived at by omission.
  write_boot_ini(
      "[tls]\n"
      "verify mode=none\n"
      "ca=\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_tls_verify_mode(), "none");
}

// ---------------------------------------------------------------------------
// boot() - [proxy] section
// ---------------------------------------------------------------------------

TEST_F(SettingsManagerBootTest, ProxySectionPopulatesAccessors) {
  write_boot_ini(
      "[proxy]\n"
      "url=http://proxy.internal:3128\n"
      "no_proxy=localhost,127.0.0.1\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_proxy_url(), "http://proxy.internal:3128");
  EXPECT_EQ(impl.get_no_proxy(), "localhost,127.0.0.1");
}

TEST_F(SettingsManagerBootTest, ProxySectionDefaultsToEmpty) {
  write_boot_ini("");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_proxy_url(), "");
  EXPECT_EQ(impl.get_no_proxy(), "");
}

// ---------------------------------------------------------------------------
// boot() - end-to-end interaction
// ---------------------------------------------------------------------------

TEST_F(SettingsManagerBootTest, SectionsCombineInSingleBoot) {
  // All sections parsed from one boot.ini in one pass. Verifies they don't
  // interfere with each other and that the single CSimpleIni load handles
  // the full file.
  write_boot_ini(
      "[paths]\n"
      "shared-path=D:\\nscp\n"
      "[tls]\n"
      "version=tlsv1.2+\n"
      "[proxy]\n"
      "url=http://p:8080\n");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(provider_->overrides().at("shared-path"), "D:\\nscp");
  EXPECT_EQ(impl.get_tls_version(), "tlsv1.2+");
  EXPECT_EQ(impl.get_proxy_url(), "http://p:8080");
}

// ===========================================================================
// settings_handler_impl tests
//
// settings_handler_impl is abstract; we exercise its concrete behaviour
// through NSCSettingsImpl since that's the only production subclass. Tests
// in this section avoid calling boot() - they construct the handler and
// poke its registry / state directly, which keeps them fast and isolated
// from the file-IO surface of the boot flow.
// ===========================================================================

class SettingsHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<recording_provider> provider_;
  std::unique_ptr<settings_manager::NSCSettingsImpl> impl_;

  void SetUp() override {
    provider_ = std::make_unique<recording_provider>("");
    impl_ = std::make_unique<settings_manager::NSCSettingsImpl>(provider_.get());
  }

  void TearDown() override {
    impl_.reset();
    settings_manager::destroy_settings();
  }
};

// ---------------------------------------------------------------------------
// Lifecycle flags - small but worth pinning down since boot() and the
// settings_handler API both read these.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, FlagsStartFalse) {
  EXPECT_FALSE(impl_->is_ready());
  EXPECT_FALSE(impl_->is_dirty());
  EXPECT_FALSE(impl_->needs_reload());
}

TEST_F(SettingsHandlerTest, ReadyFlagRoundTrip) {
  impl_->set_ready(true);
  EXPECT_TRUE(impl_->is_ready());
  impl_->set_ready(false);
  EXPECT_FALSE(impl_->is_ready());
}

TEST_F(SettingsHandlerTest, DirtyFlagRoundTrip) {
  impl_->set_dirty(true);
  EXPECT_TRUE(impl_->is_dirty());
  impl_->set_dirty(false);
  EXPECT_FALSE(impl_->is_dirty());
}

TEST_F(SettingsHandlerTest, ReloadFlagRoundTrip) {
  impl_->set_reload(true);
  EXPECT_TRUE(impl_->needs_reload());
  impl_->set_reload(false);
  EXPECT_FALSE(impl_->needs_reload());
}

// ---------------------------------------------------------------------------
// base_path - simple round-trip.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, BasePathRoundTrip) {
  impl_->set_base("/opt/nsclient");
  EXPECT_EQ(impl_->get_base().string(), "/opt/nsclient");
}

// ---------------------------------------------------------------------------
// Path / key registration. Registered metadata is what drives
// update_defaults, the settings-doc generation, and the registry-query API,
// so getting the merge semantics right matters.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, RegisterPathStoredAndQueryable) {
  impl_->register_path(0xffff, "/sec", "Section title", "Section description", false, false, true);
  const auto desc = impl_->get_registered_path("/sec");
  EXPECT_EQ(desc.title, "Section title");
  EXPECT_EQ(desc.description, "Section description");
}

TEST_F(SettingsHandlerTest, RegisterPathUpdateExistingMerges) {
  impl_->register_path(1, "/sec", "Original title", "Original description", false, false, true);
  impl_->register_path(2, "/sec", "Updated title", "Updated description", false, false, /*update_existing=*/true);
  const auto desc = impl_->get_registered_path("/sec");
  EXPECT_EQ(desc.title, "Updated title");
}

TEST_F(SettingsHandlerTest, RegisterPathRespectsNoUpdateExisting) {
  impl_->register_path(1, "/sec", "Original title", "Original description", false, false, true);
  impl_->register_path(2, "/sec", "Should not appear", "Neither", false, false, /*update_existing=*/false);
  const auto desc = impl_->get_registered_path("/sec");
  EXPECT_EQ(desc.title, "Original title");
}

TEST_F(SettingsHandlerTest, GetRegisteredPathThrowsForUnknown) {
  EXPECT_THROW(impl_->get_registered_path("/does-not-exist"), settings::settings_exception);
}

TEST_F(SettingsHandlerTest, RegisterKeyStoredWithDescription) {
  impl_->register_key(0xffff, "/sec", "k", "string", "Key title", "Key description", "default-val", false, false, true);
  const auto desc = impl_->get_registered_key("/sec", "k");
  ASSERT_TRUE(desc.has_value());
  EXPECT_EQ(desc->title, "Key title");
  EXPECT_EQ(desc->default_value, "default-val");
}

TEST_F(SettingsHandlerTest, GetRegisteredKeyUnknownReturnsNone) { EXPECT_FALSE(impl_->get_registered_key("/sec", "missing").has_value()); }

TEST_F(SettingsHandlerTest, GetRegisteredKeyModulesHasSyntheticBoolDesc) {
  // /modules has a special-case fallback: any unregistered key under it is
  // treated as a "load on startup" bool. This is what lets users enable
  // modules in nsclient.ini with just `ModuleName = enabled`.
  const auto desc = impl_->get_registered_key("/modules", "AnyModuleName");
  ASSERT_TRUE(desc.has_value());
  EXPECT_EQ(desc->type, "bool");
}

// ---------------------------------------------------------------------------
// Sensitive-key tracking - feeds the Credential-Manager redirection in
// settings_ini.hpp. Wrong answers here mean passwords leak to plaintext INI
// or, conversely, non-secret strings get hidden in cred manager.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, IsSensitiveKeyDefaultsFalse) { EXPECT_FALSE(impl_->is_sensitive_key("/settings/default", "password")); }

TEST_F(SettingsHandlerTest, AddSensitiveKeyMakesItSensitive) {
  impl_->add_sensitive_key(0xffff, "/settings/default", "password");
  EXPECT_TRUE(impl_->is_sensitive_key("/settings/default", "password"));
}

TEST_F(SettingsHandlerTest, SensitiveKeyIsExactPathPlusKey) {
  // The implementation combines path + "|||" + key, so a sensitive flag on
  // "/a"."x" must NOT bleed into "/b"."x" or "/a"."y".
  impl_->add_sensitive_key(0xffff, "/a", "x");
  EXPECT_TRUE(impl_->is_sensitive_key("/a", "x"));
  EXPECT_FALSE(impl_->is_sensitive_key("/b", "x"));
  EXPECT_FALSE(impl_->is_sensitive_key("/a", "y"));
}

// ---------------------------------------------------------------------------
// Section / key enumeration with sample filtering. Sample paths exist for
// docs generation and must not appear in the runtime listing unless the
// caller explicitly asks.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, RegSectionsListsRegisteredPaths) {
  impl_->register_path(0xffff, "/a", "A", "", false, false, true);
  impl_->register_path(0xffff, "/b", "B", "", false, false, true);
  const auto sections = impl_->get_reg_sections("", false);
  EXPECT_EQ(sections.size(), 2u);
}

TEST_F(SettingsHandlerTest, RegSectionsHonoursPathPrefix) {
  impl_->register_path(0xffff, "/foo", "F", "", false, false, true);
  impl_->register_path(0xffff, "/foo/bar", "FB", "", false, false, true);
  impl_->register_path(0xffff, "/baz", "B", "", false, false, true);
  const auto under_foo = impl_->get_reg_sections("/foo", false);
  EXPECT_EQ(under_foo.size(), 2u);
}

TEST_F(SettingsHandlerTest, RegSectionsExcludesSamplesByDefault) {
  impl_->register_path(0xffff, "/real", "R", "", false, /*is_sample=*/false, true);
  impl_->register_path(0xffff, "/sample", "S", "", false, /*is_sample=*/true, true);
  EXPECT_EQ(impl_->get_reg_sections("", false).size(), 1u);
  EXPECT_EQ(impl_->get_reg_sections("", true).size(), 2u);
}

TEST_F(SettingsHandlerTest, RegKeysListsRegisteredKeys) {
  impl_->register_key(0xffff, "/sec", "k1", "string", "", "", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k2", "string", "", "", "", false, false, true);
  EXPECT_EQ(impl_->get_reg_keys("/sec", false).size(), 2u);
}

TEST_F(SettingsHandlerTest, RegKeysExcludesSamplesByDefault) {
  impl_->register_key(0xffff, "/sec", "real", "string", "", "", "", false, /*is_sample=*/false, true);
  impl_->register_key(0xffff, "/sec", "sample", "string", "", "", "", false, /*is_sample=*/true, true);
  EXPECT_EQ(impl_->get_reg_keys("/sec", false).size(), 1u);
  EXPECT_EQ(impl_->get_reg_keys("/sec", true).size(), 2u);
}

// ---------------------------------------------------------------------------
// Templates - one place per template, recovered on enumeration.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, RegisterTplStoresAndReturns) {
  impl_->register_tpl(0xffff, "/sec", "title-a", "payload-a");
  impl_->register_tpl(0xffff, "/sec", "title-b", "payload-b");
  const auto tpls = impl_->get_registered_templates();
  EXPECT_EQ(tpls.size(), 2u);
}

// ---------------------------------------------------------------------------
// Instance access. get() / get_no_wait() must throw when no instance has
// been installed - otherwise callers silently dereference a null shared_ptr.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, GetThrowsBeforeInstance) { EXPECT_THROW(impl_->get(), settings::settings_exception); }

TEST_F(SettingsHandlerTest, GetNoWaitThrowsBeforeInstance) { EXPECT_THROW(impl_->get_no_wait(), settings::settings_exception); }

TEST_F(SettingsHandlerTest, SetInstanceMakesGetReturn) {
  impl_->set_instance("master", "dummy");
  const auto inst = impl_->get();
  ASSERT_TRUE(static_cast<bool>(inst));
}

// ===========================================================================
// update_defaults / remove_defaults
//
// These two are what `nscp settings --add-defaults` and `--remove-defaults`
// do to a live store: the first materialises every registered non-advanced
// key at its default value, the second strips back out whatever still sits at
// that default. Together they decide what a shipped nsclient.ini looks like,
// and the half that matters most is the one that leaves an operator's own
// value alone.
//
// Backed by a real INI store in a temp dir rather than the dummy backend -
// dummy discards everything written to it, which would make every assertion
// below vacuously true.
// ===========================================================================

// recording_provider answers every expand_path() with its boot.ini, which is
// what the boot() tests need but breaks anything that resolves a real path:
// INISettings runs its file name through the same hook, so loading, saving
// and context_exists() would all end up looking at the boot.ini instead.
class passthrough_provider : public recording_provider {
 public:
  passthrough_provider() : recording_provider("") {}
  std::string expand_path(std::string file) override { return file; }
};

// Build an "ini:///<absolute path>" context - three slashes so net::parse
// leaves the host empty and hands the whole absolute path to url.path.
std::string ini_context(const boost::filesystem::path &p) {
  std::string s = p.generic_string();
  if (!s.empty() && s.front() == '/') s.erase(0, 1);
  return "ini:///" + s;
}

class SettingsDefaultsTest : public SettingsHandlerTest {
 protected:
  settings_test::temp_dir store_dir_;
  std::unique_ptr<passthrough_provider> store_provider_;

  void SetUp() override {
    store_provider_ = std::make_unique<passthrough_provider>();
    impl_ = std::make_unique<settings_manager::NSCSettingsImpl>(store_provider_.get());
    // INISettings resolves its context against an existing file (see the note
    // in settings_ini_test), so the file has to be there before we point at
    // it - empty is fine.
    const boost::filesystem::path file = store_dir_.file("store.ini");
    settings_test::write_file(file, "");
    impl_->set_instance("master", ini_context(file));
  }
};

// Same provider, no store: for the context-dispatch tests, which resolve
// paths but never open an instance.
class SettingsContextTest : public SettingsHandlerTest {
 protected:
  std::unique_ptr<passthrough_provider> path_provider_;

  void SetUp() override {
    path_provider_ = std::make_unique<passthrough_provider>();
    impl_ = std::make_unique<settings_manager::NSCSettingsImpl>(path_provider_.get());
  }
};

TEST_F(SettingsDefaultsTest, UpdateDefaultsMaterialisesRegisteredKeys) {
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);

  impl_->update_defaults();

  EXPECT_EQ(impl_->get()->get_string("/sec", "k", ""), "the-default");
}

TEST_F(SettingsDefaultsTest, UpdateDefaultsSkipsAdvancedKeys) {
  // "advanced" is the flag that keeps rarely-touched keys out of a fresh ini.
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", /*advanced=*/true, false, true);

  impl_->update_defaults();

  EXPECT_FALSE(impl_->get()->has_key("/sec", "k"));
}

TEST_F(SettingsDefaultsTest, UpdateDefaultsLeavesAnOperatorsValueAlone) {
  // The whole point of the has_key branch: --add-defaults must not reset
  // configuration somebody deliberately changed.
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);
  impl_->get()->set_string("/sec", "k", "operator-value");

  impl_->update_defaults();

  EXPECT_EQ(impl_->get()->get_string("/sec", "k", ""), "operator-value");
}

TEST_F(SettingsDefaultsTest, UpdateDefaultsCreatesRegisteredSectionsWithoutKeys) {
  impl_->register_path(0xffff, "/empty-sec", "S", "", false, false, true);

  impl_->update_defaults();

  EXPECT_TRUE(impl_->get()->has_section("/empty-sec"));
}

TEST_F(SettingsDefaultsTest, UpdateDefaultsIgnoresSamplePaths) {
  // Sample sections document what a config could look like; writing them into
  // the live store would enable configuration nobody asked for.
  impl_->register_path(0xffff, "/sample", "S", "", false, /*is_sample=*/true, true);
  impl_->register_key(0xffff, "/sample", "k", "string", "", "", "d", false, /*is_sample=*/true, true);

  impl_->update_defaults();

  EXPECT_FALSE(impl_->get()->has_key("/sample", "k"));
}

TEST_F(SettingsDefaultsTest, RemoveDefaultsDropsKeysLeftAtTheirDefault) {
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);
  impl_->get()->set_string("/sec", "k", "the-default");

  impl_->remove_defaults();

  EXPECT_FALSE(impl_->get()->has_key("/sec", "k"));
}

TEST_F(SettingsDefaultsTest, RemoveDefaultsKeepsCustomisedKeys) {
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);
  impl_->get()->set_string("/sec", "k", "operator-value");

  impl_->remove_defaults();

  EXPECT_EQ(impl_->get()->get_string("/sec", "k", ""), "operator-value");
}

TEST_F(SettingsDefaultsTest, RemoveDefaultsDropsTheSectionOnceItIsEmpty) {
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);
  impl_->get()->set_string("/sec", "k", "the-default");

  impl_->remove_defaults();

  EXPECT_FALSE(impl_->get()->has_section("/sec"));
}

TEST_F(SettingsDefaultsTest, RemoveDefaultsKeepsASectionThatStillHasKeys) {
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "default-key", "string", "", "", "the-default", false, false, true);
  impl_->register_key(0xffff, "/sec", "custom-key", "string", "", "", "the-default", false, false, true);
  impl_->get()->set_string("/sec", "default-key", "the-default");
  impl_->get()->set_string("/sec", "custom-key", "operator-value");

  impl_->remove_defaults();

  EXPECT_TRUE(impl_->get()->has_section("/sec"));
  EXPECT_EQ(impl_->get()->get_string("/sec", "custom-key", ""), "operator-value");
}

TEST_F(SettingsDefaultsTest, DefaultsRoundTripLeavesNothingBehind) {
  // Adding every default and then removing every default should land back on
  // the store we started with.
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "the-default", false, false, true);

  impl_->update_defaults();
  impl_->remove_defaults();

  EXPECT_FALSE(impl_->get()->has_key("/sec", "k"));
  EXPECT_FALSE(impl_->get()->has_section("/sec"));
}

// ---------------------------------------------------------------------------
// Instance-backed accessors. Each of these dereferences the settings instance,
// so the interesting case is what they do before boot() installs one.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, HouseKeepingThrowsBeforeAnInstanceExists) {
  // scheduler::handle_settings drives this on a timer, which can fire before
  // boot() has installed an instance or after shutdown destroyed it. It used
  // to dereference the null instance; the scheduler thread catches exceptions
  // but cannot catch a segfault.
  EXPECT_THROW(impl_->house_keeping(), settings::settings_exception);
}

TEST_F(SettingsHandlerTest, HouseKeepingRunsOnALiveInstance) {
  impl_->set_instance("master", "dummy");
  EXPECT_NO_THROW(impl_->house_keeping());
}

TEST_F(SettingsHandlerTest, SupportsUpdatesThrowsBeforeAnInstanceExists) { EXPECT_THROW(impl_->supports_updates(), settings::settings_exception); }

TEST_F(SettingsHandlerTest, SupportsUpdatesFollowsTheBackend) {
  // The dummy backend swallows writes, so it must report itself read-only.
  impl_->set_instance("master", "dummy");
  EXPECT_FALSE(impl_->supports_updates());
}

TEST_F(SettingsDefaultsTest, SupportsUpdatesIsTrueForAnIniStore) { EXPECT_TRUE(impl_->supports_updates()); }

// validate() is what `nscp settings --validate` reports. It has to run against
// configuration read from disk: writing a key through the store auto-registers
// its section as "in flight" (settings_interface_impl::setter does), so a
// section created in the same process is never unregistered by the time
// validate() looks.
TEST_F(SettingsContextTest, ValidateReportsSectionsNobodyRegistered) {
  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("stray.ini");
  settings_test::write_file(file, "[/nobody-registered-this]\nk = v\n");
  impl_->set_instance("master", ini_context(file));

  EXPECT_FALSE(impl_->validate().empty());
}

TEST_F(SettingsContextTest, ValidateIsQuietForRegisteredConfiguration) {
  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("known.ini");
  settings_test::write_file(file, "[/sec]\nk = v\n");
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->register_key(0xffff, "/sec", "k", "string", "", "", "", false, false, true);
  impl_->set_instance("master", ini_context(file));

  EXPECT_TRUE(impl_->validate().empty());
}

TEST_F(SettingsContextTest, ValidateDoesNotFlagUnregisteredKeys) {
  // CHARACTERIZATION TEST - this pins current behaviour, which is wrong.
  //
  // settings_ini::validate() spots an unregistered key by catching an
  // exception out of get_registered_key(). The production core does not throw
  // for an unknown key, it returns boost::none (settings_handler_impl), so
  // that half of --validate never fires and a typo in a key name is reported
  // by nothing. Only the test core in test_helpers.hpp throws, which is why
  // settings_ini_test's own validate test sees the errors this one cannot.
  //
  // See the FIXME in settings_ini.hpp: the fix is to test the optional rather
  // than wait for an exception, but it widens what --validate prints (every
  // key of every module that is not currently loaded), so it wants its own
  // change.
  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("typo.ini");
  settings_test::write_file(file, "[/sec]\nunregistered-key = v\n");
  impl_->register_path(0xffff, "/sec", "S", "", false, false, true);
  impl_->set_instance("master", ini_context(file));

  EXPECT_TRUE(impl_->validate().empty()) << "documenting the defect: the unknown key is not reported";
}

TEST_F(SettingsDefaultsTest, UseSensitiveKeysDefaultsToFalse) { EXPECT_FALSE(impl_->use_sensitive_keys()); }

TEST_F(SettingsDefaultsTest, UseSensitiveKeysFollowsTheSetting) {
  // Turning this on is what redirects passwords into the Windows credential
  // manager instead of writing them into the ini in clear text.
  impl_->get()->set_string("/settings", "use credential manager", "true");
  EXPECT_TRUE(impl_->use_sensitive_keys());
}

// ---------------------------------------------------------------------------
// Context dispatch: which backend a context string selects, whether it can be
// edited, and whether it is already there. get_context/--migrate-to and the
// settings web UI all branch on these.
// ---------------------------------------------------------------------------

TEST_F(SettingsHandlerTest, SupportsEditAcceptsAnEmptyContext) {
  // Empty means "whatever is configured", which the caller resolves later.
  EXPECT_TRUE(impl_->supports_edit(""));
}

TEST_F(SettingsHandlerTest, SupportsEditIsTrueForIni) { EXPECT_TRUE(impl_->supports_edit("ini:///tmp/whatever.ini")); }

TEST_F(SettingsHandlerTest, SupportsEditIsFalseForReadOnlyBackends) {
  EXPECT_FALSE(impl_->supports_edit("dummy"));
  EXPECT_FALSE(impl_->supports_edit("http://localhost/settings"));
  EXPECT_FALSE(impl_->supports_edit("https://localhost/settings"));
}

TEST_F(SettingsHandlerTest, SupportsEditIsFalseForAnUnknownProtocol) { EXPECT_FALSE(impl_->supports_edit("gopher://localhost/settings")); }

TEST_F(SettingsContextTest, ContextExistsFollowsTheFileForIni) {
  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("there.ini");
  settings_test::write_file(file, "");

  EXPECT_TRUE(impl_->context_exists(ini_context(file)));
  EXPECT_FALSE(impl_->context_exists(ini_context(file) + ".missing"));
}

TEST_F(SettingsHandlerTest, ContextExistsIsAlwaysTrueForDummyAndHttp) {
  // Neither has anything to check up front: dummy has no storage at all and
  // an http store is only reachable once the daemon is running.
  EXPECT_TRUE(impl_->context_exists("dummy"));
  EXPECT_TRUE(impl_->context_exists("http://localhost/settings"));
}

TEST_F(SettingsHandlerTest, CreateInstanceThrowsForAnUnknownProtocol) {
  EXPECT_THROW(impl_->create_instance("master", "gopher://localhost/settings"), settings::settings_exception);
}

// --- issue #458: a stored context keeps its placeholder ---------------------

TEST_F(SettingsManagerBootTest, SwitchingContextKeepsAPlaceholderInBootIni) {
  // set_primary reorders boot.ini and writes every entry back. The whole point
  // of a placeholder is that the same boot.ini works on every machine, so it
  // has to survive the round trip - expanding it here would replace the
  // template with the name of whichever host happened to switch context.
  settings_test::write_file(boot_ini_, "[settings]\n1=ini://${host}-nsclient.ini\n2=dummy\n");
  ASSERT_TRUE(settings_manager::init_settings(provider_.get(), "dummy"));

  settings_manager::set_boot_ini_primary("dummy");

  const std::string after = settings_test::read_file(boot_ini_);
  EXPECT_NE(after.find("ini://${host}-nsclient.ini"), std::string::npos) << after;
  EXPECT_EQ(after.find(str::utils::getToken(boost::asio::ip::host_name(), '.').first + "-nsclient.ini"), std::string::npos) << after;
}

// --- issue #458: a per-host [/includes] entry -------------------------------

TEST_F(SettingsContextTest, AContextWithAHostPlaceholderResolvesToThePerHostFile) {
  // The end of the chain [/includes] walks: the ini backend hands the value it
  // read straight to create_instance/context_exists, so expanding the
  // placeholder there is what makes
  //
  //   [/includes]
  //   client = ${host}-nsclient.ini
  //
  // open this host's file. Without it the token reached the path manager,
  // which has no answer for it and substitutes the installation directory.
  settings_test::temp_dir dir;
  const std::string host = str::utils::getToken(boost::asio::ip::host_name(), '.').first;
  settings_test::write_file(dir.file(host + "-nsclient.ini"), "[/settings]\nkey = value\n");

  const std::string context = ini_context(dir.path() / "${host}-nsclient.ini");
  EXPECT_TRUE(impl_->context_exists(context));

  const settings::instance_raw_ptr instance = impl_->create_instance("client", context);
  ASSERT_TRUE(static_cast<bool>(instance));
  EXPECT_EQ(instance->get_type(), "ini");
  // get_info() names the file that was actually opened.
  EXPECT_NE(instance->get_info().find(host + "-nsclient.ini"), std::string::npos) << instance->get_info();
  EXPECT_EQ(instance->get_info().find("${host}"), std::string::npos) << instance->get_info();
}

TEST_F(SettingsContextTest, AContextWithoutAPlaceholderIsUnaffected) {
  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("plain.ini");
  settings_test::write_file(file, "");

  EXPECT_TRUE(impl_->context_exists(ini_context(file)));
  EXPECT_FALSE(impl_->context_exists(ini_context(dir.path() / "${host}-plain.ini")));
}

TEST_F(SettingsContextTest, CreateInstanceBuildsTheBackendTheContextNames) {
  const settings::instance_raw_ptr dummy = impl_->create_instance("master", "dummy");
  ASSERT_TRUE(static_cast<bool>(dummy));
  EXPECT_EQ(dummy->get_type(), "dummy");

  settings_test::temp_dir dir;
  const boost::filesystem::path file = dir.file("ctx.ini");
  settings_test::write_file(file, "");
  const settings::instance_raw_ptr ini = impl_->create_instance("master", ini_context(file));
  ASSERT_TRUE(static_cast<bool>(ini));
  EXPECT_EQ(ini->get_type(), "ini");
}

}  // namespace
