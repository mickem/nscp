// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/*
 * Unit tests for pidfile.
 *
 * Coverage:
 *   - path construction (rundir + process name, explicit path, default rundir)
 *   - create(pid) / create() writing the pid
 *   - refusal to overwrite an existing pid file (O_EXCL, POSIX)
 *   - refusal to follow a planted symlink (O_NOFOLLOW, POSIX)
 *   - remove() and removal from the destructor
 *
 * All files live in a per-test temp directory; nothing touches /var/run.
 */

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <fstream>
#include <pid_file.hpp>
#include <string>

#ifndef WIN32
#include <unistd.h>
#endif

namespace fs = boost::filesystem;

namespace {

// RAII temp directory: created in the system tmp area, removed on destruction.
class temp_dir {
 public:
  temp_dir() : path_(fs::temp_directory_path() / fs::unique_path("nscp-pid-%%%%%%%%")) { fs::create_directories(path_); }
  ~temp_dir() {
    boost::system::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string read_file(const fs::path& p) {
  std::ifstream in(p.string().c_str());
  std::string content;
  std::getline(in, content);
  return content;
}

}  // namespace

// =============================================================================
// path construction
// =============================================================================

TEST(PidFile, DefaultRundirIsVarRun) { EXPECT_EQ(pidfile::get_default_rundir().string(), "/var/run"); }

TEST(PidFile, DefaultPidfileCombinesNameAndRundir) {
  EXPECT_EQ(pidfile::get_default_pidfile("nscp"), (fs::path("/var/run") / "nscp.pid").string());
}

TEST(PidFile, RundirConstructorBuildsProcessNamePidPath) {
  temp_dir dir;
  pidfile pf(dir.path(), "myproc");
  ASSERT_TRUE(pf.create(4711));
  EXPECT_TRUE(fs::exists(dir.path() / "myproc.pid"));
  EXPECT_EQ(read_file(dir.path() / "myproc.pid"), "4711");
}

// =============================================================================
// create
// =============================================================================

TEST(PidFile, CreateWritesGivenPid) {
  temp_dir dir;
  const fs::path path = dir.path() / "explicit.pid";
  pidfile pf(path);
  ASSERT_TRUE(pf.create(1234));
  EXPECT_EQ(read_file(path), "1234");
}

TEST(PidFile, CreateWithoutArgumentWritesOwnPid) {
  temp_dir dir;
  const fs::path path = dir.path() / "self.pid";
  pidfile pf(path);
  ASSERT_TRUE(pf.create());
  EXPECT_EQ(read_file(path), std::to_string(static_cast<long long>(pf.get_pid())));
}

#ifndef WIN32
TEST(PidFile, GetPidReturnsCurrentProcessPid) {
  temp_dir dir;
  pidfile pf(dir.path() / "x.pid");
  EXPECT_EQ(pf.get_pid(), ::getpid());
}

TEST(PidFile, CreateRefusesToOverwriteExistingFile) {
  temp_dir dir;
  const fs::path path = dir.path() / "stale.pid";
  {
    std::ofstream out(path.string().c_str());
    out << "99999";
  }
  pidfile pf(path);
  // A stale pid file must cause a startup failure, not a silent overwrite.
  EXPECT_FALSE(pf.create(1234));
  EXPECT_EQ(read_file(path), "99999");
}

TEST(PidFile, CreateRefusesToFollowSymlink) {
  temp_dir dir;
  const fs::path target = dir.path() / "target.txt";
  const fs::path link = dir.path() / "planted.pid";
  {
    std::ofstream out(target.string().c_str());
    out << "victim";
  }
  fs::create_symlink(target, link);

  pidfile pf(link);
  // O_NOFOLLOW (plus O_EXCL) must stop us from writing through the symlink.
  EXPECT_FALSE(pf.create(1234));
  EXPECT_EQ(read_file(target), "victim");
}

TEST(PidFile, CreateFailsWhenDirectoryDoesNotExist) {
  temp_dir dir;
  pidfile pf(dir.path() / "no" / "such" / "dir" / "x.pid");
  EXPECT_FALSE(pf.create(1234));
}
#endif

// =============================================================================
// remove
// =============================================================================

TEST(PidFile, RemoveDeletesFile) {
  temp_dir dir;
  const fs::path path = dir.path() / "rm.pid";
  pidfile pf(path);
  ASSERT_TRUE(pf.create(42));
  ASSERT_TRUE(fs::exists(path));
  EXPECT_TRUE(pf.remove());
  EXPECT_FALSE(fs::exists(path));
}

TEST(PidFile, RemoveReturnsFalseWhenNoFileExists) {
  temp_dir dir;
  pidfile pf(dir.path() / "never-created.pid");
  EXPECT_FALSE(pf.remove());
}

TEST(PidFile, DestructorRemovesFile) {
  temp_dir dir;
  const fs::path path = dir.path() / "dtor.pid";
  {
    pidfile pf(path);
    ASSERT_TRUE(pf.create(42));
    ASSERT_TRUE(fs::exists(path));
  }
  EXPECT_FALSE(fs::exists(path));
}
