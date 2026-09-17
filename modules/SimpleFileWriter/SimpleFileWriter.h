// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/function.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/snapshot.hpp>

struct config_object {
  std::string time_format;
};

class SimpleFileWriter : public nscapi::impl::simple_plugin {
 public:
  typedef boost::function<std::string(const config_object &config, const std::string channel, const PB::Common::Header &hdr,
                                      const PB::Commands::QueryResponseMessage::Response &payload)>
      index_lookup_function;
  typedef std::list<index_lookup_function> index_lookup_type;

  // Everything handleNotification reads, published in one store.
  //
  // loadModuleEx re-runs on every reload while submissions are arriving on
  // other modules' threads. build_syntax only ever appended, and nothing
  // cleared the two lists, so after N reloads every written line carried N
  // copies of the syntax - and a submission landing mid-reload walked a
  // std::list whose nodes were being re-linked under it.
  struct writer_config {
    config_object config;
    std::string filename;
    index_lookup_type syntax_host_lookup;
    index_lookup_type syntax_service_lookup;
  };

 private:
  nscapi::settings::snapshot<writer_config> config_;
  // Serialises the append itself, nothing else.
  boost::shared_mutex cache_mutex_;

 public:
  SimpleFileWriter() {}
  virtual ~SimpleFileWriter() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
};
