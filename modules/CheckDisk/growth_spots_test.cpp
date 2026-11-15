// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "growth_spots.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <boost/filesystem.hpp>
#include <chrono>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "test_support.hpp"

using growth_spots::walk_clock;
using growth_spots::dir_size;
using growth_spots::spot;
using ScratchDir = check_disk_test_support::ScratchDir;

namespace {

spot dir(const std::string &path, const char *kind = growth_spots::kind_temp) {
  spot s;
  s.path = path;
  s.kind = kind;
  s.type = spot::directory;
  return s;
}

spot file(const std::string &path) {
  spot s;
  s.path = path;
  s.kind = growth_spots::kind_page_file;
  s.type = spot::file;
  return s;
}

// A fake world: every directory "takes" `cost` of fake time to walk (or up to
// its deadline), and has `bytes` in it.
struct fake {
  walk_clock::time_point t = walk_clock::time_point() + std::chrono::hours(1000);
  std::chrono::milliseconds cost = std::chrono::seconds(5);
  std::map<std::string, unsigned long long> bytes;
  std::vector<std::chrono::milliseconds> budgets;
  int pauses = 0;
  int stop_after_pauses = -1;

  growth_spots::hooks hooks() {
    growth_spots::hooks h;
    h.now = [this]() { return t; };
    h.walk_directory = [this](const std::string &path, const walk_clock::time_point deadline) {
      budgets.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - t));
      dir_size d;
      const std::map<std::string, unsigned long long>::const_iterator it = bytes.find(path);
      if (it == bytes.end()) {
        d.missing = true;
        return d;
      }
      if (t + cost > deadline) {
        // Ran out: half of it was counted.
        t = deadline;
        d.timed_out = true;
        d.bytes = it->second / 2;
      } else {
        t += cost;
        d.bytes = it->second;
      }
      d.files = 1;
      return d;
    };
    h.file_size = [this](const std::string &path, unsigned long long &size, std::string &error) {
      if (path == "denied") {
        error = "could not be read: access denied.\r\n";
        return true;
      }
      const std::map<std::string, unsigned long long>::const_iterator it = bytes.find(path);
      if (it == bytes.end()) return false;
      size = it->second;
      return true;
    };
    h.pause = [this]() {
      ++pauses;
      // A pause takes real time on a real host; it must not count against
      // the round's walking budget.
      t += std::chrono::minutes(10);
      return stop_after_pauses < 0 || pauses <= stop_after_pauses;
    };
    return h;
  }
};

growth_spots::limits limits(const int per_dir_s, const int overall_s) {
  growth_spots::limits l;
  l.per_directory = std::chrono::seconds(per_dir_s);
  l.overall = std::chrono::seconds(overall_s);
  return l;
}

}  // namespace

TEST(GrowthSpots, MeasuresEachSpotInOrderAndSkipsWhatDoesNotExist) {
  fake f;
  f.bytes = {{"/tmp", 1000}, {"/var/log", 2000}, {"/swapfile", 4096}};
  spot shadow;
  shadow.path = "C:\\";
  shadow.kind = growth_spots::kind_shadow_storage;
  shadow.type = spot::measured;
  shadow.has_size = true;
  shadow.size = 77;
  const growth_spots::sample s = growth_spots::measure(
      {dir("/tmp"), dir("/var/tmp"), dir("/var/log", growth_spots::kind_logs), file("/swapfile"), file("/missing"), shadow}, limits(30, 120), f.hooks());
  ASSERT_EQ(s.rows.size(), 4u);
  EXPECT_EQ(s.rows[0].path, "/tmp");
  EXPECT_EQ(s.rows[0].size, 1000);
  EXPECT_TRUE(s.rows[0].note.empty());
  EXPECT_EQ(s.rows[1].path, "/var/log");
  EXPECT_EQ(s.rows[1].kind, "logs");
  EXPECT_EQ(s.rows[2].size, 4096);
  EXPECT_EQ(s.rows[3].kind, "shadow_storage");
  EXPECT_EQ(s.rows[3].size, 77);
  EXPECT_FALSE(s.truncated);
  // One pause between each two directories, none before the first.
  EXPECT_EQ(f.pauses, 2);
}

TEST(GrowthSpots, ADirectoryGetsItsLimitAndReportsWhatItCountedWhenItHitsIt) {
  fake f;
  f.cost = std::chrono::seconds(45);
  f.bytes = {{"/a", 1000}, {"/b", 10}};
  f.cost = std::chrono::seconds(45);
  const growth_spots::sample s = growth_spots::measure({dir("/a")}, limits(30, 120), f.hooks());
  ASSERT_EQ(s.rows.size(), 1u);
  EXPECT_TRUE(s.rows[0].has_size);
  EXPECT_EQ(s.rows[0].size, 500);
  EXPECT_NE(s.rows[0].note.find("time limit"), std::string::npos);
  EXPECT_TRUE(s.truncated);
  ASSERT_EQ(f.budgets.size(), 1u);
  EXPECT_EQ(f.budgets[0], std::chrono::seconds(30));
}

TEST(GrowthSpots, TheRoundHasALimitOfItsOwnThatPausesDoNotUseUp) {
  fake f;
  f.cost = std::chrono::seconds(25);
  f.bytes = {{"/a", 1}, {"/b", 1}, {"/c", 1}, {"/d", 1}};
  // 25 s each against a 60 s round: a and b complete, c gets the 10 s that are
  // left and is cut, d is not walked at all - however long the pauses were.
  const growth_spots::sample s = growth_spots::measure({dir("/a"), dir("/b"), dir("/c"), dir("/d")}, limits(30, 60), f.hooks());
  ASSERT_EQ(s.rows.size(), 4u);
  ASSERT_EQ(f.budgets.size(), 3u);
  EXPECT_EQ(f.budgets[0], std::chrono::seconds(30));
  EXPECT_EQ(f.budgets[1], std::chrono::seconds(30));
  EXPECT_EQ(f.budgets[2], std::chrono::seconds(10));
  EXPECT_TRUE(s.rows[0].note.empty());
  EXPECT_NE(s.rows[2].note.find("time limit"), std::string::npos);
  EXPECT_FALSE(s.rows[3].has_size);
  EXPECT_NE(s.rows[3].note.find("ran out of time"), std::string::npos);
  EXPECT_TRUE(s.truncated);
}

TEST(GrowthSpots, AStopBetweenDirectoriesEndsTheRound) {
  fake f;
  f.bytes = {{"/a", 1}, {"/b", 1}, {"/c", 1}};
  f.stop_after_pauses = 0;
  const growth_spots::sample s = growth_spots::measure({dir("/a"), dir("/b"), dir("/c")}, limits(30, 120), f.hooks());
  EXPECT_EQ(s.rows.size(), 1u);
}

TEST(GrowthSpots, AFileThatCannotBeReadSaysSo) {
  fake f;
  const growth_spots::sample s = growth_spots::measure({file("denied")}, limits(30, 120), f.hooks());
  ASSERT_EQ(s.rows.size(), 1u);
  EXPECT_FALSE(s.rows[0].has_size);
  EXPECT_NE(s.rows[0].note.find("access denied"), std::string::npos);
  // The line break a system message ends in is not part of the note.
  EXPECT_EQ(s.rows[0].note, "could not be read: access denied.");
}

TEST(GrowthSpots, DedupeIgnoresTrailingSeparatorsAndCaseWhereThePlatformDoes) {
  const std::vector<spot> in = {dir("C:\\Windows\\Temp"), dir("c:\\windows\\temp\\"), dir("/tmp"), dir("/tmp/"), dir("/TMP"), dir("")};
  const std::vector<spot> folded = growth_spots::dedupe(in, true);
  ASSERT_EQ(folded.size(), 2u);
  EXPECT_EQ(folded[0].path, "C:\\Windows\\Temp");
  EXPECT_EQ(growth_spots::dedupe(in, false).size(), 4u);
}

TEST(GrowthSpots, DescribeSaysWhatLimitedTheSize) {
  dir_size d;
  EXPECT_EQ(growth_spots::describe(d), "");
  d.unreadable = 1;
  EXPECT_EQ(growth_spots::describe(d), "1 folder or entry could not be read");
  d.unreadable = 3;
  d.timed_out = true;
  EXPECT_EQ(growth_spots::describe(d), "time limit reached: the size counted so far; 3 folders or entries could not be read");
}

TEST(GrowthSpots, ParsesTheSwapFilesOutOfProcSwaps) {
  std::istringstream in(
      "Filename\t\t\t\tType\t\tSize\t\tUsed\t\tPriority\n"
      "/dev/sda2                               partition\t8388604\t\t0\t\t-2\n"
      "/swapfile                               file\t\t2097148\t\t1024\t\t-3\n"
      "/mnt/big\\040disk/swap                   file\t\t1024\t\t0\t\t-4\n");
  const std::vector<std::string> files = growth_spots::parse_swap_files(in);
  ASSERT_EQ(files.size(), 2u);
  EXPECT_EQ(files[0], "/swapfile");
  EXPECT_EQ(files[1], "/mnt/big disk/swap");
  EXPECT_EQ(growth_spots::unescape_proc_path("a\\134b"), "a\\b");
  EXPECT_EQ(growth_spots::unescape_proc_path("trailing\\04"), "trailing\\04");
}

TEST(GrowthSpots, TheSectionHasItsColumnsAndOnlyExistsAfterAWalk) {
  PB::Diagnostics::DiagnosticsMessage none;
  growth_spots::add_section(&none, growth_spots::sample());
  EXPECT_EQ(none.sections_size(), 0);

  growth_spots::sample s;
  s.captured_at = 1700000000;
  s.truncated = true;
  growth_spots::row a;
  a.path = "/var/log";
  a.kind = "logs";
  a.has_size = true;
  a.size = 1234;
  growth_spots::row b;
  b.path = "all volumes";
  b.kind = "shadow_storage";
  b.note = "could not be read";
  s.rows = {a, b};
  PB::Diagnostics::DiagnosticsMessage out;
  growth_spots::add_section(&out, s);
  ASSERT_EQ(out.sections_size(), 1);
  const PB::Diagnostics::Section &section = out.sections(0);
  EXPECT_EQ(section.name(), "disk.growth_spots");
  EXPECT_EQ(section.captured_at(), 1700000000);
  EXPECT_TRUE(section.truncated());
  ASSERT_EQ(section.columns_size(), 4);
  EXPECT_EQ(section.columns(0).name(), "path");
  EXPECT_EQ(section.columns(1).name(), "kind");
  EXPECT_EQ(section.columns(2).name(), "size");
  EXPECT_EQ(section.columns(2).type(), PB::Diagnostics::Column::INT);
  EXPECT_EQ(section.columns(2).unit(), "bytes");
  EXPECT_EQ(section.columns(3).name(), "note");
  ASSERT_EQ(section.rows_size(), 2);
  EXPECT_EQ(section.rows(0).cells(0).s(), "/var/log");
  EXPECT_EQ(section.rows(0).cells(2).i(), 1234);
  // A row without a size, and a complete one without a note, are nulls.
  EXPECT_EQ(section.rows(0).cells(3).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(section.rows(1).cells(2).value_case(), PB::Diagnostics::Cell::VALUE_NOT_SET);
  EXPECT_EQ(section.rows(1).cells(3).s(), "could not be read");

  growth_spots::sample failed;
  failed.captured_at = 1700000000;
  failed.error = "boom";
  PB::Diagnostics::DiagnosticsMessage err;
  growth_spots::add_section(&err, failed);
  ASSERT_EQ(err.sections_size(), 1);
  EXPECT_EQ(err.sections(0).error(), "boom");
}

// --- the real walk ------------------------------------------------------------

TEST(GrowthSpotsWalk, AddsUpTheTree) {
  const ScratchDir tree("nscp-growth");
  tree.make_file("a.log", std::string(1000, 'x'));
  tree.make_file("sub/b.log", std::string(2000, 'x'));
  tree.make_file("sub/deeper/c.tmp", std::string(30, 'x'));
  const dir_size d = growth_spots::walk_directory(tree.string(), walk_clock::now() + std::chrono::seconds(30));
  EXPECT_FALSE(d.missing);
  EXPECT_FALSE(d.timed_out);
  EXPECT_TRUE(d.error.empty()) << d.error;
  EXPECT_EQ(d.bytes, 3030u);
  EXPECT_EQ(d.files, 3u);
}

TEST(GrowthSpotsWalk, DoesNotFollowALinkOutOfTheTree) {
  const ScratchDir tree("nscp-growth");
  const ScratchDir outside("nscp-growth-outside");
  tree.make_file("inside.bin", std::string(100, 'x'));
  outside.make_file("elsewhere.bin", std::string(100000, 'x'));
  boost::system::error_code ec;
  boost::filesystem::create_directory_symlink(outside.path(), tree.path() / "link", ec);
#ifdef WIN32
  if (ec) {
    // A directory symlink needs a privilege an unelevated test may lack; a
    // junction does not, and is the reparse point Windows mostly has.
    const std::string cmd = "cmd /c mklink /J \"" + (tree.path() / "link").string() + "\" \"" + outside.string() + "\" >NUL 2>&1";
    ec.clear();
    if (std::system(cmd.c_str()) != 0 || !boost::filesystem::exists(tree.path() / "link")) ec = boost::system::errc::make_error_code(boost::system::errc::not_supported);
  }
#endif
  if (ec) GTEST_SKIP() << "cannot create a directory link here: " << ec.message();
  const dir_size d = growth_spots::walk_directory(tree.string(), walk_clock::now() + std::chrono::seconds(30));
  EXPECT_EQ(d.bytes, 100u);
  EXPECT_EQ(d.files, 1u);
}

TEST(GrowthSpotsWalk, AMissingDirectoryIsMissing) {
  const ScratchDir tree("nscp-growth");
  const dir_size d = growth_spots::walk_directory((tree.path() / "no-such-dir").string(), walk_clock::now() + std::chrono::seconds(30));
  EXPECT_TRUE(d.missing);
  // A file is not a directory to walk either.
  const std::string f = tree.make_file("plain.txt");
  EXPECT_TRUE(growth_spots::walk_directory(f, walk_clock::now() + std::chrono::seconds(30)).missing);
}

TEST(GrowthSpotsWalk, ADeadlineOrAnAbortCutsTheWalkShort) {
  const ScratchDir tree("nscp-growth");
  for (int i = 0; i < 20; ++i) tree.make_file("f" + std::to_string(i), std::string(10, 'x'));
  const dir_size late = growth_spots::walk_directory(tree.string(), walk_clock::now() - std::chrono::seconds(1));
  EXPECT_TRUE(late.timed_out);
  EXPECT_LT(late.bytes, 200u);
  const std::atomic<bool> abort{true};
  const dir_size aborted = growth_spots::walk_directory(tree.string(), walk_clock::now() + std::chrono::seconds(30), &abort);
  EXPECT_TRUE(aborted.timed_out);
  EXPECT_LT(aborted.bytes, 200u);
}

TEST(GrowthSpotsWalk, TheFileSizeOfAFileAndOfNothing) {
  const ScratchDir tree("nscp-growth");
  const std::string f = tree.make_file("swap.bin", std::string(4096, 'x'));
  unsigned long long size = 0;
  std::string error;
  ASSERT_TRUE(growth_spots::file_size(f, size, error));
  EXPECT_TRUE(error.empty()) << error;
  EXPECT_EQ(size, 4096u);
  EXPECT_FALSE(growth_spots::file_size((tree.path() / "nothing.sys").string(), size, error));
}

TEST(GrowthSpotsDiscover, FindsTheTempAndLogDirectoriesFromTheSystem) {
  const ScratchDir logs("nscp-growth-logs");
  const std::vector<spot> spots = growth_spots::discover(logs.string());
  bool temp = false;
  bool agent_logs = false;
  for (const spot &s : spots) {
    EXPECT_FALSE(s.path.empty());
    if (s.kind == std::string(growth_spots::kind_temp)) temp = true;
    if (s.kind == std::string(growth_spots::kind_logs) && s.path == logs.string()) agent_logs = true;
  }
  EXPECT_TRUE(temp);
  EXPECT_TRUE(agent_logs);
}

TEST(GrowthSpotsWalker, TheFirstWaitFollowsTheSeededRound) {
  using std::chrono::seconds;
  // Nothing to go on: the short delay after start.
  EXPECT_EQ(seconds(15), growth_spots::first_wait(seconds(15), seconds(600), 0, 10000));
  // A reload 100 seconds after the last round waits out the rest of it.
  EXPECT_EQ(seconds(500), growth_spots::first_wait(seconds(15), seconds(600), 9900, 10000));
  // A round that is due, or overdue, still gets the short delay.
  EXPECT_EQ(seconds(15), growth_spots::first_wait(seconds(15), seconds(600), 9410, 10000));
  EXPECT_EQ(seconds(15), growth_spots::first_wait(seconds(15), seconds(600), 5000, 10000));
  // A shorter interval applies to the seeded round too.
  EXPECT_EQ(seconds(15), growth_spots::first_wait(seconds(15), seconds(60), 9900, 10000));
  // A seed from the future (the clock was set back) is taken as fresh, not
  // waited on for longer than one interval.
  EXPECT_EQ(seconds(600), growth_spots::first_wait(seconds(15), seconds(600), 20000, 10000));
}

TEST(GrowthSpotsWalker, StopsPromptlyAndHasNothingBeforeTheFirstRound) {
  growth_spots::walker w;
  w.first_delay = std::chrono::seconds(3600);
  w.start();
  EXPECT_EQ(w.get().captured_at, 0);
  const walk_clock::time_point before = walk_clock::now();
  w.stop();
  EXPECT_LT(walk_clock::now() - before, std::chrono::seconds(5));
}

TEST(GrowthSpotsWalker, ARoundInProgressIsAbortedByStop) {
  growth_spots::walker w;
  w.first_delay = std::chrono::seconds(0);
  w.start();
  // Whatever it is walking right now (the system's temp and log folders), a
  // stop must not wait for the walk to finish.
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  const walk_clock::time_point before = walk_clock::now();
  w.stop();
  EXPECT_LT(walk_clock::now() - before, std::chrono::seconds(5));
}
