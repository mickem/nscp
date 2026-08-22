// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Tests for the zip plugin loader - the thing behind a ".zip module": an
// archive carrying a module.json that names the scripts to install, the
// modules they need and the commands to run once loaded.
//
// Everything here stops at the metadata: the constructor opens the archive and
// parses module.json, which is where an operator's mistake (a missing file, a
// typo in the json, a script under a name nothing can provide) turns into a
// clear error or a silently wrong provider. Actually installing the scripts
// needs a live plugin manager, so that half belongs to the integration suite.
//
// The archives are built here rather than checked in, because the interesting
// cases are all about *what module.json says* - a fixture per variation would
// mean a dozen opaque binaries in the tree.

#include "zip_plugin.h"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace fs = boost::filesystem;
using nsclient::core::zip_plugin;

namespace {

// --- a minimal zip writer ---------------------------------------------------
//
// STORED (uncompressed) entries only, which is all a reader needs to accept.
// The repo has a zip *reader* (bytes::unzip, libzip or miniz depending on the
// platform) but no writer, and pulling one of those backends into a test just
// to lay down a few hundred bytes is more coupling than the job needs.

uint32_t crc32_of(const std::string &data) {
  uint32_t crc = 0xFFFFFFFFu;
  for (const unsigned char c : data) {
    crc ^= c;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return crc ^ 0xFFFFFFFFu;
}

void put16(std::string &out, const uint16_t v) {
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void put32(std::string &out, const uint32_t v) {
  out.push_back(static_cast<char>(v & 0xFF));
  out.push_back(static_cast<char>((v >> 8) & 0xFF));
  out.push_back(static_cast<char>((v >> 16) & 0xFF));
  out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

struct zip_entry {
  std::string name;
  std::string data;
};

void write_zip(const fs::path &path, const std::vector<zip_entry> &entries) {
  std::string body;    // local headers + data
  std::string central; // central directory
  for (const zip_entry &e : entries) {
    const uint32_t crc = crc32_of(e.data);
    const uint32_t offset = static_cast<uint32_t>(body.size());

    put32(body, 0x04034b50);                                 // local file header
    put16(body, 20);                                         // version needed
    put16(body, 0);                                          // flags
    put16(body, 0);                                          // method: stored
    put16(body, 0);                                          // mod time
    put16(body, 0);                                          // mod date
    put32(body, crc);
    put32(body, static_cast<uint32_t>(e.data.size()));       // compressed size
    put32(body, static_cast<uint32_t>(e.data.size()));       // uncompressed size
    put16(body, static_cast<uint16_t>(e.name.size()));
    put16(body, 0);                                          // extra length
    body += e.name;
    body += e.data;

    put32(central, 0x02014b50);                              // central directory header
    put16(central, 20);                                      // version made by
    put16(central, 20);                                      // version needed
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put16(central, 0);
    put32(central, crc);
    put32(central, static_cast<uint32_t>(e.data.size()));
    put32(central, static_cast<uint32_t>(e.data.size()));
    put16(central, static_cast<uint16_t>(e.name.size()));
    put16(central, 0);                                       // extra
    put16(central, 0);                                       // comment
    put16(central, 0);                                       // disk number
    put16(central, 0);                                       // internal attrs
    put32(central, 0);                                       // external attrs
    put32(central, offset);
    central += e.name;
  }

  std::string out = body + central;
  put32(out, 0x06054b50);                                    // end of central directory
  put16(out, 0);                                             // this disk
  put16(out, 0);                                             // disk with cd
  put16(out, static_cast<uint16_t>(entries.size()));         // entries on this disk
  put16(out, static_cast<uint16_t>(entries.size()));         // entries total
  put32(out, static_cast<uint32_t>(central.size()));
  put32(out, static_cast<uint32_t>(body.size()));
  put16(out, 0);                                             // comment length

  std::ofstream f(path.string().c_str(), std::ios::binary);
  f.write(out.data(), static_cast<std::streamsize>(out.size()));
}

class ZipPluginTest : public ::testing::Test {
 protected:
  fs::path dir_;

  void SetUp() override {
    dir_ = fs::temp_directory_path() / fs::unique_path("nscp-zipplugin-%%%%-%%%%");
    fs::create_directories(dir_);
  }
  void TearDown() override {
    boost::system::error_code ec;
    fs::remove_all(dir_, ec);
  }

  // Build an archive and hand back a loaded plugin for it. The paths and
  // plugin-manager handles stay null: reading the metadata never touches them,
  // and installing the scripts (which does) is out of scope here.
  fs::path make_archive(const std::string &name, const std::vector<zip_entry> &entries) {
    const fs::path path = dir_ / name;
    write_zip(path, entries);
    return path;
  }

  static std::unique_ptr<zip_plugin> open(const fs::path &archive) {
    return std::make_unique<zip_plugin>(1, archive, "", nsclient::core::path_instance(), nsclient::core::plugin_mgr_instance(),
                                        nsclient::logging::logger_instance());
  }
};

}  // namespace

TEST_F(ZipPluginTest, reads_the_name_and_description_from_module_json) {
  const fs::path archive = make_archive("sample.zip", {{"module.json", R"({"name":"Sample","description":"A sample module"})"}});

  const auto plugin = open(archive);

  EXPECT_EQ(plugin->getName(), "Sample");
  EXPECT_EQ(plugin->getDescription(), "A sample module");
}

TEST_F(ZipPluginTest, the_module_name_comes_from_the_archive_file_name) {
  // The extracted scripts land in a folder named after the archive, so the
  // ".zip" has to come off - otherwise every path carries it.
  const fs::path archive = make_archive("my-module.zip", {{"module.json", R"({"name":"Sample","description":""})"}});

  EXPECT_EQ(open(archive)->getModule(), "my-module");
}

TEST_F(ZipPluginTest, an_archive_without_module_json_is_rejected) {
  const fs::path archive = make_archive("no-metadata.zip", {{"readme.txt", "nothing to see"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_file_that_is_not_an_archive_is_rejected) {
  const fs::path path = dir_ / "not-a-zip.zip";
  std::ofstream(path.string().c_str()) << "this is not a zip file";

  EXPECT_THROW(open(path), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_missing_file_is_rejected) {
  EXPECT_THROW(open(dir_ / "does-not-exist.zip"), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, malformed_json_is_rejected) {
  const fs::path archive = make_archive("bad-json.zip", {{"module.json", "{not json at all"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, module_json_without_a_name_is_rejected) {
  // as_string() on a missing key throws, and that has to surface as the
  // module failing to load rather than as an unhandled boost::json error.
  const fs::path archive = make_archive("no-name.zip", {{"module.json", R"({"description":"no name here"})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, accepts_the_full_metadata_shape) {
  // Everything module.json may carry, in the two forms a script entry takes:
  // a bare file name, and an object naming its provider explicitly.
  const fs::path archive = make_archive("full.zip", {{"module.json", R"({
        "name": "Full",
        "description": "Every key",
        "scripts": [
          "check_something.py",
          "check_other.sh",
          {"provider": "LUAScript", "script": "s.lua", "alias": "a", "command": "c"}
        ],
        "modules": ["CheckHelpers"],
        "on_start": ["CheckHelpers.add --name x"]
      })"},
                                                     {"check_something.py", "print('hi')\n"},
                                                     {"check_other.sh", "echo hi\n"},
                                                     {"s.lua", "-- lua\n"}});

  const auto plugin = open(archive);

  EXPECT_EQ(plugin->getName(), "Full");
  EXPECT_EQ(plugin->getDescription(), "Every key");
}

TEST_F(ZipPluginTest, a_script_entry_that_is_not_a_string_or_an_object_is_rejected) {
  const fs::path archive = make_archive("bad-script.zip", {{"module.json", R"({"name":"X","description":"","scripts":[42]})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, a_script_object_missing_its_keys_is_rejected) {
  const fs::path archive = make_archive("partial-script.zip", {{"module.json", R"({"name":"X","description":"","scripts":[{"script":"s.lua"}]})"}});

  EXPECT_THROW(open(archive), nsclient::core::plugin_exception);
}

TEST_F(ZipPluginTest, the_metadata_may_be_preceded_by_other_entries) {
  // read_metadata walks the whole archive looking for module.json rather than
  // assuming it comes first.
  const fs::path archive = make_archive("late-metadata.zip", {{"a.txt", "first"},
                                                              {"b.txt", "second"},
                                                              {"module.json", R"({"name":"Late","description":"found anyway"})"}});

  EXPECT_EQ(open(archive)->getName(), "Late");
}

TEST_F(ZipPluginTest, reports_a_fixed_version_and_never_claims_duplicates) {
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  EXPECT_EQ(plugin->get_version(), "1.0.0");
  EXPECT_FALSE(plugin->is_duplicate(archive, ""));
  EXPECT_FALSE(plugin->hasCommandHandler());
  EXPECT_FALSE(plugin->hasNotificationHandler());
  EXPECT_FALSE(plugin->has_on_event());
}

TEST_F(ZipPluginTest, routing_a_message_through_a_zip_module_is_an_error) {
  // A zip module carries scripts; it has no handlers of its own, so anything
  // that would dispatch to one has to fail loudly rather than silently drop.
  const fs::path archive = make_archive("simple.zip", {{"module.json", R"({"name":"Simple","description":""})"}});
  const auto plugin = open(archive);

  char *new_channel = nullptr;
  char *new_buffer = nullptr;
  unsigned int new_len = 0;
  EXPECT_THROW(plugin->route_message("chan", "data", 4, &new_channel, &new_buffer, &new_len), nsclient::core::plugin_exception);
}
