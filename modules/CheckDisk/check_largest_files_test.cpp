// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_largest_files.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <cstring>
#include <string>
#include <vector>

#include "largest_files_scan.hpp"
#include "ntfs_mft.hpp"
#include "test_support.hpp"

namespace {

using check_disk_test_support::join_lines;
using ScratchDir = check_disk_test_support::ScratchDir;
using largest_files::entry;
using largest_files::scan_request;
using largest_files::scan_result;

PB::Commands::QueryResponseMessage::Response run(const std::vector<std::string> &args, const check_largest_files_command::scanner &scan) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_largest_files");
  for (const std::string &a : args) request.add_arguments(a);
  check_largest_files_command::check_with(request, &response, check_disk_test_support::unrestricted(), scan);
  return response;
}

PB::Commands::QueryResponseMessage::Response run_real(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  PB::Commands::QueryResponseMessage::Response response;
  request.set_command("check_largest_files");
  for (const std::string &a : args) request.add_arguments(a);
  check_largest_files_command::check(request, &response, check_disk_test_support::unrestricted());
  return response;
}

// A scanner that returns a canned result and remembers what it was asked.
struct fake_scanner {
  scan_result result;
  std::shared_ptr<scan_request> seen = std::make_shared<scan_request>();
  scan_result operator()(const scan_request &r) const {
    *seen = r;
    return result;
  }
};

std::vector<std::string> paths_of(const std::vector<entry> &entries) {
  std::vector<std::string> out;
  for (const entry &e : entries) out.push_back(e.path);
  return out;
}

}  // namespace

// ============================================================================
// wildcard_match
// ============================================================================

TEST(LargestFilesWildcard, MatchesStarsAndQuestionMarks) {
  using largest_files::wildcard_match;
  EXPECT_TRUE(wildcard_match("*.log", "app.log", false));
  EXPECT_FALSE(wildcard_match("*.log", "app.log.1", false));
  EXPECT_TRUE(wildcard_match("*.log*", "app.log.1", false));
  EXPECT_TRUE(wildcard_match("a?c", "abc", false));
  EXPECT_FALSE(wildcard_match("a?c", "ac", false));
  EXPECT_TRUE(wildcard_match("*", "", false));
  EXPECT_TRUE(wildcard_match("**x*", "abxcd", false));
  EXPECT_FALSE(wildcard_match("*.LOG", "app.log", false));
  EXPECT_TRUE(wildcard_match("*.LOG", "app.log", true));
  EXPECT_TRUE(wildcard_match(L"*.Dat", L"pagefile.dat", true));
  EXPECT_TRUE(wildcard_match(u"*.vhdx", u"disk.VHDX", true));
}

// ============================================================================
// top_n and folder_rollup
// ============================================================================

TEST(LargestFilesTopN, KeepsTheLargestSortedWithStableTies) {
  largest_files::top_n top(3, 10);
  top.add(entry{"a", 5, 1});  // below min-size
  top.add(entry{"b", 100, 1});
  top.add(entry{"c", 50, 1});
  top.add(entry{"d", 200, 1});
  top.add(entry{"e", 50, 1});
  EXPECT_FALSE(top.accepts(20));
  EXPECT_TRUE(top.accepts(51));
  top.add(entry{"f", 75, 1});
  EXPECT_EQ(paths_of(top.take_sorted()), (std::vector<std::string>{"d", "b", "f"}));

  largest_files::top_n ties(2, 0);
  ties.add(entry{"z", 1, 1});
  ties.add(entry{"y", 1, 1});
  ties.add(entry{"x", 1, 1});
  EXPECT_EQ(paths_of(ties.take_sorted()), (std::vector<std::string>{"x", "y"}));
}

TEST(LargestFilesRollup, TotalsAreRecursiveAndOnlyRankedLevelsAreOffered) {
  // root
  //   a        (10 + a/x 100)
  //     x      (100)
  //   b        (40)
  auto walk = [](largest_files::folder_rollup &r) {
    r.enter("root", 0);
    r.add_file(1);
    r.enter("root/a", 1);
    r.add_file(10);
    r.enter("root/a/x", 2);
    r.add_file(60);
    r.add_file(40);
    r.leave();
    r.leave();
    r.enter("root/b", 1);
    r.add_file(40);
    r.finish();
  };
  {
    largest_files::top_n top(10, 0);
    largest_files::folder_rollup rollup(top, 1);
    walk(rollup);
    const std::vector<entry> out = top.take_sorted();
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].path, "root/a");
    EXPECT_EQ(out[0].size, 110u);
    EXPECT_EQ(out[0].files, 3u);
    EXPECT_EQ(out[1].path, "root/b");
    EXPECT_EQ(out[1].size, 40u);
  }
  {
    largest_files::top_n top(10, 0);
    largest_files::folder_rollup rollup(top, -1);
    walk(rollup);
    EXPECT_EQ(paths_of(top.take_sorted()), (std::vector<std::string>{"root/a", "root/a/x", "root/b"}));
  }
}

TEST(LargestFilesRollup, AMissedLeaveDoesNotMisattributeSizes) {
  largest_files::top_n top(10, 0);
  largest_files::folder_rollup rollup(top, 1);
  rollup.enter("root", 0);
  rollup.enter("root/a", 1);
  rollup.add_file(10);
  // No leave() for root/a: entering a sibling closes it.
  rollup.enter("root/b", 1);
  rollup.add_file(5);
  rollup.finish();
  const std::vector<entry> out = top.take_sorted();
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].size, 10u);
  EXPECT_EQ(out[1].size, 5u);
}

// ============================================================================
// NTFS record parsing
// ============================================================================

namespace {

void put16(std::vector<unsigned char> &b, std::size_t at, std::uint16_t v) { std::memcpy(&b[at], &v, 2); }
void put32(std::vector<unsigned char> &b, std::size_t at, std::uint32_t v) { std::memcpy(&b[at], &v, 4); }
void put64(std::vector<unsigned char> &b, std::size_t at, std::uint64_t v) { std::memcpy(&b[at], &v, 8); }

// Builds a 1024-byte FILE record the way NTFS lays one out on disk,
// fixup included.
class record_builder {
 public:
  explicit record_builder(std::uint16_t flags, std::uint64_t base = 0) : data_(1024, 0) {
    std::memcpy(&data_[0], "FILE", 4);
    put16(data_, 4, 48);  // update sequence array offset
    put16(data_, 6, 3);   // sequence number + one entry per 512-byte stride
    put16(data_, 20, 56);
    put16(data_, 22, flags);
    put32(data_, 28, 1024);
    put64(data_, 32, base);
  }

  record_builder &resident(std::uint32_t type, const std::vector<unsigned char> &value, unsigned name_length = 0) {
    const std::size_t length = (24 + value.size() + 7) & ~static_cast<std::size_t>(7);
    put32(data_, at_, type);
    put32(data_, at_ + 4, static_cast<std::uint32_t>(length));
    data_[at_ + 9] = static_cast<unsigned char>(name_length);
    put32(data_, at_ + 16, static_cast<std::uint32_t>(value.size()));
    put16(data_, at_ + 20, 24);
    std::memcpy(&data_[at_ + 24], value.data(), value.size());
    at_ += length;
    return *this;
  }

  record_builder &file_name(std::uint64_t parent, const std::u16string &name, std::uint8_t name_namespace = 1) {
    std::vector<unsigned char> v(66 + name.size() * 2, 0);
    put64(v, 0, parent | (static_cast<std::uint64_t>(7) << 48));  // a reuse counter in the high bits
    v[64] = static_cast<unsigned char>(name.size());
    v[65] = name_namespace;
    for (std::size_t i = 0; i < name.size(); i++) put16(v, 66 + i * 2, name[i]);
    return resident(0x30, v);
  }

  record_builder &non_resident_data(std::uint64_t size, const std::vector<unsigned char> &runs, std::uint64_t start_vcn = 0) {
    const std::size_t length = (64 + runs.size() + 7) & ~static_cast<std::size_t>(7);
    put32(data_, at_, 0x80);
    put32(data_, at_ + 4, static_cast<std::uint32_t>(length));
    data_[at_ + 8] = 1;
    put64(data_, at_ + 16, start_vcn);
    put64(data_, at_ + 24, 41);
    put16(data_, at_ + 32, 64);
    put64(data_, at_ + 48, size);
    std::memcpy(&data_[at_ + 64], runs.data(), runs.size());
    at_ += length;
    return *this;
  }

  record_builder &reparse(std::uint32_t tag) {
    std::vector<unsigned char> v(8, 0);
    put32(v, 0, tag);
    return resident(0xC0, v);
  }

  std::vector<unsigned char> build() {
    std::vector<unsigned char> out = data_;
    put32(out, at_, 0xFFFFFFFF);
    put32(out, 24, static_cast<std::uint32_t>(at_ + 8));
    // Protect each stride: the original tail goes into the array and the
    // sequence number takes its place.
    put16(out, 48, 0x0042);
    for (std::size_t i = 1; i <= 2; i++) {
      std::memcpy(&out[48 + i * 2], &out[i * 512 - 2], 2);
      put16(out, i * 512 - 2, 0x0042);
    }
    return out;
  }

 private:
  std::vector<unsigned char> data_;
  std::size_t at_ = 56;
};

}  // namespace

TEST(NtfsMft, DecodesARunList) {
  // 24 clusters at 0x5634, 16 clusters 16 back from there, then 5 sparse.
  const std::vector<unsigned char> bytes = {0x21, 0x18, 0x34, 0x56, 0x11, 0x10, 0xF0, 0x01, 0x05, 0x00};
  std::vector<ntfs::run> runs;
  ASSERT_TRUE(ntfs::decode_runlist(bytes.data(), bytes.size(), runs));
  ASSERT_EQ(runs.size(), 3u);
  EXPECT_EQ(runs[0].lcn, 0x5634);
  EXPECT_EQ(runs[0].clusters, 24u);
  EXPECT_EQ(runs[1].lcn, 0x5634 - 16);
  EXPECT_EQ(runs[1].clusters, 16u);
  EXPECT_EQ(runs[2].lcn, -1);
  EXPECT_EQ(runs[2].clusters, 5u);

  // A header promising more bytes than there are.
  const std::vector<unsigned char> truncated = {0x21, 0x18, 0x34};
  runs.clear();
  EXPECT_FALSE(ntfs::decode_runlist(truncated.data(), truncated.size(), runs));
}

TEST(NtfsMft, ParsesAFileRecord) {
  std::vector<unsigned char> std_info(72, 0);
  put32(std_info, 32, 0x20);  // FILE_ATTRIBUTE_ARCHIVE
  // A long attribute list so the second stride's tail is inside an attribute
  // and only comes back right if the fixup is undone.
  std::vector<unsigned char> filler(500, 0xAB);
  std::vector<unsigned char> raw = record_builder(0x0001)
                                       .resident(0x10, std_info)
                                       .file_name(1234, u"PAGEFI~1.SYS", ntfs::namespace_dos)
                                       .resident(0x40, filler)
                                       .file_name(1234, u"pagefile.sys")
                                       .non_resident_data(8ULL << 30, {0x21, 0x18, 0x34, 0x56, 0x00})
                                       .build();
  ntfs::record r;
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r, true));
  EXPECT_TRUE(r.in_use);
  EXPECT_FALSE(r.is_directory);
  EXPECT_EQ(r.base_record, 0u);
  ASSERT_TRUE(r.has_name);
  EXPECT_EQ(r.name, u"pagefile.sys");
  EXPECT_EQ(r.parent, 1234u);
  EXPECT_EQ(r.names.size(), 2u);
  ASSERT_TRUE(r.has_data);
  EXPECT_EQ(r.data_size, 8ULL << 30);
  ASSERT_EQ(r.data_runs.size(), 1u);
  EXPECT_EQ(r.data_runs[0].lcn, 0x5634);
  EXPECT_TRUE(r.has_attributes);
  EXPECT_EQ(r.attributes, 0x20u);
  EXPECT_FALSE(r.is_link());
}

TEST(NtfsMft, ResidentDataNamedStreamsAndExtensionRecords) {
  std::vector<unsigned char> content(300, 'x');
  std::vector<unsigned char> stream(9000 % 256, 'y');
  // A named stream (an alternate data stream) must not count as the size.
  std::vector<unsigned char> raw = record_builder(0x0001, 77).file_name(5, u"small.txt").resident(0x80, stream, 4).resident(0x80, content).build();
  ntfs::record r;
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_EQ(r.base_record, 77u);
  EXPECT_EQ(r.data_size, 300u);

  // A later segment of the data attribute (VCN > 0) does not carry the size.
  raw = record_builder(0x0001).non_resident_data(1, {0x11, 0x01, 0x01, 0x00}, 42).build();
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_FALSE(r.has_data);
}

TEST(NtfsMft, KeepsEveryHardLink) {
  std::vector<unsigned char> raw =
      record_builder(0x0001).file_name(900, u"amd64_x.dll").file_name(31, u"x.dll").file_name(31, u"X~1.DLL", ntfs::namespace_dos).build();
  ntfs::record r;
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_EQ(r.name, u"amd64_x.dll");
  EXPECT_EQ(r.parent, 900u);
  ASSERT_EQ(r.names.size(), 3u);
  EXPECT_EQ(r.names[1].parent, 31u);
  EXPECT_EQ(r.names[1].name, u"x.dll");
}

TEST(NtfsMft, ClassifiesReparsePoints) {
  ntfs::record r;
  std::vector<unsigned char> raw = record_builder(0x0001).file_name(5, u"link").reparse(0xA000000C).build();  // symlink
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_TRUE(r.is_link());

  raw = record_builder(0x0003).file_name(5, u"junction").reparse(0xA0000003).build();  // mount point / junction
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_TRUE(r.is_directory);
  EXPECT_TRUE(r.is_link());

  raw = record_builder(0x0001).file_name(5, u"dedup.vhdx").reparse(0x80000013).build();  // deduplicated data
  ASSERT_TRUE(ntfs::parse_record(raw.data(), raw.size(), r));
  EXPECT_TRUE(r.is_reparse_point);
  EXPECT_FALSE(r.is_link());
}

TEST(NtfsMft, RejectsTornAndForeignRecords) {
  std::vector<unsigned char> raw = record_builder(0x0001).file_name(5, u"a").build();
  std::vector<unsigned char> torn = raw;
  torn[1022] ^= 0xFF;  // a stride written by a different update
  ntfs::record r;
  EXPECT_FALSE(ntfs::parse_record(torn.data(), torn.size(), r));

  std::vector<unsigned char> foreign = raw;
  std::memcpy(&foreign[0], "BAAD", 4);
  EXPECT_FALSE(ntfs::parse_record(foreign.data(), foreign.size(), r));

  // An attribute length running past the record ends the walk, not the
  // process.
  std::vector<unsigned char> overrun = record_builder(0x0001).file_name(5, u"a").build();
  put32(overrun, 56 + 4, 0x7FFFFFF0);
  ASSERT_TRUE(ntfs::parse_record(overrun.data(), overrun.size(), r));
  EXPECT_FALSE(r.has_name);
}

// ============================================================================
// The check, against a canned scanner
// ============================================================================

TEST(CheckLargestFilesCommand, NoPathIsUnknown) {
  fake_scanner fake;
  const auto response = run({}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No path specified"), std::string::npos) << join_lines(response);
}

TEST(CheckLargestFilesCommand, RejectsBadOptions) {
  fake_scanner fake;
  for (const char *bad : {"type=disks", "top=0", "folder-depth=0", "folder-depth=-2", "timeout=-1", "method=fast", "timeout-state=maybe", "min-size=1.5G"}) {
    const auto response = run({"path=/data", std::string(bad)}, fake);
    EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << bad;
    EXPECT_NE(join_lines(response).find("Invalid"), std::string::npos) << bad << ": " << join_lines(response);
  }
}

TEST(CheckLargestFilesCommand, PassesTheRequestThrough) {
  fake_scanner fake;
  run({"path=/data", "type=folders", "top=3", "min-size=2M", "pattern=*.log", "folder-depth=2", "method=walk", "same-filesystem=false"}, fake);
  EXPECT_EQ(fake.seen->root, "/data");
  EXPECT_TRUE(fake.seen->folders);
  EXPECT_EQ(fake.seen->top, 3u);
  EXPECT_EQ(fake.seen->min_size, 2u * 1024 * 1024);
  EXPECT_EQ(fake.seen->pattern, "*.log");
  EXPECT_EQ(fake.seen->folder_depth, 2);
  EXPECT_EQ(fake.seen->method, largest_files::scan_method::walk);
  EXPECT_FALSE(fake.seen->same_filesystem);
  EXPECT_NE(fake.seen->deadline, (std::chrono::steady_clock::time_point::max)());

  run({"path=/data", "pattern=*", "timeout=0"}, fake);
  EXPECT_FALSE(fake.seen->folders);
  EXPECT_TRUE(fake.seen->pattern.empty());
  EXPECT_EQ(fake.seen->deadline, (std::chrono::steady_clock::time_point::max)());
}

TEST(CheckLargestFilesCommand, RendersTheListAndThresholds) {
  fake_scanner fake;
  fake.result.method = "walk";
  fake.result.entries = {entry{"/data/big.iso", 3ULL << 30, 1}, entry{"/data/logs/app.log", 200ULL << 20, 1}};

  auto response = run({"path=/data"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  std::string out = join_lines(response);
  EXPECT_NE(out.find("/data/big.iso (3GB)"), std::string::npos) << out;
  EXPECT_LT(out.find("big.iso"), out.find("app.log")) << out;

  response = run({"path=/data", "warning=size > 100M", "critical=size > 1G"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL);

  response = run({"path=/data", "detail-syntax=${rank}:${name}", "top-syntax=${list}"}, fake);
  out = join_lines(response);
  EXPECT_NE(out.find("1:big.iso"), std::string::npos) << out;
  EXPECT_NE(out.find("2:app.log"), std::string::npos) << out;
}

TEST(CheckLargestFilesCommand, NothingFoundIsOk) {
  fake_scanner fake;
  const auto response = run({"path=/data", "min-size=1T"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);
  EXPECT_NE(join_lines(response).find("No files found"), std::string::npos) << join_lines(response);
}

TEST(CheckLargestFilesCommand, ScanErrorIsUnknown) {
  fake_scanner fake;
  fake.result.error = "Path not found: /nope";
  const auto response = run({"path=/nope"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("Path not found"), std::string::npos);
}

TEST(CheckLargestFilesCommand, ATimeoutIsReportedAndDoesNotMaskAProblem) {
  fake_scanner fake;
  fake.result.timed_out = true;
  fake.result.entries = {entry{"/data/a", 10, 1}};

  auto response = run({"path=/data", "timeout=5"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("timed out after 5s"), std::string::npos) << join_lines(response);

  response = run({"path=/data", "timeout-state=ok"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK);

  response = run({"path=/data", "critical=size > 1"}, fake);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL);
}

// ============================================================================
// The real scanner, walking a scratch tree
// ============================================================================

namespace {
void write_file(const ScratchDir &dir, const std::string &name, std::size_t size) { dir.make_file(name, std::string(size, 'x')); }
}  // namespace

TEST(CheckLargestFilesScan, WalkRanksFilesAndFolders) {
  const ScratchDir dir;
  write_file(dir, "top.bin", 5000);
  write_file(dir, "a/one.log", 3000);
  write_file(dir, "a/deep/two.log", 4000);
  write_file(dir, "b/three.txt", 1000);
  write_file(dir, "b/four.log", 10);

  scan_request request;
  request.root = dir.path().string();
  request.method = largest_files::scan_method::walk;
  request.top = 3;
  scan_result result = largest_files::scan(request);
  ASSERT_TRUE(result.error.empty()) << result.error;
  ASSERT_EQ(result.entries.size(), 3u);
  EXPECT_EQ(boost::filesystem::path(result.entries[0].path).filename().string(), "top.bin");
  EXPECT_EQ(result.entries[0].size, 5000u);
  EXPECT_EQ(boost::filesystem::path(result.entries[1].path).filename().string(), "two.log");
  EXPECT_EQ(boost::filesystem::path(result.entries[2].path).filename().string(), "one.log");
  EXPECT_EQ(result.files_seen, 5u);
  EXPECT_EQ(result.method, "walk");

  request.pattern = "*.log";
  request.min_size = 100;
  result = largest_files::scan(request);
  EXPECT_EQ(result.entries.size(), 2u);

  request.pattern.clear();
  request.min_size = 0;
  request.folders = true;
  result = largest_files::scan(request);
  ASSERT_EQ(result.entries.size(), 2u);
  EXPECT_EQ(boost::filesystem::path(result.entries[0].path).filename().string(), "a");
  EXPECT_EQ(result.entries[0].size, 7000u);
  EXPECT_EQ(result.entries[0].files, 2u);
  EXPECT_EQ(boost::filesystem::path(result.entries[1].path).filename().string(), "b");
  EXPECT_EQ(result.entries[1].size, 1010u);

  request.folder_depth = -1;
  result = largest_files::scan(request);
  EXPECT_EQ(result.entries.size(), 3u);  // a, a/deep, b
}

TEST(CheckLargestFilesScan, DoesNotFollowLinks) {
  const ScratchDir dir;
  const ScratchDir outside;
  write_file(dir, "inside.bin", 100);
  write_file(outside, "elsewhere.bin", 100000);
  boost::system::error_code ec;
  boost::filesystem::create_directory_symlink(outside.path(), dir.path() / "link", ec);
  if (ec) GTEST_SKIP() << "cannot create a directory symlink here: " << ec.message();

  scan_request request;
  request.root = dir.path().string();
  request.method = largest_files::scan_method::walk;
  const scan_result result = largest_files::scan(request);
  ASSERT_TRUE(result.error.empty()) << result.error;
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(boost::filesystem::path(result.entries[0].path).filename().string(), "inside.bin");
  EXPECT_EQ(result.links_skipped, 1u);
}

TEST(CheckLargestFilesScan, MissingRootIsAnError) {
  scan_request request;
  request.root = (boost::filesystem::temp_directory_path() / "nscp_largest_files_definitely_missing_9f2c").string();
  const scan_result result = largest_files::scan(request);
  EXPECT_NE(result.error.find("Path not found"), std::string::npos) << result.error;
}

TEST(CheckLargestFilesScan, TheCheckEndToEnd) {
  const ScratchDir dir;
  write_file(dir, "big.bin", 2048);
  write_file(dir, "small.bin", 16);
  const auto response = run_real({"path=" + dir.path().string(), "method=walk", "top=1", "warning=size > 1K"});
  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
  const std::string out = join_lines(response);
  EXPECT_NE(out.find("big.bin"), std::string::npos) << out;
  EXPECT_EQ(out.find("small.bin"), std::string::npos) << out;
}
