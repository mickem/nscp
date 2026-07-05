// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

namespace clr {

/**
 * C++/CLI RAII wrapper that lets a managed (ref class) object own the
 * lifetime of a native object. Analogous to std::unique_ptr / boost::scoped_ptr
 * for unmanaged code. Non-copyable.
 *
 *   ~ runs deterministically when the managed object is disposed.
 *   ! is the finalizer (runs on GC if Dispose() wasn't called).
 *
 * Both delete the owned native pointer exactly once.
 */
template <typename T>
public ref class clr_scoped_ptr {
 public:
  clr_scoped_ptr() : ptr_(nullptr) {}
  explicit clr_scoped_ptr(T* p) : ptr_(p) {}

  ~clr_scoped_ptr() { this->!clr_scoped_ptr(); }
  !clr_scoped_ptr() {
    delete ptr_;
    ptr_ = nullptr;
  }

  T* get() { return ptr_; }

  T* release() {
    T* tmp = ptr_;
    ptr_ = nullptr;
    return tmp;
  }

  void reset() {
    delete ptr_;
    ptr_ = nullptr;
  }

  void reset(T* p) {
    if (p != ptr_) {
      delete ptr_;
      ptr_ = p;
    }
  }

  void swap(clr_scoped_ptr<T> % other) {
    T* tmp = ptr_;
    ptr_ = other.ptr_;
    other.ptr_ = tmp;
  }

  static T* operator->(clr_scoped_ptr<T> % p) { return p.get(); }
  static T& operator*(clr_scoped_ptr<T> % p) { return *p.get(); }

  operator bool() { return ptr_ != nullptr; }

 private:
  T* ptr_;

  // Non-copyable. Declared private and undefined to forbid copy/assign
  // (handle-reference parameters can't use C++11 = delete in /clr).
  clr_scoped_ptr(clr_scoped_ptr<T> %);
  clr_scoped_ptr<T> % operator=(clr_scoped_ptr<T> %);
};

template <typename T>
inline void swap(clr_scoped_ptr<T> % a, clr_scoped_ptr<T> % b) {
  a.swap(b);
}

}  // namespace clr
