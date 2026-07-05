// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
class command_proxy {
  unsigned int plugin_id_;
  core_wrapper* core_;

 public:
  virtual ~command_proxy() = default;
  command_proxy(unsigned int plugin_id, nscapi::core_wrapper* core) : plugin_id_(plugin_id), core_(core) {}
  virtual void registry_query(const std::string& request, std::string& response) {
    if (!core_->registry_query(request, response)) {
      throw "TODO: FIXME: DAMN!!!";
    }
  }

  typedef std::shared_ptr<nscapi::command_proxy> ptr;

  static ptr create(unsigned int plugin_id, nscapi::core_wrapper* core) { return std::make_shared<nscapi::command_proxy>(plugin_id, core); }

  unsigned int get_plugin_id() const { return plugin_id_; }

  virtual void err(const char* file, int line, std::string message) { core_->log(NSCAPI::log_level::error, file, line, message); }
  virtual void warn(const char* file, int line, std::string message) { core_->log(NSCAPI::log_level::warning, file, line, message); }
  virtual void info(const char* file, int line, std::string message) { core_->log(NSCAPI::log_level::info, file, line, message); }
  virtual void debug(const char* file, int line, std::string message) { core_->log(NSCAPI::log_level::debug, file, line, message); }
  virtual void trace(const char* file, int line, std::string message) { core_->log(NSCAPI::log_level::trace, file, line, message); }
};
}  // namespace nscapi
