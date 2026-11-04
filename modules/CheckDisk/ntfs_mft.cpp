// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ntfs_mft.hpp"

#include <cstring>

namespace ntfs {

namespace {
// NTFS is little-endian on disk, as is every platform this builds for.
template <class T>
T read(const unsigned char *p) {
  T value;
  std::memcpy(&value, p, sizeof(T));
  return value;
}

constexpr std::size_t fixup_stride = 512;

constexpr std::uint32_t attr_standard_information = 0x10;
constexpr std::uint32_t attr_attribute_list = 0x20;
constexpr std::uint32_t attr_file_name = 0x30;
constexpr std::uint32_t attr_data = 0x80;
constexpr std::uint32_t attr_reparse_point = 0xC0;
constexpr std::uint32_t attr_end = 0xFFFFFFFF;

constexpr std::uint16_t record_in_use = 0x0001;
constexpr std::uint16_t record_directory = 0x0002;
}  // namespace

bool apply_fixup(unsigned char *data, const std::size_t size) {
  if (size < 8) return false;
  const std::size_t usa_offset = read<std::uint16_t>(data + 4);
  const std::size_t usa_count = read<std::uint16_t>(data + 6);
  if (usa_count < 2 || usa_offset + usa_count * 2 > size || (usa_count - 1) * fixup_stride > size) return false;
  const std::uint16_t sequence = read<std::uint16_t>(data + usa_offset);
  for (std::size_t i = 1; i < usa_count; i++) {
    unsigned char *tail = data + i * fixup_stride - 2;
    if (read<std::uint16_t>(tail) != sequence) return false;
    std::memcpy(tail, data + usa_offset + i * 2, 2);
  }
  return true;
}

bool decode_runlist(const unsigned char *data, const std::size_t size, std::vector<run> &out) {
  const unsigned char *p = data;
  const unsigned char *end = data + size;
  std::int64_t lcn = 0;
  while (p < end && *p != 0) {
    const unsigned length_size = *p & 0x0F;
    const unsigned offset_size = *p >> 4;
    ++p;
    if (length_size == 0 || length_size > 8 || offset_size > 8 || static_cast<std::size_t>(end - p) < length_size + offset_size) return false;
    std::uint64_t clusters = 0;
    for (unsigned i = 0; i < length_size; i++) clusters |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    p += length_size;
    if (offset_size == 0) {
      out.push_back(run{-1, clusters});
      continue;
    }
    std::uint64_t delta = 0;
    for (unsigned i = 0; i < offset_size; i++) delta |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    // The offset is signed, relative to the previous run.
    if (offset_size < 8 && (p[offset_size - 1] & 0x80) != 0) delta |= ~0ULL << (8 * offset_size);
    p += offset_size;
    lcn += static_cast<std::int64_t>(delta);
    out.push_back(run{lcn, clusters});
  }
  return true;
}

bool parse_record(unsigned char *data, const std::size_t size, record &out, const bool want_runs) {
  out = record();
  if (size < 48 || std::memcmp(data, "FILE", 4) != 0) return false;
  if (!apply_fixup(data, size)) return false;

  const std::uint16_t flags = read<std::uint16_t>(data + 22);
  out.in_use = (flags & record_in_use) != 0;
  out.is_directory = (flags & record_directory) != 0;
  out.base_record = record_index(read<std::uint64_t>(data + 32));

  std::size_t limit = read<std::uint32_t>(data + 24);
  if (limit > size) limit = size;
  std::size_t offset = read<std::uint16_t>(data + 20);

  while (offset + 16 <= limit) {
    const unsigned char *attr = data + offset;
    const std::uint32_t type = read<std::uint32_t>(attr);
    if (type == attr_end) break;
    const std::uint32_t length = read<std::uint32_t>(attr + 4);
    if (length < 16 || length > limit - offset) break;
    const bool non_resident = attr[8] != 0;
    const unsigned name_length = attr[9];

    const unsigned char *value = nullptr;
    std::size_t value_length = 0;
    if (!non_resident && length >= 24) {
      value_length = read<std::uint32_t>(attr + 16);
      const std::size_t value_offset = read<std::uint16_t>(attr + 20);
      if (value_offset <= length && value_length <= length - value_offset) {
        value = attr + value_offset;
      } else {
        value_length = 0;
      }
    }

    switch (type) {
      case attr_standard_information:
        if (value && value_length >= 36) {
          out.has_attributes = true;
          out.attributes = read<std::uint32_t>(value + 32);
        }
        break;
      case attr_attribute_list:
        out.has_attribute_list = true;
        break;
      case attr_file_name:
        if (value && value_length >= 66) {
          const std::size_t chars = value[64];
          const std::uint8_t name_namespace = value[65];
          if (66 + chars * 2 > value_length) break;
          file_name fn;
          fn.parent = record_index(read<std::uint64_t>(value));
          fn.name_namespace = name_namespace;
          fn.name.resize(chars);
          for (std::size_t i = 0; i < chars; i++) fn.name[i] = static_cast<char16_t>(read<std::uint16_t>(value + 66 + i * 2));
          // A file with a short name has it in a DOS-only $FILE_NAME of its
          // own; any other name is the one to report.
          if (!out.has_name || (out.name_namespace == namespace_dos && name_namespace != namespace_dos)) {
            out.has_name = true;
            out.parent = fn.parent;
            out.name_namespace = fn.name_namespace;
            out.name = fn.name;
          }
          out.names.push_back(std::move(fn));
        }
        break;
      case attr_data:
        // Named streams are alternate data streams; only the file's own
        // content counts, as it does for a directory listing.
        if (name_length != 0) break;
        if (!non_resident) {
          if (value) {
            out.has_data = true;
            out.data_size = value_length;
          }
        } else if (length >= 64 && read<std::uint64_t>(attr + 16) == 0) {
          out.has_data = true;
          out.data_size = read<std::uint64_t>(attr + 48);
          out.data_last_vcn = read<std::uint64_t>(attr + 24);
          if (want_runs) {
            const std::size_t runs_offset = read<std::uint16_t>(attr + 32);
            if (runs_offset >= length || !decode_runlist(attr + runs_offset, length - runs_offset, out.data_runs)) out.data_runs.clear();
          }
        }
        break;
      case attr_reparse_point:
        out.is_reparse_point = true;
        if (value && value_length >= 4) out.reparse_tag = read<std::uint32_t>(value);
        break;
      default:
        break;
    }
    offset += length;
  }
  return true;
}

}  // namespace ntfs
