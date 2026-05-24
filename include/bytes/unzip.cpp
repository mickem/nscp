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

#include <bytes/unzip.hpp>

// Backend selection: the vendored miniz source is used on Windows (MINIZ is
// propagated by the nscp_miniz CMake target); everywhere else we use the
// system libzip. The public interface in unzip.hpp is identical for both.
#ifdef MINIZ
#include <miniz.h>
#else
#include <zip.h>

#include <cstdio>
#include <vector>
#endif

#include <cstring>

namespace bytes {
namespace unzip {

#ifdef MINIZ
struct reader::impl {
  mz_zip_archive handle_{};
  bool open_ = false;

  impl() { std::memset(&handle_, 0, sizeof(handle_)); }
  ~impl() { close(); }

  bool open(const std::string &file) {
    close();
    std::memset(&handle_, 0, sizeof(handle_));
    open_ = mz_zip_reader_init_file(&handle_, file.c_str(), 0) != MZ_FALSE;
    return open_;
  }
  bool is_open() const { return open_; }
  void close() {
    if (open_) {
      mz_zip_reader_end(&handle_);
      open_ = false;
    }
  }
  unsigned int size() {
    if (!open_) return 0;
    return mz_zip_reader_get_num_files(&handle_);
  }
  bool stat(const unsigned int index, file_entry &out) {
    if (!open_) return false;
    mz_zip_archive_file_stat st;
    if (!mz_zip_reader_file_stat(&handle_, index, &st)) return false;
    out.filename = st.m_filename;
    out.uncompressed_size = static_cast<std::size_t>(st.m_uncomp_size);
    return true;
  }
  bool extract(const std::string &filename, std::string &out) {
    if (!open_) return false;
    std::size_t size = 0;
    // unique_ptr ensures the miniz heap buffer is released even if out.assign throws.
    const std::unique_ptr<void, void (*)(void *)> p(mz_zip_reader_extract_file_to_heap(&handle_, filename.c_str(), &size, 0), mz_free);
    if (!p) return false;
    out.assign(static_cast<const char *>(p.get()), size);
    return true;
  }
  bool extract_to_file(const std::string &filename, const std::string &destination) {
    if (!open_) return false;
    return mz_zip_reader_extract_file_to_file(&handle_, filename.c_str(), destination.c_str(), 0) != MZ_FALSE;
  }
};
#else
// libzip-backed implementation. Used on Linux where miniz is not packaged but
// libzip-dev is a first-class Debian/Ubuntu/Rocky package.

// RAII guards so an entry handle / output file is always released, even when a
// std::string / std::vector allocation throws between acquiring and using it.
namespace {
using zip_file_ptr = std::unique_ptr<zip_file_t, int (*)(zip_file_t *)>;
using file_ptr = std::unique_ptr<FILE, int (*)(FILE *)>;
}  // namespace

struct reader::impl {
  zip_t *handle_ = nullptr;

  impl() = default;
  ~impl() { close(); }

  bool open(const std::string &file) {
    close();
    int err = 0;
    handle_ = zip_open(file.c_str(), ZIP_RDONLY, &err);
    return handle_ != nullptr;
  }
  bool is_open() const { return handle_ != nullptr; }
  void close() {
    if (handle_) {
      zip_close(handle_);
      handle_ = nullptr;
    }
  }
  unsigned int size() {
    if (!handle_) return 0;
    const zip_int64_t n = zip_get_num_entries(handle_, 0);
    return n < 0 ? 0u : static_cast<unsigned int>(n);
  }
  bool stat(unsigned int index, file_entry &out) {
    if (!handle_) return false;
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat_index(handle_, index, 0, &st) != 0) return false;
    out.filename = (st.valid & ZIP_STAT_NAME) && st.name ? st.name : "";
    out.uncompressed_size = (st.valid & ZIP_STAT_SIZE) ? static_cast<std::size_t>(st.size) : 0;
    return true;
  }
  bool extract(const std::string &filename, std::string &out) {
    if (!handle_) return false;
    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat(handle_, filename.c_str(), 0, &st) != 0) return false;
    // Without a valid size we cannot preallocate the output buffer safely.
    if (!(st.valid & ZIP_STAT_SIZE)) return false;
    zip_file_ptr zf(zip_fopen(handle_, filename.c_str(), 0), zip_fclose);
    if (!zf) return false;
    const std::size_t need = static_cast<std::size_t>(st.size);
    std::string buf(need, '\0');
    const zip_int64_t got = need == 0 ? 0 : zip_fread(zf.get(), &buf[0], need);
    if (got < 0 || static_cast<std::size_t>(got) != need) return false;
    out.swap(buf);
    return true;
  }
  bool extract_to_file(const std::string &filename, const std::string &destination) {
    if (!handle_) return false;
    zip_file_ptr zf(zip_fopen(handle_, filename.c_str(), 0), zip_fclose);
    if (!zf) return false;
    file_ptr dst(std::fopen(destination.c_str(), "wb"), std::fclose);
    if (!dst) return false;
    std::vector<char> buf(64 * 1024);
    zip_int64_t got = 0;
    while ((got = zip_fread(zf.get(), buf.data(), buf.size())) > 0) {
      if (std::fwrite(buf.data(), 1, static_cast<std::size_t>(got), dst.get()) != static_cast<std::size_t>(got)) {
        return false;
      }
    }
    return got >= 0;
  }
};
#endif

reader::reader() : impl_(std::make_unique<impl>()) {}
reader::reader(const std::string &file) : impl_(std::make_unique<impl>()) { impl_->open(file); }
reader::~reader() = default;
reader::reader(reader &&) noexcept = default;
reader &reader::operator=(reader &&) noexcept = default;

bool reader::open(const std::string &file) { return impl_->open(file); }
bool reader::is_open() const { return impl_->is_open(); }
void reader::close() { impl_->close(); }
unsigned int reader::size() const { return impl_->size(); }
bool reader::stat(const unsigned int index, file_entry &out) const { return impl_->stat(index, out); }
bool reader::extract(const std::string &filename, std::string &out) const { return impl_->extract(filename, out); }
bool reader::extract_to_file(const std::string &filename, const std::string &destination) const { return impl_->extract_to_file(filename, destination); }

}  // namespace unzip
}  // namespace bytes
