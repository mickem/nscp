// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <list>

namespace PDH {
#define PDH_INDEX_BUF_LEN 2048

class Enumerations {
 public:
  struct Object {
    std::string name;
    std::string error;
    std::list<std::string> instances;
    std::list<std::string> counters;
  };

  typedef std::list<Object> Objects;
  static std::list<std::string> expand_wild_card_path(const std::string &query, std::string &error);
  static void fetch_object_details(Object &object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
  static Objects EnumObjects(bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
  static Object EnumObject(const std::string &object, bool instances = true, bool objects = true, DWORD dwDetailLevel = PERF_DETAIL_WIZARD);
};
}  // namespace PDH
