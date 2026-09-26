// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "storage_facts.hpp"

#include <gtest/gtest.h>

#include <nscapi/nscapi_facts_helper.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

namespace {
using nscapi::facts::testing::gathered_of;
using nscapi::facts::testing::json_of;

std::string storage_json(const nscapi::facts::response &out) { return json_of(out, "storage"); }
}  // namespace

TEST(StorageFacts, PublishesOneRecordPerVolume) {
  storage_facts::volume root;
  root.id = "/";
  root.device = "/dev/sda1";
  root.filesystem = "ext4";
  root.type = "fixed";
  root.label = "root";
  root.size_bytes = 107374182400ULL;
  storage_facts::volume windows;
  windows.id = "C:\\";
  windows.device = "\\\\?\\Volume{0}\\";
  windows.filesystem = "NTFS";
  windows.type = "fixed";
  windows.size_bytes = 1;

  nscapi::facts::response out;
  storage_facts::publish({root, windows}, 0, out);

  EXPECT_EQ(storage_json(out),
            "{\"volumes\":[{\"id\":\"/\",\"device\":\"/dev/sda1\",\"filesystem\":\"ext4\",\"type\":\"fixed\",\"label\":\"root\",\"size_bytes\":107374182400},"
            "{\"id\":\"C:\\\\\",\"device\":\"\\\\\\\\?\\\\Volume{0}\\\\\",\"filesystem\":\"NTFS\",\"type\":\"fixed\",\"size_bytes\":1}]}");
}

TEST(StorageFacts, AnUnknownValueIsOmittedNotWrittenEmpty) {
  // A remote volume is listed without asking it for a size, and most volumes
  // have no label: both are absent from the record, never "" or 0.
  storage_facts::volume share;
  share.id = "/mnt/share";
  share.device = "server:/export";
  share.filesystem = "nfs4";
  share.type = "remote";

  nscapi::facts::response out;
  storage_facts::publish({share}, 0, out);

  EXPECT_EQ(storage_json(out), "{\"volumes\":[{\"id\":\"/mnt/share\",\"device\":\"server:/export\",\"filesystem\":\"nfs4\",\"type\":\"remote\"}]}");
}

TEST(StorageFacts, NoVolumesIsAnEmptyListNotAMissingSet) {
  // Enabled and collected, with nothing to report: that is an answer, and it
  // must not read as "not collected" (which is what an absent set means).
  nscapi::facts::response out;
  storage_facts::publish({}, 0, out);
  EXPECT_EQ(storage_json(out), "{\"volumes\":[]}");
}

TEST(StorageFacts, StampsWhenTheValuesWereRead) {
  nscapi::facts::response out;
  storage_facts::publish({}, 1790000000, out);
  EXPECT_EQ(gathered_of(out, "storage"), nscapi::facts::format_time(1790000000));
}

TEST(StorageFacts, DecodesUdevLabelEscapes) {
  EXPECT_EQ(storage_facts::decode_udev_label("root"), "root");
  EXPECT_EQ(storage_facts::decode_udev_label("My\\x20Disk"), "My Disk");
  EXPECT_EQ(storage_facts::decode_udev_label("a\\x2fb"), "a/b");
  EXPECT_EQ(storage_facts::decode_udev_label("\\x41\\x42"), "AB");
}

TEST(StorageFacts, KeepsWhatIsNotAWellFormedEscape) {
  EXPECT_EQ(storage_facts::decode_udev_label("tail\\x2"), "tail\\x2");
  EXPECT_EQ(storage_facts::decode_udev_label("bad\\xzz"), "bad\\xzz");
  EXPECT_EQ(storage_facts::decode_udev_label("\\"), "\\");
  EXPECT_EQ(storage_facts::decode_udev_label(""), "");
}
