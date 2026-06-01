/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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
