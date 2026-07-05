// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/function.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

struct config_object {
  std::string time_format;
};

class SimpleFileWriter : public nscapi::impl::simple_plugin {
 public:
  typedef boost::function<std::string(const config_object &config, const std::string channel, const PB::Common::Header &hdr,
                                      const PB::Commands::QueryResponseMessage::Response &payload)>
      index_lookup_function;
  typedef std::list<index_lookup_function> index_lookup_type;

 private:
  index_lookup_type syntax_service_lookup_, syntax_host_lookup_;
  std::string filename_;
  boost::shared_mutex cache_mutex_;
  config_object config_;

 public:
  SimpleFileWriter() {}
  virtual ~SimpleFileWriter() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
};
