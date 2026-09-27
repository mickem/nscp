// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <functional>
#include <string>

#include "filter.hpp"

struct TaskSched {
  using visitor = std::function<void(const std::shared_ptr<tasksched_filter::filter_obj> &)>;
  void visit(const visitor &consume, std::string computer, std::string user, std::string domain, std::string password, std::string folder, bool recursive,
             bool hidden, bool old, bool strict = false);
  void findAll(tasksched_filter::filter &filter, std::string computer, std::string user, std::string domain, std::string password, std::string folder,
               bool recursive, bool hidden, bool old);
};
