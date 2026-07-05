// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/function.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <list>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/protobuf/command.hpp>

class SimpleCache : public nscapi::impl::simple_plugin {
 public:
  struct cache_query {
    std::string channel;
    std::string host;
    std::string alias;
    std::string command;
  };

 private:
  typedef boost::function<std::string(const std::string channel, const PB::Common::Header &hdr, const PB::Commands::QueryResponseMessage::Response &payload)>
      index_lookup_function;
  typedef boost::function<std::string(const cache_query &query)> command_lookup_function;
  typedef std::list<index_lookup_function> index_lookup_type;
  typedef std::list<command_lookup_function> command_lookup_type;
  index_lookup_type index_lookup_;
  command_lookup_type command_lookup_;

  typedef std::map<std::string, std::string> cache_type;
  cache_type cache_;
  boost::shared_mutex cache_mutex_;

 public:
  SimpleCache() {}
  virtual ~SimpleCache() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);

  void handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message);
  void check_cache(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  void list_cache(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
};
