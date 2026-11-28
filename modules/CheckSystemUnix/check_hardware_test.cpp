// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hardware.h"

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using hardware_check::hardware_info;
using hardware_check::memory_module;
using hardware_check::parse_smbios_entry_point;
using hardware_check::parse_smbios_table;
using hardware_check::smbios_entry_point;
using hardware_check::smbios_inventory;

namespace {

// --- SMBIOS table builder ---------------------------------------------------------

void put16(std::string &s, std::size_t off, std::uint16_t v) {
  if (s.size() < off + 2) s.resize(off + 2, '\0');
  s[off] = static_cast<char>(v & 0xFF);
  s[off + 1] = static_cast<char>(v >> 8);
}
void put32(std::string &s, std::size_t off, std::uint32_t v) {
  if (s.size() < off + 4) s.resize(off + 4, '\0');
  for (int i = 0; i < 4; ++i) s[off + i] = static_cast<char>((v >> (8 * i)) & 0xFF);
}
void put8(std::string &s, std::size_t off, std::uint8_t v) {
  if (s.size() < off + 1) s.resize(off + 1, '\0');
  s[off] = static_cast<char>(v);
}

// One structure: `formatted` is the whole formatted area *including* the
// 4-byte header, whose type/length bytes are filled in here.
std::string structure(std::uint8_t type, std::string formatted, std::size_t length, const std::vector<std::string> &strings) {
  formatted.resize(length, '\0');
  formatted[0] = static_cast<char>(type);
  formatted[1] = static_cast<char>(length);
  std::string out = formatted;
  for (const std::string &s : strings) out += s + std::string(1, '\0');
  if (strings.empty()) out += std::string(1, '\0');
  out += std::string(1, '\0');
  return out;
}

std::string type1(const std::string &vendor, const std::string &model, const std::string &serial) {
  std::string f;
  put8(f, 0x04, 1);
  put8(f, 0x05, 2);
  put8(f, 0x06, 0);
  put8(f, 0x07, serial.empty() ? 0 : 3);
  // UUID 4C4C4544-0042-3510-8051-B4C04F4B4E32 stored little-endian in the first three fields.
  const unsigned char uuid[16] = {0x44, 0x45, 0x4C, 0x4C, 0x42, 0x00, 0x10, 0x35, 0x80, 0x51, 0xB4, 0xC0, 0x4F, 0x4B, 0x4E, 0x32};
  for (int i = 0; i < 16; ++i) put8(f, 0x08 + i, uuid[i]);
  std::vector<std::string> strings = {vendor, model};
  if (!serial.empty()) strings.push_back(serial);  // an empty string would end the string set
  return structure(1, f, 0x1B, strings);
}

std::string type3(std::uint8_t chassis_type, const std::string &serial, const std::string &asset) {
  std::string f;
  put8(f, 0x04, 0);
  put8(f, 0x05, chassis_type);
  put8(f, 0x07, 1);
  put8(f, 0x08, 2);
  return structure(3, f, 0x15, {serial, asset});
}

std::string type16(std::uint8_t use, std::uint16_t devices) {
  std::string f;
  put8(f, 0x04, 3);  // system board
  put8(f, 0x05, use);
  put32(f, 0x07, 0x80000000u);
  put16(f, 0x0D, devices);
  return structure(16, f, 0x17, {});
}

// A type 17 with the 2.8-era length (0x28); size word, configured speed and
// rated speed as given.
std::string type17(std::uint16_t size, std::uint16_t speed, std::uint16_t configured, const std::string &locator, const std::string &part,
                   std::size_t length = 0x28, std::uint32_t extended_size = 0) {
  std::string f;
  put16(f, 0x0C, size);
  put8(f, 0x10, 1);  // device locator
  put8(f, 0x11, 2);  // bank locator
  put16(f, 0x15, speed);
  put8(f, 0x17, 3);                     // manufacturer
  put8(f, 0x18, 4);                     // serial
  put8(f, 0x1A, part.empty() ? 0 : 5);  // part number
  put32(f, 0x1C, extended_size);
  put16(f, 0x20, configured);
  std::vector<std::string> strings = {locator, "BANK 0", "Samsung", "12345678"};
  if (!part.empty()) strings.push_back(part);  // an empty string would end the string set
  return structure(17, f, length, strings);
}

std::string end_of_table() { return structure(127, std::string(), 4, {}); }

// A four-socket board with two empty sockets: 2 x 32 GiB populated (32 GiB
// needs the extended size: the size word tops out at 0x7FFE MiB), the slowest
// configured at 4400 MHz.
std::string server_table() {
  return type1("Dell Inc.", "PowerEdge R760", "ABC1234") + type3(23, "CHS0001", "ASSET-42") + type16(3, 4) + type16(4, 1) /* video: ignored */ +
         type17(0x7FFF, 4800, 4800, "A1", "M321R4GA3BB6-CQKET", 0x28, 32768) + type17(0x7FFF, 4800, 4400, "A2", "M321R4GA3BB6-CQKET", 0x28, 32768) +
         type17(0, 0, 0, "A3", "NO DIMM") /* empty socket */ + type17(0, 0, 0, "A4", "NO DIMM") + end_of_table();
}

// --- fixture tree -------------------------------------------------------------------

class FakeRoot {
 public:
  FakeRoot() {
    base_ = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-hw-%%%%-%%%%");
    boost::filesystem::create_directories(base_);
  }
  ~FakeRoot() {
    boost::system::error_code ec;
    boost::filesystem::permissions(base_, boost::filesystem::owner_all, ec);
    boost::filesystem::remove_all(base_, ec);
  }
  FakeRoot(const FakeRoot &) = delete;
  FakeRoot &operator=(const FakeRoot &) = delete;

  std::string root() const { return base_.string(); }
  void write(const std::string &rel, const std::string &content) const {
    const boost::filesystem::path p = base_ / rel;
    boost::filesystem::create_directories(p.parent_path());
    std::ofstream ofs(p.string().c_str(), std::ios::binary);
    ofs << content;
  }
  void chmod(const std::string &rel, mode_t mode) const { ::chmod((base_ / rel).string().c_str(), mode); }

  void dmi(const std::string &attr, const std::string &value) const { write("sys/class/dmi/id/" + attr, value + "\n"); }
  void standard_dmi() const {
    dmi("sys_vendor", "Dell Inc.");
    dmi("product_name", "PowerEdge R760");
    dmi("product_uuid", "4c4c4544-0042-3510-8051-b4c04f4b4e32");
    dmi("product_serial", "ABC1234");
    dmi("chassis_type", "23");
    dmi("chassis_serial", "CHS0001");
    dmi("chassis_asset_tag", "ASSET-42");
  }
  void memory_blocks(const std::string &block_hex, int online, int offline) const {
    write("sys/devices/system/memory/block_size_bytes", block_hex + "\n");
    write("sys/devices/system/memory/auto_online_blocks", "online\n");
    int n = 0;
    for (int i = 0; i < online; ++i) write("sys/devices/system/memory/memory" + std::to_string(n++) + "/online", "1\n");
    for (int i = 0; i < offline; ++i) write("sys/devices/system/memory/memory" + std::to_string(n++) + "/online", "0\n");
  }

 private:
  boost::filesystem::path base_;
};

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const hardware_info &info, const std::vector<std::string> &args, PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_hardware");
  for (const std::string &a : args) request.add_arguments(a);
  hardware_check::check_from(request, &response, info);
  return response.result();
}

bool has_perf(const PB::Commands::QueryResponseMessage::Response &r, const std::string &alias) {
  for (int i = 0; i < r.lines_size(); ++i) {
    for (int j = 0; j < r.lines(i).perf_size(); ++j) {
      if (r.lines(i).perf(j).alias() == alias) return true;
    }
  }
  return false;
}

std::string entry_point_v2(std::uint16_t table_len, std::uint16_t count, std::uint8_t major = 2, std::uint8_t minor = 8) {
  std::string e(0x1F, '\0');
  e.replace(0, 4, "_SM_");
  e[0x05] = 0x1F;
  e[0x06] = static_cast<char>(major);
  e[0x07] = static_cast<char>(minor);
  e.replace(0x10, 5, "_DMI_");
  put16(e, 0x16, table_len);
  put16(e, 0x1C, count);
  return e;
}

std::string entry_point_v3(std::uint32_t max_len) {
  std::string e(0x18, '\0');
  e.replace(0, 5, "_SM3_");
  e[0x06] = 0x18;
  e[0x07] = 3;
  e[0x08] = 4;
  put32(e, 0x0C, max_len);
  return e;
}

}  // namespace

// --- chassis ------------------------------------------------------------------------

TEST(CheckHardwareUnix, ChassisNames) {
  EXPECT_EQ(hardware_check::chassis_type_to_string(23), "Rack Mount Chassis");
  EXPECT_EQ(hardware_check::chassis_type_to_string(10), "Notebook");
  EXPECT_EQ(hardware_check::chassis_type_to_string(1), "Other");
  EXPECT_EQ(hardware_check::chassis_type_to_string(99), "Unknown (99)");
}

// --- entry point ----------------------------------------------------------------------

TEST(CheckHardwareUnix, EntryPoint32) {
  const auto e = parse_smbios_entry_point(entry_point_v2(1234, 56, 2, 7));
  ASSERT_TRUE(e);
  EXPECT_EQ(e.value().major, 2);
  EXPECT_EQ(e.value().minor, 7);
  EXPECT_EQ(e.value().table_length, 1234u);
  EXPECT_EQ(e.value().structure_count, 56u);
}

TEST(CheckHardwareUnix, EntryPoint64) {
  const auto e = parse_smbios_entry_point(entry_point_v3(4096));
  ASSERT_TRUE(e);
  EXPECT_EQ(e.value().major, 3);
  EXPECT_EQ(e.value().minor, 4);
  EXPECT_EQ(e.value().table_length, 4096u);
  EXPECT_EQ(e.value().structure_count, 0u);
}

TEST(CheckHardwareUnix, EntryPointRejectsGarbageAndTruncation) {
  EXPECT_FALSE(parse_smbios_entry_point(""));
  EXPECT_FALSE(parse_smbios_entry_point("_SM_"));
  EXPECT_FALSE(parse_smbios_entry_point(entry_point_v2(1, 1).substr(0, 0x10)));
  EXPECT_FALSE(parse_smbios_entry_point(entry_point_v3(1).substr(0, 0x10)));
  EXPECT_FALSE(parse_smbios_entry_point(std::string(64, '\xff')));
  std::string bad_anchor = entry_point_v2(1, 1);
  bad_anchor.replace(0x10, 5, "XXXXX");
  EXPECT_FALSE(parse_smbios_entry_point(bad_anchor));
  std::string bad_len = entry_point_v3(1);
  bad_len[0x06] = static_cast<char>(0xF0);  // claims more bytes than there are
  EXPECT_FALSE(parse_smbios_entry_point(bad_len));
}

// --- table parsing -----------------------------------------------------------------------

TEST(CheckHardwareUnix, ParsesServerTable) {
  const smbios_inventory inv = parse_smbios_table(server_table(), boost::none);
  EXPECT_FALSE(inv.truncated);
  EXPECT_EQ(inv.vendor, "Dell Inc.");
  EXPECT_EQ(inv.model, "PowerEdge R760");
  EXPECT_EQ(inv.serial, "ABC1234");
  EXPECT_EQ(inv.uuid, "4C4C4544-0042-3510-8051-B4C04F4B4E32");
  EXPECT_EQ(inv.chassis_type, 23);
  EXPECT_EQ(inv.chassis_serial, "CHS0001");
  EXPECT_EQ(inv.asset_tag, "ASSET-42");
  EXPECT_TRUE(inv.have_arrays);
  EXPECT_EQ(inv.slots, 4);  // the video array is not system memory
  EXPECT_EQ(inv.device_records, 4);
  ASSERT_EQ(inv.modules.size(), 2u);
  EXPECT_EQ(inv.modules[0].locator, "A1");
  EXPECT_EQ(inv.modules[0].capacity, 32LL * 1024 * 1024 * 1024);
  EXPECT_EQ(inv.modules[0].speed, 4800);
  EXPECT_EQ(inv.modules[1].speed, 4400);  // configured beats rated
  EXPECT_EQ(inv.modules[0].part_number, "M321R4GA3BB6-CQKET");
  EXPECT_EQ(inv.modules[0].manufacturer, "Samsung");
  EXPECT_EQ(inv.modules[0].serial, "12345678");
  EXPECT_EQ(inv.structures, 9);
}

TEST(CheckHardwareUnix, SizeEncodings) {
  // KiB granularity, extended size, unknown size, and a pre-2.7 record too
  // short to carry the configured speed.
  const std::string table = type17(0x8000 | 512, 0, 0, "K", "") + type17(0x7FFF, 0, 0, "X", "", 0x28, 65536) + type17(0xFFFF, 0, 0, "U", "") +
                            type17(8192, 1600, 0, "OLD", "", 0x1B) + end_of_table();
  const smbios_inventory inv = parse_smbios_table(table, boost::none);
  ASSERT_EQ(inv.modules.size(), 4u);
  EXPECT_EQ(inv.modules[0].capacity, 512LL * 1024);
  EXPECT_EQ(inv.modules[1].capacity, 65536LL * 1024 * 1024);
  EXPECT_EQ(inv.modules[2].capacity, 0);
  EXPECT_EQ(inv.modules[3].capacity, 8192LL * 1024 * 1024);
  EXPECT_EQ(inv.modules[3].speed, 1600);      // rated speed, configured absent
  EXPECT_EQ(inv.modules[3].part_number, "");  // index 0: no part number
  EXPECT_FALSE(inv.have_arrays);
}

TEST(CheckHardwareUnix, ExtendedSpeed) {
  std::string f;
  put16(f, 0x0C, 16384);
  put16(f, 0x15, 0xFFFF);
  put16(f, 0x20, 0xFFFF);
  put32(f, 0x54, 8000);
  put32(f, 0x58, 7200);
  const std::string table = structure(17, f, 0x5C, {}) + end_of_table();
  const smbios_inventory inv = parse_smbios_table(table, boost::none);
  ASSERT_EQ(inv.modules.size(), 1u);
  EXPECT_EQ(inv.modules[0].speed, 7200);
  EXPECT_EQ(inv.modules[0].locator, "");  // string index 0
}

TEST(CheckHardwareUnix, OutOfRangeStringIndexIsEmpty) {
  std::string f;
  put16(f, 0x0C, 1024);
  put8(f, 0x10, 9);  // only one string exists
  put8(f, 0x11, 1);
  const std::string table = structure(17, f, 0x22, {"BANK 7"}) + end_of_table();
  const smbios_inventory inv = parse_smbios_table(table, boost::none);
  ASSERT_EQ(inv.modules.size(), 1u);
  EXPECT_EQ(inv.modules[0].locator, "BANK 7");  // falls back to the bank locator
  EXPECT_EQ(inv.modules[0].part_number, "");    // index byte 0
}

TEST(CheckHardwareUnix, ShortRecordFieldsAreNotRead) {
  // A type 17 whose length stops before the size word, and a type 1 too
  // short for the UUID: neither may read past its own formatted area (the
  // bytes that follow are the string set, which here look like data).
  std::string f17;
  const std::string table = structure(17, f17, 0x0A, {"\x01\x02\x03\x04\x05\x06\x07\x08"}) + structure(1, std::string(), 0x08, {"V", "M"}) + end_of_table();
  const smbios_inventory inv = parse_smbios_table(table, boost::none);
  EXPECT_FALSE(inv.truncated);
  EXPECT_EQ(inv.device_records, 1);
  EXPECT_TRUE(inv.modules.empty());
  EXPECT_EQ(inv.vendor, "");  // offset 4 holds 0 = no string
  EXPECT_EQ(inv.uuid, "");
}

TEST(CheckHardwareUnix, NonPrintableStringsAreSanitised) {
  std::string f;
  put8(f, 0x04, 1);
  put8(f, 0x05, 2);
  const std::string table = structure(1, f, 0x08,
                                      {std::string("Ven\x01\xff"
                                                   "dor  "),
                                       "  Model X  "}) +
                            end_of_table();
  const smbios_inventory inv = parse_smbios_table(table, boost::none);
  EXPECT_EQ(inv.vendor, "Ven..dor");
  EXPECT_EQ(inv.model, "Model X");
}

TEST(CheckHardwareUnix, TruncatedTablesKeepWhatWasRead) {
  const std::string full = server_table();
  // Cut at every byte: the parser must never read out of bounds (run under
  // ASan to prove it) and never report more than the full table holds.
  for (std::size_t cut = 0; cut < full.size(); ++cut) {
    const smbios_inventory inv = parse_smbios_table(full.substr(0, cut), boost::none);
    EXPECT_LE(inv.modules.size(), 2u);
    EXPECT_LE(inv.structures, 9);
    if (cut > 0) {
      EXPECT_TRUE(inv.truncated || inv.structures > 0) << cut;
    }
  }
  // Cut inside the first module's string set: the system/chassis/array data
  // before it survives.
  const std::size_t before_modules = (type1("Dell Inc.", "PowerEdge R760", "ABC1234") + type3(23, "CHS0001", "ASSET-42") + type16(3, 4) + type16(4, 1)).size();
  const smbios_inventory inv = parse_smbios_table(full.substr(0, before_modules + 0x28 + 3), boost::none);
  EXPECT_TRUE(inv.truncated);
  EXPECT_EQ(inv.vendor, "Dell Inc.");
  EXPECT_EQ(inv.slots, 4);
  EXPECT_TRUE(inv.modules.empty());
}

TEST(CheckHardwareUnix, MalformedHeaders) {
  // Length below the 4-byte header.
  std::string bad(8, '\0');
  bad[0] = 17;
  bad[1] = 2;
  smbios_inventory inv = parse_smbios_table(bad, boost::none);
  EXPECT_TRUE(inv.truncated);
  EXPECT_EQ(inv.structures, 0);
  // Length past the end of the buffer.
  bad[1] = static_cast<char>(0xFF);
  inv = parse_smbios_table(bad, boost::none);
  EXPECT_TRUE(inv.truncated);
  // No double-NUL terminator anywhere.
  std::string unterminated = type1("A", "B", "C");
  unterminated.resize(unterminated.size() - 2);
  unterminated += "xyz";
  inv = parse_smbios_table(unterminated, boost::none);
  EXPECT_TRUE(inv.truncated);
  EXPECT_EQ(inv.vendor, "");
  // Random garbage terminates.
  std::string garbage;
  for (int i = 0; i < 4096; ++i) garbage += static_cast<char>((i * 131 + 7) & 0xFF);
  inv = parse_smbios_table(garbage, boost::none);
  EXPECT_LE(inv.structures, 4096);
}

TEST(CheckHardwareUnix, EntryPointBoundsTheWalk) {
  const std::string table = server_table() + type17(8192, 0, 0, "TRAILING", "");
  // 2.x: the structure count stops the walk before the trailing record.
  smbios_inventory inv = parse_smbios_table(table, parse_smbios_entry_point(entry_point_v2(0, 3)));
  EXPECT_EQ(inv.structures, 3);
  EXPECT_TRUE(inv.modules.empty());
  // 3.x: the maximum length cuts the table.
  inv = parse_smbios_table(table, parse_smbios_entry_point(entry_point_v3(static_cast<std::uint32_t>(server_table().size()))));
  EXPECT_EQ(inv.modules.size(), 2u);
  // A table length larger than the buffer is clamped to the buffer.
  inv = parse_smbios_table(server_table(), parse_smbios_entry_point(entry_point_v3(1u << 30)));
  EXPECT_EQ(inv.modules.size(), 2u);
}

TEST(CheckHardwareUnix, UuidFormatting) {
  const unsigned char zero[16] = {0};
  unsigned char ff[16];
  for (int i = 0; i < 16; ++i) ff[i] = 0xFF;
  EXPECT_EQ(hardware_check::format_smbios_uuid(zero), "");
  EXPECT_EQ(hardware_check::format_smbios_uuid(ff), "");
  const unsigned char seq[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  EXPECT_EQ(hardware_check::format_smbios_uuid(seq), "03020100-0504-0706-0809-0A0B0C0D0E0F");
  EXPECT_EQ(hardware_check::format_smbios_uuid(seq, false), "00010203-0405-0607-0809-0A0B0C0D0E0F");
}

// --- gathering from fixture trees -------------------------------------------------------------

TEST(CheckHardwareUnix, GatherAsRoot) {
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", server_table());
  fs.write("sys/firmware/dmi/tables/smbios_entry_point", entry_point_v3(static_cast<std::uint32_t>(server_table().size())));
  fs.memory_blocks("8000000", 4, 0);  // ignored: the table answers
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_TRUE(h.dmi_ok);
  EXPECT_TRUE(h.smbios_ok);
  EXPECT_EQ(h.vendor, "Dell Inc.");
  EXPECT_EQ(h.model, "PowerEdge R760");
  EXPECT_EQ(h.uuid, "4C4C4544-0042-3510-8051-B4C04F4B4E32");  // upper case, as on Windows
  EXPECT_EQ(h.serial, "ABC1234");
  EXPECT_EQ(h.chassis_type, 23);
  EXPECT_EQ(h.chassis, "Rack Mount Chassis");
  EXPECT_EQ(h.asset_tag, "ASSET-42");
  EXPECT_EQ(h.slots.value(), 4);
  EXPECT_EQ(h.modules.value(), 2);
  EXPECT_EQ(h.memory.value(), 64LL * 1024 * 1024 * 1024);
  EXPECT_EQ(h.memory_speed.value(), 4400);
  EXPECT_EQ(h.memory_source, "smbios");
  EXPECT_EQ(h.module_list, "A1: 32GB@4800MHz; A2: 32GB@4400MHz");
}

TEST(CheckHardwareUnix, GatherUnprivileged) {
  // The root-only attributes and the table are unreadable (mode 0400 owned
  // by root): empty strings and an unknown module count, never an error.
  // Root ignores file modes, so the denied-serial half only runs unprivileged;
  // the missing table and UUID cover the rest either way.
  FakeRoot fs;
  fs.dmi("sys_vendor", "LENOVO");
  fs.dmi("product_name", "20XW0055GE");
  fs.dmi("chassis_type", "10");
  fs.dmi("chassis_asset_tag", "No Asset Information");
  fs.dmi("product_serial", "PF1234");
  fs.chmod("sys/class/dmi/id/product_serial", 0);
  fs.memory_blocks("8000000", 128, 2);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_TRUE(h.dmi_ok);
  EXPECT_FALSE(h.smbios_ok);
  EXPECT_EQ(h.vendor, "LENOVO");
  EXPECT_EQ(h.chassis, "Notebook");
  if (::geteuid() != 0) {
    EXPECT_EQ(h.serial, "");
    EXPECT_NE(h.gather_errors.find("product_serial"), std::string::npos) << h.gather_errors;
  }
  EXPECT_EQ(h.uuid, "");
  EXPECT_FALSE(h.modules);
  EXPECT_FALSE(h.slots);
  EXPECT_FALSE(h.memory_speed);
  EXPECT_EQ(h.memory.value(), 128LL * 0x8000000);  // online blocks only
  EXPECT_EQ(h.memory_source, "sysfs");
  EXPECT_NE(h.gather_errors.find("SMBIOS table"), std::string::npos);
}

TEST(CheckHardwareUnix, GatherNoDmiAtAll) {
  // A container / microVM: no DMI, no table, maybe memory blocks.
  FakeRoot fs;
  fs.memory_blocks("8000000", 8, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_FALSE(h.dmi_ok);
  EXPECT_FALSE(h.smbios_ok);
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(h, {}, r), PB::Common::ResultCode::UNKNOWN);
  EXPECT_EQ(join_lines(r), hardware_check::no_dmi_message);
}

TEST(CheckHardwareUnix, GatherTableWithoutMemoryDevicesFallsBack) {
  // Some hypervisors publish types 1/3 but no 16/17.
  FakeRoot fs;
  fs.write("sys/firmware/dmi/tables/DMI", type1("QEMU", "Standard PC (Q35 + ICH9, 2009)", "") + end_of_table());
  fs.memory_blocks("8000000", 16, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_FALSE(h.dmi_ok);
  EXPECT_TRUE(h.smbios_ok);
  EXPECT_EQ(h.vendor, "QEMU");  // from the table, as sysfs has none
  EXPECT_FALSE(h.modules);
  EXPECT_EQ(h.memory_source, "sysfs");
  EXPECT_EQ(h.memory.value(), 16LL * 0x8000000);
}

TEST(CheckHardwareUnix, DevicesOfNonSystemArraysAreNotModules) {
  // A flash array (handle 0x20) with one 16 MiB device, described *after* its
  // device, next to a two-socket system array (handle 0x10).
  std::string sys_array = type16(3, 2);
  put16(sys_array, 0x02, 0x10);
  std::string flash_array = type16(5, 1);
  put16(flash_array, 0x02, 0x20);
  std::string dimm1 = type17(8192, 3200, 3200, "DIMM 1", "");
  put16(dimm1, 0x04, 0x10);
  std::string dimm2 = type17(0, 0, 0, "DIMM 2", "");
  put16(dimm2, 0x04, 0x10);
  std::string rom = type17(16, 0, 0, "SYSTEM ROM", "");
  put16(rom, 0x04, 0x20);
  const smbios_inventory inv = parse_smbios_table(sys_array + rom + dimm1 + dimm2 + flash_array + end_of_table(), boost::none);
  EXPECT_FALSE(inv.truncated);
  EXPECT_EQ(inv.slots, 2);
  EXPECT_EQ(inv.device_records, 2);
  ASSERT_EQ(inv.modules.size(), 1u);
  EXPECT_EQ(inv.modules[0].locator, "DIMM 1");
}

TEST(CheckHardwareUnix, GatherTruncatedTableLeavesMemoryInventoryUnknown) {
  // The walk stops inside the second module: one module of two is not an
  // inventory, so the counts stay unknown and memory comes from sysfs.
  FakeRoot fs;
  fs.standard_dmi();
  const std::string full = server_table();
  const std::size_t before_modules = (type1("Dell Inc.", "PowerEdge R760", "ABC1234") + type3(23, "CHS0001", "ASSET-42") + type16(3, 4) + type16(4, 1)).size();
  const std::size_t first_module = type17(0x7FFF, 4800, 4800, "A1", "M321R4GA3BB6-CQKET", 0x28, 32768).size();
  fs.write("sys/firmware/dmi/tables/DMI", full.substr(0, before_modules + first_module + 0x28 + 3));
  fs.memory_blocks("8000000", 8, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_TRUE(h.smbios_ok);
  EXPECT_FALSE(h.modules);
  EXPECT_FALSE(h.slots);
  EXPECT_FALSE(h.memory_speed);
  EXPECT_EQ(h.module_list, "");
  EXPECT_EQ(h.memory.value(), 8LL * 0x8000000);
  EXPECT_EQ(h.memory_source, "sysfs");
  EXPECT_NE(h.gather_errors.find("malformed or truncated"), std::string::npos) << h.gather_errors;
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(h, {"crit=modules < 8"}, r), PB::Common::ResultCode::OK);
}

TEST(CheckHardwareUnix, DevicesOfOtherOrUnknownArraysAreKept) {
  // Firmware that labels its main memory array Other (1) or Unknown (2).
  for (const std::uint8_t use : {std::uint8_t(1), std::uint8_t(2)}) {
    std::string array = type16(use, 2);
    put16(array, 0x02, 0x30);
    std::string dimm = type17(8192, 3200, 3200, "DIMM 1", "");
    put16(dimm, 0x04, 0x30);
    const smbios_inventory inv = parse_smbios_table(array + dimm + end_of_table(), boost::none);
    EXPECT_EQ(inv.device_records, 1);
    EXPECT_EQ(inv.other_devices, 0);
    ASSERT_EQ(inv.modules.size(), 1u);
  }
}

TEST(CheckHardwareUnix, GatherOnlyNonSystemDevicesSaysSo) {
  FakeRoot fs;
  fs.standard_dmi();
  std::string flash_array = type16(5, 1);
  put16(flash_array, 0x02, 0x20);
  std::string rom = type17(16, 0, 0, "SYSTEM ROM", "");
  put16(rom, 0x04, 0x20);
  fs.write("sys/firmware/dmi/tables/DMI", flash_array + rom + end_of_table());
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_FALSE(h.modules);
  EXPECT_NE(h.gather_errors.find("no populated system memory module"), std::string::npos) << h.gather_errors;
  EXPECT_NE(h.gather_errors.find("1 device(s) of video, flash or cache arrays ignored"), std::string::npos) << h.gather_errors;
}

TEST(CheckHardwareUnix, GatherDamageAfterAllSocketsKeepsInventory) {
  // Every socket the system array announces was read before the damage.
  FakeRoot fs;
  fs.standard_dmi();
  std::string broken(8, '\0');
  broken[0] = 41;
  broken[1] = 2;  // length below the header
  const std::string full = server_table();
  fs.write("sys/firmware/dmi/tables/DMI", full.substr(0, full.size() - end_of_table().size()) + broken);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_EQ(h.modules.value(), 2);
  EXPECT_EQ(h.slots.value(), 4);
  EXPECT_EQ(h.memory_source, "smbios");
  EXPECT_NE(h.gather_errors.find("malformed or truncated"), std::string::npos) << h.gather_errors;
}

TEST(CheckHardwareUnix, GatherEntryPointCapLeavesMemoryInventoryUnknown) {
  // A 2.x structure count that ends the walk after the first of two modules.
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", server_table());
  fs.write("sys/firmware/dmi/tables/smbios_entry_point", entry_point_v2(0, 5));
  fs.memory_blocks("8000000", 8, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_FALSE(h.modules);
  EXPECT_FALSE(h.slots);
  EXPECT_EQ(h.memory_source, "sysfs");
  EXPECT_NE(h.gather_errors.find("entry point ends the walk"), std::string::npos) << h.gather_errors;
  // A count that covers the whole table is not a cap.
  fs.write("sys/firmware/dmi/tables/smbios_entry_point", entry_point_v2(0, 9));
  const hardware_info ok = hardware_check::gather_hardware_from(fs.root());
  EXPECT_EQ(ok.modules.value(), 2);
  EXPECT_EQ(ok.gather_errors.find("entry point"), std::string::npos) << ok.gather_errors;
}

TEST(CheckHardwareUnix, GatherNoPopulatedModuleIsUnknownNotZero) {
  // An array and sockets but nothing in them: the table does not describe a
  // running machine, so this is "unknown", never "0 modules, 0B".
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", type16(3, 2) + type17(0, 0, 0, "A1", "") + type17(0, 0, 0, "A2", "") + end_of_table());
  fs.memory_blocks("8000000", 8, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_FALSE(h.modules);
  EXPECT_FALSE(h.slots);
  EXPECT_EQ(h.memory.value(), 8LL * 0x8000000);
  EXPECT_EQ(h.memory_source, "sysfs");
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(h, {"warn=memory < 512M or modules < 1"}, r), PB::Common::ResultCode::OK);
}

TEST(CheckHardwareUnix, GatherModuleOfUnknownSizeDoesNotSumMemory) {
  // The modules are counted, but a sum missing one of them is not the total.
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", type16(3, 2) + type17(8192, 0, 0, "A1", "") + type17(0xFFFF, 0, 0, "A2", "") + end_of_table());
  fs.memory_blocks("8000000", 128, 0);
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  EXPECT_EQ(h.modules.value(), 2);
  EXPECT_EQ(h.slots.value(), 2);
  EXPECT_EQ(h.memory.value(), 128LL * 0x8000000);
  EXPECT_EQ(h.memory_source, "sysfs");
}

// --- the check ----------------------------------------------------------------------------

TEST(CheckHardwareUnix, RendersInventoryLine) {
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", server_table());
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(h, {}, r), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r), "OK: Dell Inc. PowerEdge R760 (Rack Mount Chassis), serial=ABC1234, 2 memory module(s), 64GB");
  EXPECT_TRUE(has_perf(r, "hardware_memory"));
  EXPECT_TRUE(has_perf(r, "hardware_modules"));
}

TEST(CheckHardwareUnix, PinnedExpectations) {
  FakeRoot fs;
  fs.standard_dmi();
  fs.write("sys/firmware/dmi/tables/DMI", server_table());
  const hardware_info h = hardware_check::gather_hardware_from(fs.root());
  PB::Commands::QueryResponseMessage::Response r1;
  EXPECT_EQ(run_check(h, {"crit=serial != 'ABC1234'"}, r1), PB::Common::ResultCode::OK);
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(h, {"crit=serial != 'XYZ0000'"}, r2), PB::Common::ResultCode::CRITICAL);
  PB::Commands::QueryResponseMessage::Response r3;
  EXPECT_EQ(run_check(h, {"warn=modules < 4", "crit=memory < 128G"}, r3), PB::Common::ResultCode::CRITICAL);
  PB::Commands::QueryResponseMessage::Response r4;
  EXPECT_EQ(run_check(h, {"warn=slots != 4 or memory_speed < 4400 or chassis_type != 23"}, r4), PB::Common::ResultCode::OK);
  PB::Commands::QueryResponseMessage::Response r5;
  EXPECT_EQ(run_check(h, {"detail-syntax=${module_list} (slots=${slots}, speed=${memory_speed}MHz, src=${memory_source}, uuid=${uuid})"}, r5),
            PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r5), "OK: A1: 32GB@4800MHz; A2: 32GB@4400MHz (slots=4, speed=4400MHz, src=smbios, uuid=4C4C4544-0042-3510-8051-B4C04F4B4E32)");
}

TEST(CheckHardwareUnix, UnknownModuleCountNeverTrips) {
  hardware_info h;
  h.dmi_ok = true;
  h.vendor = "LENOVO";
  h.model = "20XW0055GE";
  h.chassis_type = 10;
  h.chassis = "Notebook";
  h.memory = 16LL * 1024 * 1024 * 1024;
  h.memory_source = "sysfs";
  PB::Commands::QueryResponseMessage::Response r;
  EXPECT_EQ(run_check(h, {"warn=modules < 8 or slots < 2 or memory_speed < 1000"}, r), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(r), "OK: LENOVO 20XW0055GE (Notebook), serial=, unknown memory module(s), 16GB");
  EXPECT_FALSE(has_perf(r, "hardware_modules"));
  PB::Commands::QueryResponseMessage::Response r2;
  EXPECT_EQ(run_check(h, {"crit=modules = 'unknown'"}, r2), PB::Common::ResultCode::CRITICAL);
}
