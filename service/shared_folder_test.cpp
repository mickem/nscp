// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#ifdef WIN32

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "shared_folder.hpp"

namespace sf = nsclient::core::shared_folder;
using nsclient::windows_acl::protection;

namespace {

// Keeps what was logged, by level: half of what this function decides is how
// loud to be.
class recording_logger : public nsclient::logging::log_interface {
 public:
  std::vector<std::string> debugs;
  std::vector<std::string> errors;
  // Anything an operator would see without turning debug logging on.
  std::vector<std::string> visible;

  void trace(const std::string &, const char *, int, const std::string &) override {}
  void debug(const std::string &, const char *, int, const std::string &message) override { debugs.push_back(message); }
  void info(const std::string &, const char *, int, const std::string &message) override { visible.push_back(message); }
  void warning(const std::string &, const char *, int, const std::string &message) override { visible.push_back(message); }
  void error(const std::string &, const char *, int, const std::string &message) override {
    visible.push_back(message);
    errors.push_back(message);
  }
  void critical(const std::string &, const char *, int, const std::string &message) override {
    visible.push_back(message);
    errors.push_back(message);
  }
  bool should_trace() const override { return true; }
  bool should_debug() const override { return true; }
  bool should_info() const override { return true; }
  bool should_warning() const override { return true; }
  bool should_error() const override { return true; }
  bool should_critical() const override { return true; }
};

bool mentions(const std::vector<std::string> &lines, const std::string &text) {
  for (const std::string &line : lines) {
    if (line.find(text) != std::string::npos) return true;
  }
  return false;
}

class SharedFolderTest : public ::testing::Test {
 protected:
  boost::filesystem::path root_;
  boost::filesystem::path shared_;
  std::shared_ptr<recording_logger> log_;
  nsclient::core::path_instance paths_;

  // The scripted folder state, and what was asked of it.
  bool reparse_ = false;
  protection state_ = protection::open;
  bool protect_succeeds_ = true;
  bool protected_after_ = true;
  std::vector<std::string> calls_;

  void SetUp() override {
    root_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-shared-%%%%-%%%%");
    boost::filesystem::create_directories(root_);
    shared_ = root_ / "NSClient++";
    log_ = std::make_shared<recording_logger>();
    paths_ = std::make_shared<nsclient::core::path_manager>(nsclient::logging::log_client_accessor(log_));
    use_layout(nscp::paths::layout::modern);
  }
  void TearDown() override {
    boost::system::error_code ignored;
    boost::filesystem::remove_all(root_, ignored);
  }

  void use_layout(const nscp::paths::layout layout) {
    paths_->set_layout(layout);
    paths_->set_overrides({{"shared-path", shared_.string()}});
  }

  sf::acl_functions scripted() {
    sf::acl_functions acl;
    acl.is_reparse_point = [this](const std::string &, std::list<std::string> &) {
      calls_.push_back("is_reparse_point");
      return reparse_;
    };
    acl.inspect_protection = [this](const std::string &, std::list<std::string> &errors) {
      calls_.push_back("inspect_protection");
      if (state_ == protection::unknown) errors.push_back("GetSecurityInfo failed: error=5");
      return state_;
    };
    acl.protect_directory = [this](const std::string &path, std::list<std::string> &errors) {
      calls_.push_back("protect_directory");
      // The lockdown is applied to a directory that exists: creating it is
      // this function's job, not the ACL layer's.
      EXPECT_TRUE(boost::filesystem::is_directory(path)) << path;
      if (!protect_succeeds_) errors.push_back("SetSecurityInfo(owner) failed: error=1307");
      return protect_succeeds_;
    };
    acl.is_protected = [this](const std::string &, std::list<std::string> &errors) {
      calls_.push_back("is_protected");
      if (!protected_after_) errors.push_back("BUILTIN\\Users still has access");
      return protected_after_;
    };
    return acl;
  }

  sf::outcome prepare() { return sf::prepare(*paths_, log_, scripted()); }

  bool was_called(const std::string &name) const {
    for (const std::string &call : calls_) {
      if (call == name) return true;
    }
    return false;
  }
};

}  // namespace

// --- when it does not apply --------------------------------------------------

TEST_F(SharedFolderTest, TheLegacyLayoutIsLeftEntirelyAlone) {
  // The install directory's permissions are the installer's business.
  use_layout(nscp::paths::layout::legacy);
  EXPECT_EQ(prepare(), sf::outcome::not_applicable);
  EXPECT_TRUE(calls_.empty());
  EXPECT_FALSE(boost::filesystem::exists(shared_));
  EXPECT_TRUE(log_->visible.empty());
}

// --- the one fatal answer ----------------------------------------------------

TEST_F(SharedFolderTest, ALinkWhereTheFolderShouldBeIsRefusedBeforeAnythingElse) {
  reparse_ = true;
  try {
    prepare();
    FAIL() << "the configuration would have been read from behind a link";
  } catch (const settings::settings_exception &e) {
    const std::string what = e.what();
    EXPECT_NE(what.find("Refusing to use " + shared_.string()), std::string::npos) << what;
    EXPECT_NE(what.find("junction or symbolic link"), std::string::npos) << what;
    // The way out, in the same message.
    EXPECT_NE(what.find("boot.ini"), std::string::npos) << what;
  }
  // Nothing was created behind the link, and nothing was secured through it.
  EXPECT_EQ(calls_, std::vector<std::string>({"is_reparse_point"}));
  EXPECT_FALSE(boost::filesystem::exists(shared_));
}

TEST_F(SharedFolderTest, ARealJunctionIsRefused) {
  // The same through the real ACL layer and a real junction, which any account
  // can create (no privilege needed, unlike a symbolic link) - which is the
  // attack.
  const boost::filesystem::path target = root_ / "elsewhere";
  boost::filesystem::create_directories(target);
  const std::string make = "mklink /J \"" + shared_.string() + "\" \"" + target.string() + "\" > nul 2>&1";
  ASSERT_EQ(std::system(make.c_str()), 0) << "could not create a junction to test with";

  EXPECT_THROW(sf::prepare(*paths_, log_), settings::settings_exception);
  EXPECT_TRUE(boost::filesystem::is_empty(target)) << "something was written through the link";

  // Remove the link itself, so the fixture's cleanup cannot follow it.
  const std::string unlink = "rmdir \"" + shared_.string() + "\" > nul 2>&1";
  const int ignored = std::system(unlink.c_str());
  static_cast<void>(ignored);
}

// --- look before acting ------------------------------------------------------

TEST_F(SharedFolderTest, AFolderThatIsAlreadyRestrictedIsNotTouched) {
  boost::filesystem::create_directories(shared_);
  state_ = protection::restricted;
  EXPECT_EQ(prepare(), sf::outcome::already_restricted);
  EXPECT_FALSE(was_called("protect_directory"));
  EXPECT_TRUE(log_->visible.empty());
}

TEST_F(SharedFolderTest, AFolderThatCannotBeInspectedIsLeftToTheServiceQuietly) {
  // What a correctly locked-down folder looks like to an unelevated
  // `nscp client ...`: it has no READ_CONTROL, so it cannot look. Trying the
  // lockdown anyway fails, and used to announce that a folder which was
  // exactly right was readable by every user - on every command.
  boost::filesystem::create_directories(shared_);
  state_ = protection::unknown;
  EXPECT_EQ(prepare(), sf::outcome::cannot_inspect);
  EXPECT_FALSE(was_called("protect_directory"));
  EXPECT_TRUE(log_->visible.empty()) << log_->visible.front();
  // Still findable by someone who goes looking.
  EXPECT_TRUE(mentions(log_->debugs, "GetSecurityInfo failed"));
  EXPECT_TRUE(mentions(log_->debugs, "leaving that to the service"));
}

// --- the lockdown ------------------------------------------------------------

TEST_F(SharedFolderTest, AnOpenFolderIsLockedDownAndVerified) {
  boost::filesystem::create_directories(shared_);
  state_ = protection::open;
  EXPECT_EQ(prepare(), sf::outcome::restricted);
  EXPECT_EQ(calls_, std::vector<std::string>({"is_reparse_point", "inspect_protection", "protect_directory", "is_protected"}));
  EXPECT_TRUE(log_->visible.empty());
}

TEST_F(SharedFolderTest, AMissingFolderIsCreatedThenLockedDown) {
  ASSERT_FALSE(boost::filesystem::exists(shared_));
  EXPECT_EQ(prepare(), sf::outcome::restricted);
  EXPECT_TRUE(boost::filesystem::is_directory(shared_));
  // There was nothing to inspect; a folder just created under %ProgramData%
  // is open by inheritance, so it goes straight to the lockdown.
  EXPECT_EQ(calls_, std::vector<std::string>({"is_reparse_point", "protect_directory", "is_protected"}));
  EXPECT_TRUE(log_->visible.empty());
}

TEST_F(SharedFolderTest, AFolderThatCannotBeCreatedIsReportedAndNotSecured) {
  // A file where the parent should be.
  std::ofstream(shared_.string().c_str()) << "in the way";
  paths_->set_overrides({{"shared-path", (shared_ / "below").string()}});
  EXPECT_EQ(prepare(), sf::outcome::create_failed);
  EXPECT_FALSE(was_called("protect_directory"));
  EXPECT_TRUE(mentions(log_->errors, "Failed to create " + (shared_ / "below").string()));
}

TEST_F(SharedFolderTest, AFailedLockdownSaysPlainlyWhatIsExposed) {
  // Loud, and not fatal: refusing to start would take a working agent offline
  // over a permissions problem an operator can fix.
  boost::filesystem::create_directories(shared_);
  protect_succeeds_ = false;
  EXPECT_EQ(prepare(), sf::outcome::protect_failed);
  EXPECT_FALSE(was_called("is_protected"));
  EXPECT_TRUE(mentions(log_->errors, "acl: SetSecurityInfo(owner) failed: error=1307"));
  EXPECT_TRUE(mentions(log_->errors, "Could not restrict access to " + shared_.string()));
  // "is", not "may be": by this point it was either just created or seen open.
  EXPECT_TRUE(mentions(log_->errors, "it is readable by every user on this machine"));
}

TEST_F(SharedFolderTest, ALockdownThatDidNotTakeIsNotTakenAtItsWord) {
  // Setting a DACL and the DACL excluding everyone else are different claims.
  boost::filesystem::create_directories(shared_);
  protected_after_ = false;
  EXPECT_EQ(prepare(), sf::outcome::still_open);
  EXPECT_TRUE(mentions(log_->errors, "acl: BUILTIN\\Users still has access"));
  EXPECT_TRUE(mentions(log_->errors, "is wider than SYSTEM and Administrators after locking it down"));
}

TEST_F(SharedFolderTest, AnExceptionFromTheAclLayerIsReportedNotRaised) {
  // Only the link is fatal. This runs in the settings bootstrap of every
  // process, so anything else escaping would stop the agent from starting.
  boost::filesystem::create_directories(shared_);
  sf::acl_functions acl = scripted();
  acl.inspect_protection = [](const std::string &, std::list<std::string> &) -> protection { throw std::runtime_error("the disk is on fire"); };
  EXPECT_EQ(sf::prepare(*paths_, log_, acl), sf::outcome::failed);
  EXPECT_TRUE(mentions(log_->errors, "Failed to prepare the shared folder: the disk is on fire"));
}

// --- the join with the real ACL layer ----------------------------------------

TEST_F(SharedFolderTest, TheRealLockdownAgreesWithWhatItReports) {
  // Whatever privileges the test has: elevated, the folder ends up restricted;
  // unelevated, the lockdown is refused and that is said out loud. Either way
  // the answer has to match the folder.
  const sf::outcome result = sf::prepare(*paths_, log_);
  ASSERT_TRUE(boost::filesystem::is_directory(shared_));
  std::list<std::string> errors;
  const bool is_protected = nsclient::windows_acl::is_protected(shared_.string(), errors);
  if (result == sf::outcome::restricted) {
    EXPECT_TRUE(is_protected) << (errors.empty() ? "" : errors.front());
    EXPECT_TRUE(log_->visible.empty());
    // A second run finds it done and leaves it alone.
    EXPECT_EQ(sf::prepare(*paths_, log_), sf::outcome::already_restricted);
  } else {
    EXPECT_EQ(result, sf::outcome::protect_failed);
    EXPECT_FALSE(is_protected);
    EXPECT_TRUE(mentions(log_->errors, "it is readable by every user on this machine"));
  }
  // Hand it back to inheritance so the fixture can clean up - owner first, as
  // in AclTest. After a refused lockdown too: it can get as far as replacing
  // the DACL before it is refused the ownership, which leaves a folder its own
  // creator cannot list.
  for (const std::string &command :
       {"icacls \"" + shared_.string() + "\" /setowner \"%USERNAME%\" /t /q > nul 2>&1", "icacls \"" + shared_.string() + "\" /reset /t /q > nul 2>&1"}) {
    const int ignored = std::system(command.c_str());
    static_cast<void>(ignored);
  }
}

#endif
