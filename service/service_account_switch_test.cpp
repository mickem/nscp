// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <list>
#include <stdexcept>
#include <string>
#include <vector>

#include "service_account_switch.hpp"

namespace sw = nsclient::core::service_account_switch;
namespace sa = nscp::service_account;
using nscp::paths::layout;

// Changing the account a service runs as touches three places that cannot be
// changed together: the service registration, the grant on the shared folder
// and the record in boot.ini. What these tests pin is the order, because the
// order is what decides which half-finished states can exist.
class ServiceAccountSwitchTest : public ::testing::Test {
 protected:
  std::vector<std::string> calls_;
  std::vector<std::string> info_;
  std::vector<std::string> errors_;
  bool installed_ = true;
  bool protect_succeeds_ = true;
  std::string failing_step_;
  std::string failing_group_;

  void step(const std::string &name) {
    calls_.push_back(name);
    if (name.compare(0, failing_step_.size(), failing_step_) == 0 && !failing_step_.empty()) throw std::runtime_error("access is denied");
  }

  sw::operations scripted() {
    sw::operations ops;
    ops.is_installed = [this](const std::string &) { return installed_; };
    ops.protect = [this](const std::string &, const std::vector<std::string> &sids, std::list<std::string> &errors) {
      calls_.push_back(sids.empty() ? "protect()" : "protect(" + sids.front() + ")");
      if (!protect_succeeds_) errors.push_back("SetSecurityInfo failed: error=5");
      return protect_succeeds_;
    };
    ops.record = [this](const std::string &key, const std::string &value) { step("record " + key + "=" + value); };
    ops.set_account = [this](const std::string &, const std::string &account) { step("account " + account); };
    ops.set_unrestricted_sid = [this](const std::string &) { step("sid-type"); };
    ops.set_required_privileges = [this](const std::string &, const std::vector<std::string> &privileges) {
      step("privileges " + std::to_string(privileges.size()));
    };
    ops.set_group_membership = [this](const std::string &, const std::string &group, const bool add, std::string &error) {
      calls_.push_back(std::string(add ? "join " : "leave ") + group);
      if (group == failing_group_) {
        error = "the group does not exist on this host";
        return false;
      }
      return true;
    };
    ops.info = [this](const std::string &line) { info_.push_back(line); };
    ops.error = [this](const std::string &line) { errors_.push_back(line); };
    return ops;
  }

  static sw::request want(const sa::kind wanted, const layout on = layout::modern) {
    sw::request what;
    what.wanted = wanted;
    what.service_name = "nscp";
    what.layout = on;
    what.shared_folder = "C:\\ProgramData\\NSClient++";
    return what;
  }

  int index_of(const std::string &prefix) const {
    for (std::size_t i = 0; i < calls_.size(); i++) {
      if (calls_[i].compare(0, prefix.size(), prefix) == 0) return static_cast<int>(i);
    }
    return -1;
  }

  bool said(const std::vector<std::string> &lines, const std::string &needle) const {
    for (const std::string &line : lines) {
      if (line.find(needle) != std::string::npos) return true;
    }
    return false;
  }
};

TEST_F(ServiceAccountSwitchTest, TheVirtualAccountIsRefusedOnTheLegacyLayoutBeforeAnythingIsTouched) {
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account, layout::legacy), scripted()), sw::outcome::refused);
  EXPECT_TRUE(calls_.empty());
  EXPECT_TRUE(said(errors_, "nscp settings --migrate-layout modern"));
}

TEST_F(ServiceAccountSwitchTest, AServiceThatIsNotInstalledIsRefused) {
  installed_ = false;
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::refused);
  EXPECT_TRUE(calls_.empty());
  EXPECT_TRUE(said(errors_, "--install --account virtual"));
}

TEST_F(ServiceAccountSwitchTest, TowardsVirtualTheFolderAndTheRecordComeBeforeTheService) {
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::switched);

  const std::string sid = sa::service_sid("nscp");
  ASSERT_EQ(index_of("protect(" + sid + ")"), 0) << "the folder has to be open to the account before the service becomes it";
  const int recorded = index_of("record account=virtual");
  const int became = index_of("account NT SERVICE\\nscp");
  ASSERT_GE(recorded, 0);
  ASSERT_GE(became, 0);
  // A service registered as the virtual account while boot.ini still says
  // LocalSystem is the one state that must not exist: the next elevated nscp
  // would remove the grant.
  EXPECT_LT(recorded, became);
  EXPECT_LT(index_of("record name=nscp"), became);
  // Re-registering as the account is the last registration change, so a
  // failure in the two before it leaves a working LocalSystem service.
  EXPECT_LT(index_of("sid-type"), became);
  EXPECT_LT(index_of("privileges 1"), became);
  // Groups only once there is an account worth adding.
  EXPECT_GT(index_of("join S-1-5-32-558"), became);
  EXPECT_GE(index_of("join S-1-5-32-559"), 0);
  EXPECT_GE(index_of("join S-1-5-32-573"), 0);
  EXPECT_TRUE(errors_.empty());
  EXPECT_TRUE(said(info_, "Restart the service"));
}

TEST_F(ServiceAccountSwitchTest, AFolderThatCannotBeOpenedStopsEverything) {
  protect_succeeds_ = false;
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::failed);
  EXPECT_EQ(calls_.size(), 1u) << "nothing after the failed lockdown may run";
  EXPECT_TRUE(said(errors_, "still runs as LocalSystem"));
}

TEST_F(ServiceAccountSwitchTest, ARecordThatCannotBeWrittenLeavesTheServiceAlone) {
  failing_step_ = "record";
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::failed);
  EXPECT_EQ(index_of("account"), -1) << "the service must not become the account when boot.ini does not say so";
  EXPECT_EQ(index_of("sid-type"), -1);
  EXPECT_TRUE(said(errors_, "still runs as LocalSystem"));
}

TEST_F(ServiceAccountSwitchTest, AFailedRegistrationSaysHowToUndoIt) {
  failing_step_ = "privileges";
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::failed);
  EXPECT_EQ(index_of("account"), -1);
  EXPECT_EQ(index_of("join"), -1);
  EXPECT_TRUE(said(errors_, "nscp service --account system"));
}

TEST_F(ServiceAccountSwitchTest, AMissingGroupIsReportedButDoesNotFailTheSwitch) {
  // Event Log Readers does not exist on every version of Windows.
  failing_group_ = "S-1-5-32-573";
  EXPECT_EQ(sw::apply(want(sa::kind::virtual_account), scripted()), sw::outcome::switched);
  EXPECT_TRUE(said(errors_, "does not exist on this host"));
  EXPECT_GE(index_of("join S-1-5-32-558"), 0);
  EXPECT_GE(index_of("join S-1-5-32-559"), 0);
}

TEST_F(ServiceAccountSwitchTest, BackToSystemTheServiceComesFirstAndTheGrantLast) {
  EXPECT_EQ(sw::apply(want(sa::kind::system), scripted()), sw::outcome::switched);

  ASSERT_EQ(index_of("account LocalSystem"), 0) << "once the service is SYSTEM again it can read the folder whatever else happens";
  // An empty list gives the service its account's default privileges back.
  EXPECT_EQ(index_of("privileges 0"), 1);
  const int recorded = index_of("record account=system");
  const int revoked = index_of("protect()");
  ASSERT_GE(recorded, 0);
  ASSERT_GE(revoked, 0);
  // Record before revoke: with the order reversed, a failed record would
  // leave boot.ini promising a grant that is no longer there.
  EXPECT_LT(recorded, revoked);
  EXPECT_EQ(revoked, static_cast<int>(calls_.size()) - 1);
  EXPECT_GE(index_of("leave S-1-5-32-558"), 0);
  EXPECT_GE(index_of("leave S-1-5-32-559"), 0);
  EXPECT_GE(index_of("leave S-1-5-32-573"), 0);
  EXPECT_TRUE(errors_.empty());
}

TEST_F(ServiceAccountSwitchTest, BackToSystemLeavesTheFolderAloneWhenTheServiceCouldNotBeChanged) {
  failing_step_ = "account";
  EXPECT_EQ(sw::apply(want(sa::kind::system), scripted()), sw::outcome::failed);
  EXPECT_EQ(calls_.size(), 1u) << "a service still running as the virtual account must keep its access";
}

TEST_F(ServiceAccountSwitchTest, SystemOnTheLegacyLayoutNeverTouchesTheInstallFolder) {
  // Nothing was ever granted there, and locking Program Files down to SYSTEM
  // and Administrators is not this command's business.
  EXPECT_EQ(sw::apply(want(sa::kind::system, layout::legacy), scripted()), sw::outcome::switched);
  EXPECT_EQ(index_of("protect"), -1);
}
