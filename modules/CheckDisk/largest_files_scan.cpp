// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "largest_files_scan.hpp"

#include <algorithm>
#include <utility>

namespace largest_files {

namespace {
// True when `a` ranks before `b`. As a heap comparator it keeps the entry
// that ranks last at the front - the smallest, and among equal sizes the path
// which sorts last - which is the one to evict.
bool heap_order(const entry &a, const entry &b) {
  if (a.size != b.size) return a.size > b.size;
  return a.path < b.path;
}
}  // namespace

void top_n::add(entry e) {
  if (!accepts(e.size)) return;
  if (heap_.size() >= n_) {
    if (!heap_order(e, heap_.front())) return;
    std::pop_heap(heap_.begin(), heap_.end(), heap_order);
    heap_.pop_back();
  }
  heap_.push_back(std::move(e));
  std::push_heap(heap_.begin(), heap_.end(), heap_order);
}

std::vector<entry> top_n::take_sorted() {
  std::vector<entry> out;
  out.swap(heap_);
  std::sort(out.begin(), out.end(), heap_order);
  return out;
}

void folder_rollup::enter(const std::string &path, const int level) {
  while (!stack_.empty() && stack_.back().level >= level) leave();
  // Only a ranked folder needs its path; the others only carry a total
  // upwards, and a deep tree has far more of those.
  stack_.push_back(frame{ranks(level) ? path : std::string(), level, 0, 0});
}

void folder_rollup::add_file(const unsigned long long size) {
  if (stack_.empty()) return;
  stack_.back().size += size;
  stack_.back().files++;
}

void folder_rollup::leave() {
  if (stack_.empty()) return;
  frame done = std::move(stack_.back());
  stack_.pop_back();
  if (!stack_.empty()) {
    stack_.back().size += done.size;
    stack_.back().files += done.files;
  }
  if (ranks(done.level) && ranking_.accepts(done.size)) ranking_.add(entry{std::move(done.path), done.size, done.files});
}

void folder_rollup::finish() {
  while (!stack_.empty()) leave();
}

}  // namespace largest_files
