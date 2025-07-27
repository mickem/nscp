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

#include "SimpleCache.h"

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>

#include <parsers/expression/expression.hpp>

#include <str/format.hpp>

#include <boost/thread.hpp>

#include <map>
#include <vector>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

struct simple_string_functor {
  std::string value;
  simple_string_functor(std::string value) : value(value) {}
  simple_string_functor(const simple_string_functor &other) : value(other.value) {}
  const simple_string_functor &operator=(const simple_string_functor &other) {
    value = other.value;
    return *this;
  }
  std::string operator()(const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &) { return value; }
  std::string operator()(const SimpleCache::cache_query &) { return value; }
};
struct header_host_functor {
  std::string operator()(const std::string, const PB::Common::Header &hdr, const PB::Commands::QueryResponseMessage::Response &) {
    std::string sender = hdr.sender_id();
    for (int i = 0; i < hdr.hosts_size(); i++) {
      if (hdr.hosts(i).id() == sender) return hdr.hosts(i).host();
    }
    return "";
  }
  std::string operator()(const SimpleCache::cache_query &query) { return query.host; }
};
struct payload_command_functor {
  std::string operator()(const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    return payload.command();
  }
  std::string operator()(const SimpleCache::cache_query &query) { return query.command; }
};
struct channel_functor {
  std::string operator()(const std::string channel, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &) { return channel; }
  std::string operator()(const SimpleCache::cache_query &query) { return query.channel; }
};
struct payload_alias_functor {
  std::string operator()(const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) { return payload.alias(); }
  std::string operator()(const SimpleCache::cache_query &query) { return query.alias; }
};
struct payload_alias_or_command_functor {
  std::string operator()(const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    if (!payload.alias().empty()) return payload.alias();
    return payload.command();
  }
  std::string operator()(const SimpleCache::cache_query &query) {
    if (!query.alias.empty()) return query.alias;
    return query.command;
  }
};

std::string simple_string_fun(std::string key) { return key; }
bool SimpleCache::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  std::string primary_key;
  std::string channel;
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));

  settings.set_alias(alias, "cache");

  settings.alias().add_path_to_settings()("CACHE", "Section for simple cache module (SimpleCache.dll).");

  settings.alias()
      .add_key_to_settings()
      .add_string("primary index", sh::string_key(&primary_key, "${alias-or-command}"), "PRIMARY CACHE INDEX",
                  "Set this to the value you want to use as unique key for the cache.\nCan be any arbitrary string as well as include any of the following "
                  "special keywords:"
                  "${command} = The command name, ${host} the host, ${channel} the receiving channel, ${alias} the alias for the command, ${alias-or-command} "
                  "= alias if set otherwise command, ${message} = the message data (no escape), ${result} = The result status (number).")

      .add_string("channel", sh::string_key(&channel, "CACHE"), "CHANNEL", "The channel to listen to.");

  settings.register_all();
  settings.notify();

  nscapi::core_helper core(get_core(), get_id());
  core.register_channel(channel);

  parsers::simple_expression parser;
  parsers::simple_expression::result_type result;
  if (!parser.parse(primary_key, result)) {
    NSC_LOG_ERROR_STD("Failed to parse primary key: " + primary_key)
  }
  for (parsers::simple_expression::entry &e : result) {
    if (!e.is_variable) {
      index_lookup_.push_back(simple_string_functor(e.name));
      command_lookup_.push_back(simple_string_functor(e.name));
    } else if (e.name == "command") {
      index_lookup_.push_back(payload_command_functor());
      command_lookup_.push_back(payload_command_functor());
    } else if (e.name == "host") {
      index_lookup_.push_back(header_host_functor());
      command_lookup_.push_back(header_host_functor());
    } else if (e.name == "channel") {
      index_lookup_.push_back(channel_functor());
      command_lookup_.push_back(channel_functor());
    } else if (e.name == "alias") {
      index_lookup_.push_back(payload_alias_functor());
      command_lookup_.push_back(payload_alias_functor());
    } else if (e.name == "alias-or-command") {
      index_lookup_.push_back(payload_alias_or_command_functor());
      command_lookup_.push_back(payload_alias_or_command_functor());
    } else {
      NSC_LOG_ERROR_STD("Invalid index: " + e.name);
    }
  }
  return true;
}

void SimpleCache::handleNotification(const std::string &channel, const PB::Commands::QueryResponseMessage::Response &request,
                                     PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  std::string key;
  for (index_lookup_function &f : index_lookup_) {
    key += f(channel, request_message.header(), request);
  }
  std::string data = request.SerializeAsString();
  NSC_DEBUG_MSG("Adding to index: " + key);
  {
    boost::unique_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!lock) {
      nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), false, "Failed to get lock");
      return;
    }
    cache_[key] = data;
  }
  nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), true, "message has been cached");
}

void SimpleCache::list_cache(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  cache_query query;
  std::string not_found_msg, not_found_msg_code;
  std::string key;
  po::options_description desc = nscapi::program_options::create_desc(request);

  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;

  std::string data;
  {
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!lock) {
      return nscapi::protobuf::functions::set_response_bad(*response, std::string("Failed to get lock"));
    }
    for (const cache_type::value_type &c : cache_) {
      str::format::append_list(data, c.first);
    }
  }
  response->add_lines()->set_message(data);
  response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(nscapi::plugin_helper::translateReturn(not_found_msg_code)));
}

void SimpleCache::check_cache(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  cache_query query;
  std::string not_found_msg, not_found_msg_code;
  std::string key;
  po::options_description desc = nscapi::program_options::create_desc(request);
  // clang-format off
	desc.add_options()
		("key", po::value<std::string>(&key), "The key (will not be parsed)")
		("host", po::value<std::string>(&query.host), "The host to look for (translates into the key)")
		("command", po::value<std::string>(&query.command), "The command to look for (translates into the key)")
		("channel", po::value<std::string>(&query.channel), "The channel to look for (translates into the key)")
		("alias", po::value<std::string>(&query.alias), "The alias to look for (translates into the key)")
		("not-found-msg", po::value<std::string>(&not_found_msg)->default_value("Entry not found"), "The message to display when a message is not found")
		("not-found-code", po::value<std::string>(&not_found_msg_code)->default_value("unknown"), "The return status to return when a message is not found")
		;
  // clang-format on

  boost::program_options::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;

  if (key.empty()) {
    for (command_lookup_function &f : command_lookup_) {
      key += f(query);
    }
  }
  if (key.empty()) return nscapi::program_options::invalid_syntax(desc, request.command(), "No key specified", *response);
  NSC_DEBUG_MSG("Searching for index: " + key);
  boost::optional<std::string> data;
  {
    boost::shared_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!lock) {
      return nscapi::protobuf::functions::set_response_bad(*response, std::string("Failed to get lock"));
    }
    std::map<std::string, std::string>::const_iterator cit = cache_.find(key);
    if (cit != cache_.end()) data = cit->second;
  }
  if (data) {
    response->ParseFromString(*data);
  } else {
    response->add_lines()->set_message(not_found_msg);
    response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(nscapi::plugin_helper::translateReturn(not_found_msg_code)));
  }
}