/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace bytes {
namespace unzip {

// Backend-neutral metadata for a single entry in an archive. Keeps callers
// free of miniz/libzip-specific types.
struct file_entry {
  std::string filename;
  std::size_t uncompressed_size = 0;
};

// Read-only ZIP archive reader with a single, platform-independent interface.
//
// The implementation is selected at compile time (vendored miniz on Windows,
// system libzip elsewhere) and hidden behind a pImpl so this header never
// pulls in any backend headers or types. Callers link nscp_miniz and use the
// same API regardless of platform.
//
// The reader is move-only; it owns the underlying archive handle and releases
// it on destruction.
class reader {
 public:
  reader();
  // Opens the given file immediately; use is_open() to check the result.
  explicit reader(const std::string &file);
  ~reader();

  reader(const reader &) = delete;
  reader &operator=(const reader &) = delete;
  reader(reader &&) noexcept;
  reader &operator=(reader &&) noexcept;

  // Opens an archive for reading, closing any previously opened one first.
  // Returns true on success.
  bool open(const std::string &file) const;
  bool is_open() const;
  void close() const;

  // Number of entries in the archive (0 when not open).
  unsigned int size() const;

  // Fills out with metadata for the entry at index. Returns false on failure
  // (not open or index out of range).
  bool stat(unsigned int index, file_entry &out) const;

  // Extracts the named entry into out. Returns false if the entry is missing
  // or extraction fails. out is binary-safe (std::string is used as a byte
  // buffer).
  bool extract(const std::string &filename, std::string &out) const;

  // Extracts the named entry to destination on disk. Returns false on failure.
  bool extract_to_file(const std::string &filename, const std::string &destination) const;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace unzip
}  // namespace bytes
