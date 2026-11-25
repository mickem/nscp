// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <streambuf>
#include <utility>
#include <vector>

namespace safe_file {

// A read-only, seekable std::streambuf over a "device": anything with
//
//   bool read(char *buf, std::size_t len, std::size_t &got);  // got == 0 at end
//   bool seek(std::uint64_t offset);                           // absolute
//   bool size(std::uint64_t &out);
//
// safe_file::open_stream() wraps a safe_file::file in it, so a caller which
// reads with std::istream (check_logfile's file_reader.hpp: the tail scan,
// capped reads and bookmarks all seek) gets the safe open without a second
// implementation of the reading.
//
// Platform-neutral on purpose: the unit test drives it over an in-memory
// device on every OS, which is how the buffer and seek bookkeeping are tested
// on hosts that never build the Windows reader.
template <class Device>
class device_streambuf : public std::streambuf {
 public:
  explicit device_streambuf(Device device, std::size_t buffer_size = 64 * 1024) : device_(std::move(device)), buffer_(buffer_size == 0 ? 1 : buffer_size) {
    setg(buffer_.data(), buffer_.data(), buffer_.data());
  }
  device_streambuf(const device_streambuf &) = delete;
  device_streambuf &operator=(const device_streambuf &) = delete;

 protected:
  int_type underflow() override {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    std::size_t got = 0;
    // A read error ends the stream, exactly as it does for a std::filebuf.
    if (!device_.read(buffer_.data(), buffer_.size(), got) || got == 0) {
      setg(buffer_.data(), buffer_.data(), buffer_.data());
      return traits_type::eof();
    }
    end_pos_ += got;
    setg(buffer_.data(), buffer_.data(), buffer_.data() + got);
    return traits_type::to_int_type(*gptr());
  }

  pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {
    if ((which & std::ios_base::in) == 0) return failed();
    const std::uint64_t current = end_pos_ - static_cast<std::uint64_t>(egptr() - gptr());
    std::uint64_t base = 0;
    if (dir == std::ios_base::cur) {
      // tellg(): answer without dropping the buffer.
      if (off == 0) return pos_type(static_cast<off_type>(current));
      base = current;
    } else if (dir == std::ios_base::end) {
      if (!device_.size(base)) return failed();
    } else if (dir != std::ios_base::beg) {
      return failed();
    }
    const off_type target = static_cast<off_type>(base) + off;
    if (target < 0) return failed();
    return seekpos(pos_type(target), which);
  }

  pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
    const off_type target = static_cast<off_type>(pos);
    if ((which & std::ios_base::in) == 0 || target < 0) return failed();
    const std::uint64_t t = static_cast<std::uint64_t>(target);
    // Still inside what is buffered: move the read pointer only.
    const std::uint64_t buffer_start = end_pos_ - static_cast<std::uint64_t>(egptr() - eback());
    if (t >= buffer_start && t <= end_pos_) {
      setg(eback(), eback() + static_cast<std::ptrdiff_t>(t - buffer_start), egptr());
      return pos;
    }
    if (!device_.seek(t)) return failed();
    end_pos_ = t;
    setg(buffer_.data(), buffer_.data(), buffer_.data());
    return pos;
  }

 private:
  static pos_type failed() { return pos_type(off_type(-1)); }

  Device device_;
  std::vector<char> buffer_;
  // Device offset of egptr(): where the next device read starts.
  std::uint64_t end_pos_ = 0;
};

}  // namespace safe_file
