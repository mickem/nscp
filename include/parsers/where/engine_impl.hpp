// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/node.hpp>

namespace parsers {
namespace where {
template <class TObject>
struct generic_summary;

template <class TObject>
struct evaluation_context_impl : object_factory_interface {
  typedef TObject object_type;
  typedef generic_summary<TObject> *summary_type;
  typedef std::list<std::string> errors_type;
  bool enable_debug_;
  evaluation_context_impl() : enable_debug_(false) {}
  boost::optional<TObject> object;
  boost::optional<summary_type> summary;
  errors_type errors_;
  errors_type warnings_;
  std::list<std::pair<TObject, object_match>> object_debug_;
  boost::optional<TObject> last_debug_object_;

  TObject get_object() { return *object; }
  void enable_debug(const bool enable_debug) override { enable_debug_ = enable_debug; }
  bool debug_enabled() override { return enable_debug_; }

  std::string get_debug() const override {
    std::stringstream ss;
    for (const auto &obj : object_debug_) {
      switch (obj.second) {
        case none:
          ss << "ignored  ";
          break;
        case match:
          ss << "match    ";
          break;
        case critical:
          ss << "critical ";
          break;
        case warning:
          ss << "warning  ";
          break;
        case ok:
          ss << "ok       ";
          break;
        default:
          ss << "?";
          break;
      }
      ss << obj.first->show() << "\n";
    }
    return ss.str();
  }
  void debug(object_match reason) override {
    if (!enable_debug_) {
      return;
    }
    if (last_debug_object_ && object && last_debug_object_ == object && !object_debug_.empty()) {
      last_debug_object_ = *object;
      object_debug_.back().second = reason;
    } else {
      last_debug_object_ = *object;
      object_debug_.emplace_back(*object, reason);
    }
  }

  summary_type get_summary() { return *summary; }
  bool has_object() { return static_cast<bool>(object); }
  bool has_summary() { return static_cast<bool>(summary); }
  void remove_object() { object.reset(); }
  void remove_summary() { summary.reset(); }
  void set_object(TObject o) { object = o; }
  void set_summary(summary_type o) { summary = o; }

  bool has_error() const override { return !errors_.empty(); }
  bool has_warn() const override { return !warnings_.empty(); }
  std::string get_error() const override {
    std::string ret;
    for (const std::string &m : errors_) {
      if (!ret.empty()) ret += ", ";
      ret += m;
    }
    return ret;
  }
  std::string get_warn() const override {
    std::string ret;
    for (const std::string &m : warnings_) {
      if (!ret.empty()) ret += ", ";
      ret += m;
    }
    return ret;
  }
  void clear() override {
    errors_.clear();
    warnings_.clear();
  }
  void error(const std::string msg) override { errors_.push_back(msg); }
  void warn(const std::string msg) override { warnings_.push_back(msg); }
};
}  // namespace where
}  // namespace parsers