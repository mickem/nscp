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

#include <boost/filesystem.hpp>
#include <fstream>
#include <map>
#include <memory>
#include <settings/test_helpers.hpp>
#include <string>

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
  }

  const std::map<std::string, std::string> &overrides() const { return overrides_; }
  int apply_count() const { return apply_count_; }

 private:
  std::string boot_ini_path_;
  nsclient::logging::logger_instance logger_;
  std::map<std::string, std::string> overrides_;
  int apply_count_ = 0;
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
  // Constructor defaults are "1.3" / "none" / "". A boot.ini with no [tls]
  // section must leave them intact.
  write_boot_ini("");

  settings_manager::NSCSettingsImpl impl(provider_.get());
  impl.boot("");

  EXPECT_EQ(impl.get_tls_version(), "1.3");
  EXPECT_EQ(impl.get_tls_verify_mode(), "none");
  EXPECT_EQ(impl.get_tls_ca(), "");
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

}  // namespace
