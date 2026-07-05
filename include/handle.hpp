// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

namespace hlp {
template <class THandle, class TCloser>
struct handle {
  THandle handle_;
  handle() : handle_(NULL) {}
  handle(THandle handle) : handle_(handle) {}
  ~handle() { close(); }
  void close() {
    if (handle_ != NULL) TCloser::close(handle_);
    handle_ = NULL;
  }
  THandle get() const { return handle_; }
  void set(THandle other) {
    close();
    handle_ = other;
  }
  THandle* ref() { return &handle_; }
  THandle detach() {
    THandle tmp = handle_;
    handle_ = NULL;
    return tmp;
  }
  operator THandle() const { return handle_; }
  operator bool() const { return handle_ != NULL; }
  const handle<THandle, TCloser>& operator=(const THandle& other) {
    close();
    handle_ = other;
    return *this;
  }
  const handle<THandle, TCloser>& operator=(handle<THandle, TCloser>& other) {
    close();
    handle_ = other.detach();
    return *this;
  }
};

}  // namespace hlp