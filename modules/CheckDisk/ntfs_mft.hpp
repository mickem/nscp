// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// A reader for NTFS Master File Table records: the on-disk format only, with
// no I/O, so it is platform neutral and unit tested on every build. The
// Windows source (largest_files_source_win.cpp) reads the raw records off the
// volume and hands them here.
//
// Only what check_largest_files needs is decoded: the record header, the
// $STANDARD_INFORMATION attributes, every $FILE_NAME (parent and name), the
// unnamed $DATA stream's size (and, for $MFT itself, its run list) and the
// reparse tag. Every offset read from the record is bounds checked against
// the record, since the input is whatever happens to be on the disk.
namespace ntfs {

// The low 48 bits of a file reference are the record number; the high 16 are
// a reuse counter.
constexpr std::uint64_t record_index(std::uint64_t reference) { return reference & 0x0000FFFFFFFFFFFFULL; }

// The record number of the volume's root directory.
constexpr std::uint64_t root_record = 5;
// Records below this are the volume's metadata files ($MFT, $LogFile, ...)
// and the slots reserved for more of them; none of them is a user file.
constexpr std::uint64_t first_user_record = 16;

constexpr std::uint8_t namespace_dos = 2;

// Microsoft's "name surrogate" bit: set on every reparse tag which stands in
// for another named entity (symbolic links, junctions, mount points, WSL
// links), and clear on the ones which only change how a file's own data is
// stored (deduplication, cloud placeholders). Those are real files.
constexpr std::uint32_t reparse_name_surrogate = 0x20000000;

struct run {
  // -1 for a sparse run.
  std::int64_t lcn;
  std::uint64_t clusters;
};

struct file_name {
  std::uint64_t parent = 0;
  std::uint8_t name_namespace = 0;
  std::u16string name;
};

struct record {
  bool in_use = false;
  bool is_directory = false;
  // The base record this one extends, or 0 when it is itself a base record.
  std::uint64_t base_record = 0;

  // The name to report: the first one that is not a DOS short name.
  bool has_name = false;
  std::uint64_t parent = 0;
  std::uint8_t name_namespace = 0;
  std::u16string name;
  // Every $FILE_NAME in the record, short names included. A hard link adds
  // one per link, each with its own parent folder.
  std::vector<file_name> names;

  // The unnamed $DATA stream's logical size, from the attribute segment that
  // starts at VCN 0 (the only one that carries it).
  bool has_data = false;
  std::uint64_t data_size = 0;
  // That segment's run list, when parse_record was asked for it.
  std::vector<run> data_runs;
  // The last VCN the segment above covers.
  std::uint64_t data_last_vcn = 0;

  bool has_attributes = false;
  std::uint32_t attributes = 0;

  bool is_reparse_point = false;
  // 0 when the tag could not be read (non-resident reparse data).
  std::uint32_t reparse_tag = 0;

  // The record's attributes overflowed into extension records.
  bool has_attribute_list = false;

  // A link is any reparse point whose tag is a name surrogate - or whose tag
  // could not be read, so a long symbolic link target cannot slip through.
  bool is_link() const { return is_reparse_point && (reparse_tag == 0 || (reparse_tag & reparse_name_surrogate) != 0); }
};

// Undo the update sequence ("fixup") NTFS applies to every 512-byte stride
// of a record before writing it. False when a stride does not carry the
// sequence number, which means the record is torn or not a record.
bool apply_fixup(unsigned char *data, std::size_t size);

// Decode a non-resident attribute's run list. False when it is malformed.
bool decode_runlist(const unsigned char *data, std::size_t size, std::vector<run> &out);

// Parse one record (fixup included, so `data` is modified). False when it is
// not a valid FILE record; `out` is then undefined.
bool parse_record(unsigned char *data, std::size_t size, record &out, bool want_runs = false);

}  // namespace ntfs
