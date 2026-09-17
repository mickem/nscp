// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "SimpleFileWriter.h"

#include <boost/date_time.hpp>
#include <boost/thread/lock_types.hpp>
#include <fstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nscapi/protobuf/nagios.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <ostream>
#include <parsers/expression/expression.hpp>
#include <str/utf8.hpp>
#include <vector>

namespace sh = nscapi::settings_helper;

void build_syntax(parsers::simple_expression &parser, std::string &syntax, SimpleFileWriter::index_lookup_type &index);

struct simple_string_functor {
  std::string value;
  simple_string_functor(std::string value) : value(value) {}
  simple_string_functor(const simple_string_functor &other) : value(other.value) {}
  const simple_string_functor &operator=(const simple_string_functor &other) {
    value = other.value;
    return *this;
  }
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &) {
    return value;
  }
};
struct header_host_functor {
  std::string operator()(const config_object &, const std::string channel, const PB::Common::Header &hdr,
                         const PB::Commands::QueryResponseMessage::Response &) {
    std::string sender = hdr.sender_id();
    for (const PB::Common::Host &h : hdr.hosts()) {
      if (h.id() == sender) return h.host();
    }
    return sender;
  }
};
struct payload_command_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    return payload.command();
  }
};
struct channel_functor {
  std::string operator()(const config_object &, const std::string channel, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &) {
    return channel;
  }
};
struct payload_alias_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    return payload.alias();
  }
};
struct payload_message_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    std::string ret;
    for (PB::Commands::QueryResponseMessage::Response::Line l : payload.lines()) ret += l.message();
    return ret;
  }
};
struct payload_result_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    return nscapi::plugin_helper::translateReturn(nscapi::protobuf::functions::gbp_to_nagios_status(payload.result()));
  }
};
struct payload_result_nr_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    return str::xtos(nscapi::protobuf::functions::gbp_to_nagios_status(payload.result()));
  }
};
struct payload_alias_or_command_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    if (!payload.alias().empty()) return payload.alias();
    return payload.command();
  }
};

struct epoch_functor {
  std::string operator()(const config_object &, const std::string, const PB::Common::Header &, const PB::Commands::QueryResponseMessage::Response &payload) {
    boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));
    boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
    boost::posix_time::time_duration diff = now - time_t_epoch;
    return str::xtos(diff.total_seconds());
  }
};

struct time_functor {
  std::string operator()(const config_object &config, const std::string, const PB::Common::Header &,
                         const PB::Commands::QueryResponseMessage::Response &payload) {
    std::stringstream ss;
    boost::posix_time::time_facet *facet = new boost::posix_time::time_facet(config.time_format.c_str());
    ss.imbue(std::locale(std::cout.getloc(), facet));
    ss << boost::posix_time::second_clock::local_time();
    return ss.str();
  }
};

std::string simple_string_fun(std::string key) { return key; }
bool SimpleFileWriter::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  std::string syntax;
  std::string syntax_host;
  std::string syntax_service;
  std::string channel;
  // Built from scratch every time and published at the end, so a reload
  // replaces the syntax rather than appending another copy of it to what is
  // already there.
  writer_config fresh;
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));

    // clang-format off
		settings.set_alias(alias, "writers/file");

		settings.alias().add_path_to_settings()
			("FILE WRITER", "Section for simple file writer module (SimpleFileWriter.dll).")

			;

    // clang-format on
    settings.alias()
        .add_key_to_settings()
        .add_string("syntax", sh::string_key(&syntax, "${alias-or-command} ${result} ${message}"), "MESSAGE SYNTAX",
                    "The syntax of the message to write to the line.\nCan be any arbitrary string as well as include any of the following special keywords:"
                    "${command} = The command name, ${host} the host, ${channel} the receiving channel, ${alias} the alias for the command, "
                    "${alias-or-command} = alias if set otherwise command, ${message} = the message data (no escape), ${result} or ${result_number} = The "
                    "result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.")

        .add_string("service-syntax", sh::string_key(&syntax_service), "SERVICE MESSAGE SYNTAX",
                    "The syntax of the message to write to the line.\nCan be any arbitrary string as well as include any of the following special keywords:"
                    "${command} = The command name, ${host} the host, ${channel} the receiving channel, ${alias} the alias for the command, "
                    "${alias-or-command} = alias if set otherwise command, ${message} = the message data (no escape), ${result} or ${result_number} = The "
                    "result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.")

        .add_string("host-syntax", sh::string_key(&syntax_host), "HOST MESSAGE SYNTAX",
                    "The syntax of the message to write to the line.\nCan be any arbitrary string as well as include any of the following special keywords:"
                    "${command} = The command name, ${host} the host, ${channel} the receiving channel, ${alias} the alias for the command, "
                    "${alias-or-command} = alias if set otherwise command, ${message} = the message data (no escape), ${result} or ${result_number} = The "
                    "result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.")

        .add_file("file", sh::path_key(&fresh.filename, "output.txt"), "FILE TO WRITE TO", "The filename to write output to.")

        .add_string("channel", sh::string_key(&channel, "FILE"), "CHANNEL", "The channel to listen to.")

        .add_string("time-syntax", sh::string_key(&fresh.config.time_format, "%Y-%m-%d %H:%M:%S"), "TIME SYNTAX",
                    "The date format using strftime format flags. This is the time of writing the message as messages currently does not have a source time.");

    settings.register_all();
    settings.notify();

    nscapi::core_helper core(get_core(), get_id());
    core.register_channel(channel);

    if (syntax_host.empty()) {
      syntax_host = syntax;
    }
    if (syntax_service.empty()) {
      syntax_service = syntax;
    }
    parsers::simple_expression parser;
    build_syntax(parser, syntax_host, fresh.syntax_host_lookup);
    build_syntax(parser, syntax_service, fresh.syntax_service_lookup);

    config_.set(std::move(fresh));

  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("Failed to register command: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("load", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("load");
    return false;
  }
  return true;
}

void build_syntax(parsers::simple_expression &parser, std::string &syntax, SimpleFileWriter::index_lookup_type &index) {
  parsers::simple_expression::result_type result;
  if (!parser.parse(syntax, result)) {
    NSC_LOG_ERROR_STD("Failed to parse syntax: " + syntax)
  }
  for (parsers::simple_expression::entry &e : result) {
    if (!e.is_variable) {
      index.push_back(simple_string_functor(e.name));
    } else if (e.name == "command") {
      index.push_back(payload_command_functor());
    } else if (e.name == "host") {
      index.push_back(header_host_functor());
    } else if (e.name == "channel") {
      index.push_back(channel_functor());
    } else if (e.name == "alias") {
      index.push_back(payload_alias_functor());
    } else if (e.name == "alias-or-command") {
      index.push_back(payload_alias_or_command_functor());
    } else if (e.name == "message") {
      index.push_back(payload_message_functor());
    } else if (e.name == "result") {
      index.push_back(payload_result_functor());
    } else if (e.name == "result_number") {
      index.push_back(payload_result_nr_functor());
    } else if (e.name == "epoch") {
      index.push_back(epoch_functor());
    } else if (e.name == "time") {
      index.push_back(time_functor());
    } else {
      NSC_LOG_ERROR_STD("Invalid index: " + e.name);
    }
  }
}

void SimpleFileWriter::handleNotification(const std::string &, const PB::Commands::QueryResponseMessage::Response &request,
                                          PB::Commands::SubmitResponseMessage::Response *response, const PB::Commands::SubmitRequestMessage &request_message) {
  std::string key;

  // One snapshot for the whole submission: the syntax that renders the line and
  // the file it is written to come from the same generation.
  const std::shared_ptr<const writer_config> cfg = config_.get();
  const index_lookup_type &lookup =
      (!request.alias().empty() || !request.command().empty()) ? cfg->syntax_service_lookup : cfg->syntax_host_lookup;
  for (const index_lookup_function &f : lookup) {
    key += f(cfg->config, request.command(), request_message.header(), request);
  }
  {
    boost::unique_lock<boost::shared_mutex> lock(cache_mutex_);
    if (!lock) {
      nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), false, "Failed to get lock");
      return;
    }
    std::ofstream out;
    out.open(cfg->filename.c_str(), std::ios::out | std::ios::app);
    out << key << std::endl;
  }
  nscapi::protobuf::functions::append_simple_submit_response_payload(response, request.command(), true, "message has been written");
}