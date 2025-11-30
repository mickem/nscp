/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <nsca/server/protocol.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>

class NSCAServer final : public nscapi::impl::simple_plugin, nsca::server::handler {
  nscapi::core_wrapper* core_;
  unsigned int payload_length_;
  int plugin_id_;
  bool noPerfData_;
  bool allowNasty_;
  bool allowArgs_;

  std::string channel_;
  int encryption_;
  std::string password_;

  void set_encryption(const std::string& enc) { encryption_ = nscp::encryption::helpers::encryption_to_int(enc); }
  void set_perf_data(const bool value) {
    noPerfData_ = !value;
    if (noPerfData_) log_debug("nsca", __FILE__, __LINE__, "Performance data disabled!");
  }

 public:
  NSCAServer()
      : simple_plugin(), core_(nullptr), payload_length_(0), plugin_id_(0), noPerfData_(false), allowNasty_(false), allowArgs_(false), encryption_(0) {}
  virtual ~NSCAServer() = default;
  // Module calls
  bool loadModuleEx(const std::string& alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // handler
  void handle(nsca::packet packet) override;
  void log_debug(std::string module, std::string file, int line, std::string msg) const override {
    if (get_core()->should_log(NSCAPI::log_level::debug)) {
      get_core()->log(NSCAPI::log_level::debug, file, line, msg);
    }
  }
  void log_error(std::string module, std::string file, int line, std::string msg) const override {
    if (get_core()->should_log(NSCAPI::log_level::error)) {
      get_core()->log(NSCAPI::log_level::error, file, line, msg);
    }
  }
  unsigned int get_payload_length() override { return payload_length_; }
  int get_encryption() override { return encryption_; }
  std::string get_password() override { return password_; }

 private:
  socket_helpers::connection_info info_;
  boost::shared_ptr<nsca::server::server> server_;
};
