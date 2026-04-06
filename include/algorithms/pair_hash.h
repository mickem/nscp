#pragma once

#include <iostream>
#include <unordered_map>
#include <utility>

struct pair_hash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);

    // A common bit-mixing formula (inspired by Boost)
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};