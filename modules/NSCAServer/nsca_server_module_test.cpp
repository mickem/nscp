// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the NSCAServer module class - the settings it registers and
// reads in loadModuleEx(), and what it does with a configuration the listener
// cannot start on. The wire format and the packet parsing live in the module's
// other translation units and have their own tests; what is covered here is
// only the plugin shell around them.
//
// The one thing worth pinning is the load-mode split. With encryption enabled
// the server needs a key, and an empty or hashed one is a well-known key, so a
// *start* refuses. But dontStart - `nscp settings`, `nscp client`, the
// documentation extractor - never binds the port and exists to show and repair
// the configuration, so it has to load a module whose configuration is broken:
// otherwise the keys the operator has to fix are the ones that cannot be
// listed. A regression here is invisible in a running agent and only shows up
// as a module missing from the generated reference.
//
// Every load that must succeed uses NSCAPI::dontStart, so no socket is ever
// opened. The refusing loads use normalStart and return before the server is
// created, so they open none either.

#include "NSCAServer.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

// A syntactically valid pbkdf2-sha256 string, as `nscp web password --set`
// writes for the shared default password. NSCA can never use one: the key is
// the clear-text password itself.
const char *const kHashedPassword =
    "pbkdf2-sha256$100000$00112233445566778899aabbccddeeff$0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class NscaServerModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override {
    module_.unloadModule();
    core().reset();
  }

  bool load(const NSCAPI::moduleLoadMode mode = NSCAPI::dontStart) { return module_.loadModuleEx("", mode); }

  NSCAServer module_;
};

}  // namespace

// ============================================================================
// The settings contract
// ============================================================================

TEST_F(NscaServerModule, LoadRegistersItsKeysWithTheDocumentedDefaults) {
  ASSERT_TRUE(load());

  EXPECT_EQ(core().default_for("port"), "5667");
  EXPECT_EQ(core().default_for("payload length"), "512");
  EXPECT_EQ(core().default_for("encryption"), "aes256");
  EXPECT_EQ(core().default_for("timezone"), "utc");
  EXPECT_EQ(core().default_for("inbox"), "inbox");
  EXPECT_EQ(core().default_for("password"), "") << "the key is inherited from nowhere, so it has no default";
}

TEST_F(NscaServerModule, ThePasswordIsRegisteredAsSensitive) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(core().is_sensitive("password"));
}

// ============================================================================
// dontStart loads whatever the configuration says
// ============================================================================

// The default configuration: aes256 and no key. That is exactly what a fresh
// checkout hands the documentation extractor, and it has to get the module.
TEST_F(NscaServerModule, DontStartLoadsWithoutAPassword) { EXPECT_TRUE(load(NSCAPI::dontStart)); }

TEST_F(NscaServerModule, DontStartLoadsWithAHashedPassword) {
  core().set_setting("password", kHashedPassword);
  EXPECT_TRUE(load(NSCAPI::dontStart));
}

TEST_F(NscaServerModule, DontStartLoadsWithAnUnknownEncryption) {
  core().set_setting("encryption", "no-such-cipher");
  EXPECT_TRUE(load(NSCAPI::dontStart));
}

TEST_F(NscaServerModule, DontStartLoadsAValidConfiguration) {
  core().set_setting("password", "the shared key");
  EXPECT_TRUE(load(NSCAPI::dontStart));
  EXPECT_EQ(module_.get_password(), "the shared key");
  EXPECT_NE(module_.get_encryption(), 0) << "aes256 should have resolved to a cipher";
}

// ============================================================================
// A start refuses the same configurations
// ============================================================================

// An empty password is a well-known key. The refusal happens before the
// server object exists, so nothing is bound.
TEST_F(NscaServerModule, StartRefusesWithoutAPassword) { EXPECT_FALSE(load(NSCAPI::normalStart)); }

TEST_F(NscaServerModule, StartRefusesAHashedPassword) {
  core().set_setting("password", kHashedPassword);
  EXPECT_FALSE(load(NSCAPI::normalStart));
}

TEST_F(NscaServerModule, StartRefusesAnUnknownEncryption) {
  core().set_setting("encryption", "no-such-cipher");
  core().set_setting("password", "the shared key");
  EXPECT_FALSE(load(NSCAPI::normalStart));
}

// A reload is a start too: the running listener has been stopped by the time
// the configuration is checked, and it must not come back on a bad key.
TEST_F(NscaServerModule, ReloadRefusesWithoutAPassword) { EXPECT_FALSE(load(NSCAPI::reloadStart)); }

// ============================================================================
// Lifecycle
// ============================================================================

TEST_F(NscaServerModule, UnloadWithoutLoadIsSafe) { EXPECT_TRUE(module_.unloadModule()); }

TEST_F(NscaServerModule, UnloadAfterLoadIsIdempotent) {
  ASSERT_TRUE(load());
  EXPECT_TRUE(module_.unloadModule());
  EXPECT_TRUE(module_.unloadModule());
}
