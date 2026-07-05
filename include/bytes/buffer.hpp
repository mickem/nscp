// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

namespace hlp {
template <class T, class U = T*>
struct buffer {
  T* data;
  std::size_t size_;
  explicit buffer(const std::size_t size) : size_(size) { data = new T[size]; }
  buffer(const std::size_t size, const T* src_data) : size_(size) {
    data = new T[size];
    memcpy(data, src_data, size * sizeof(T));
  }
  buffer(const buffer& other) : size_(other.size_) {
    data = new T[size_];
    memcpy(data, other.data, size_ * sizeof(T));
  }
  buffer& operator=(const buffer& other) {
    if (this == &other) {
      return *this;
    }
    size_ = other.size_;
    data = new T[size_];
    memcpy(data, other.data, size_ * sizeof(T));
    return *this;
  }
  T& operator[](const std::size_t pos) { return data[pos]; }
  ~buffer() { delete[] data; }
  operator T*() const { return data; }
  std::size_t size() const { return size_; }
  std::size_t size_in_bytes() const { return size_ * sizeof(T); }
  U get(std::size_t offset = 0) const { return reinterpret_cast<U>(&data[offset]); }
  template <class V>
  V get_t(std::size_t offset = 0) const {
    return reinterpret_cast<V>(&data[offset]);
  }
  void resize(const std::size_t size) {
    size_ = size;
    delete[] data;
    data = new T[size_];
  }
};
}  // namespace hlp
