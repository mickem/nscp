// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Unit tests for the CheckHelpers module class around its alias table: the
// aliases loadModuleEx reads from [/settings/check helpers/alias], what it
// registers with the core, and what a reload does to both.

#include "CheckHelpers.h"

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/test_helpers.hpp>
#include <string>
#include <utility>
#include <vector>

// Test binaries have no generated module glue, so the plugin singleton
// (normally provided by NSC_WRAP_DLL()) must be defined here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

namespace {

class CheckHelpersModule : public ::testing::Test {
 protected:
  nscapi::test_helpers::stub_core &core() { return nscapi::test_helpers::stub_core::instance(); }

  void SetUp() override {
    core().reset();
    module_.set_id(42);
  }
  void TearDown() override { core().reset(); }

  void configure_aliases(const std::vector<std::pair<std::string, std::string> > &aliases) { core().set_keys("alias", aliases); }

  bool load() { return module_.loadModuleEx("", NSCAPI::dontStart); }

  // Dispatch a command name through the alias fallback and return the message.
  // An alias that is found is forwarded to the core, which the stub does not
  // implement, so that path ends in an exception: reported as such here, and
  // distinct from the "No alias found" answer an unknown name gets.
  std::string resolve(const std::string &command) {
    PB::Commands::QueryRequestMessage request_message;
    PB::Commands::QueryRequestMessage::Request request;
    request.set_command(command);
    PB::Commands::QueryResponseMessage::Response response;
    try {
      module_.query_fallback(request, &response, request_message);
    } catch (const std::exception &e) {
      return std::string("forwarded to the core: ") + e.what();
    }
    return response.lines_size() > 0 ? response.lines(0).message() : "";
  }

  CheckHelpers module_;
};

}  // namespace

TEST_F(CheckHelpersModule, LoadRegistersEveryAliasWithTheCore) {
  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_warn", "check_warning"}});
  ASSERT_TRUE(load());

  EXPECT_TRUE(core().has_command("my_ok"));
  EXPECT_TRUE(core().has_command("my_warn"));
}

TEST_F(CheckHelpersModule, AnUnknownCommandIsReportedAsNoAlias) {
  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("no_such_alias"), "No alias found matching: no_such_alias");
}

// A reload runs loadModuleEx again on the live module. The alias table used to
// be added to in place, so an alias removed from the ini kept resolving until
// the service was restarted.
TEST_F(CheckHelpersModule, ReloadDropsAnAliasRemovedFromTheConfiguration) {
  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_gone", "check_ok message=gone"}});
  ASSERT_TRUE(load());
  ASSERT_NE(resolve("my_gone"), "No alias found matching: my_gone");

  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());

  EXPECT_EQ(resolve("my_gone"), "No alias found matching: my_gone");
  EXPECT_NE(resolve("my_ok"), "No alias found matching: my_ok");
  // Retracted from the core as well, so the name no longer routes here.
  EXPECT_FALSE(core().has_command("my_gone"));
  EXPECT_TRUE(core().has_command("my_ok"));
}

TEST_F(CheckHelpersModule, ReloadPicksUpAnAliasAddedToTheConfiguration) {
  configure_aliases({{"my_ok", "check_ok message=fine"}});
  ASSERT_TRUE(load());
  ASSERT_EQ(resolve("my_new"), "No alias found matching: my_new");

  configure_aliases({{"my_ok", "check_ok message=fine"}, {"my_new", "check_ok message=new"}});
  ASSERT_TRUE(load());

  EXPECT_NE(resolve("my_new"), "No alias found matching: my_new");
  EXPECT_TRUE(core().has_command("my_new"));
}
