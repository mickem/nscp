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

#include <client/command_line_parser.hpp>

#include <nscapi/nscapi_protobuf_nagios.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_metrics.hpp>

#include <utf8.hpp>

#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#pragma warning(disable : 4100)
#pragma warning(disable : 4101)
#pragma warning(disable : 4456)
#endif

namespace po = boost::program_options;
namespace ph = boost::placeholders;

struct payload_builder {
  enum types { type_submit, type_query, type_exec, type_none };

  ::PB::Commands::SubmitRequestMessage submit_message;
  ::PB::Commands::QueryResponseMessage::Response *submit_payload;

  ::PB::Commands::ExecuteRequestMessage exec_message;
  ::PB::Commands::ExecuteRequestMessage::Request *exec_payload;

  ::PB::Commands::QueryRequestMessage query_message;
  ::PB::Commands::QueryRequestMessage::Request *query_payload;

  types type;
  std::string separator;
  payload_builder() : submit_payload(nullptr), exec_payload(nullptr), query_payload(nullptr), type(type_none), separator("|") {}

  void set_type(types type_) { type = type_; }

  void set_separator(const std::string &value) { separator = value; }
  bool is_query() const { return type == type_query; }
  bool is_exec() const { return type == type_exec; }
  bool is_submit() const { return type == type_submit; }

  void set_result(const std::string &value);
  void set_message(const std::string &value) {
    if (is_submit()) {
      PB::Commands::QueryResponseMessage::Response::Line *l = get_submit_payload()->add_lines();
      l->set_message(value);
    } else if (is_exec()) {
      throw client::cli_exception("message not supported for exec");
    } else {
      throw client::cli_exception("message not supported for query");
    }
  }
  void set_command(const std::string& value) {
    if (is_submit()) {
      get_submit_payload()->set_command(value);
    } else if (is_exec()) {
      get_exec_payload()->set_command(value);
    } else {
      get_query_payload()->set_command(value);
    }
  }
  void set_arguments(const std::vector<std::string> &value) {
    if (is_submit()) {
      throw client::cli_exception("arguments not supported for submit");
    } else if (is_exec()) {
      for (const std::string &a : value) get_exec_payload()->add_arguments(a);
    } else {
      for (const std::string &a : value) get_query_payload()->add_arguments(a);
    }
  }
  void set_batch(const std::vector<std::string> &data);

 private:
  ::PB::Commands::QueryResponseMessage::Response *get_submit_payload() {
    if (submit_payload == nullptr) submit_payload = submit_message.add_payload();
    return submit_payload;
  }
  ::PB::Commands::QueryRequestMessage::Request *get_query_payload() {
    if (query_payload == nullptr) query_payload = query_message.add_payload();
    return query_payload;
  }
  ::PB::Commands::ExecuteRequestMessage::Request *get_exec_payload() {
    if (exec_payload == nullptr) exec_payload = exec_message.add_payload();
    return exec_payload;
  }
};

std::string client::destination_container::to_string() const {
  std::stringstream ss;
  ss << "address: " << address.to_string() << ", timeout: " << timeout << ", retry: " << retry << ", data: { ";
  for (const data_map::value_type &t : data) {
    ss << t.first << ": " << t.second << ", ";
  }
  ss << "}";
  return ss.str();
}

void client::options_reader_interface::add_ssl_options(boost::program_options::options_description &desc, client::destination_container &data) {
  // clang-format off
  desc.add_options()
  ("certificate", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("certificate", v); }),
	"The client certificate to use")
    ("dh", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("dh", v); }),
	"The DH key to use")
    ("certificate-key", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("certificate key", v); }),
	"Client certificate to use")
    ("certificate-format", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("certificate format", v); }),
	"Client certificate format")
    ("ca", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("ca", v); }),
	"Certificate authority")
    ("verify", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("verify mode", v); }),
	"Client certificate format")
    ("allowed-ciphers", po::value<std::string>()->notifier([&data](const auto& v) { data.set_string_data("allowed ciphers", v); }),
	"Client certificate format")
    ("ssl,n", po::value<bool>()->implicit_value(true)->notifier([&data](const bool& v) { data.set_bool_data("ssl", v); }),
	"Initial an ssl handshake with the server.")
    ;
  // clang-format on
}

po::options_description add_common_options(client::destination_container &source, client::destination_container &destination) {
  // clang-format off
  po::options_description desc("Common options");
  desc.add_options()
    ("host,H", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_host(v); }),
	"The host of the host running the server")
    ("port,P", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_port(v); }),
	"The port of the host running the server")
    ("address", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_address(v); }),
	"The address (host:port) of the host running the server")
    ("timeout,T", po::value<int>()->notifier([&destination](const int& v) { destination.set_int_data("timeout", v); }),
	"Number of seconds before connection times out (default=10)")
    ("target,t", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_string_data("$target.id$", v); }),
	"Target to use (lookup connection info from config)")
    ("retry", po::value<int>()->notifier([&destination](const int& v) { destination.set_int_data("retry", v); }),
	"Number of times ti retry a failed connection attempt (default=2)")
    ("retries", po::value<int>()->notifier([&destination](const int& v) { destination.set_int_data("retry", v); }),
	"legacy version of retry")
    ("source-host", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_string_data("host", v); }),
	"Source/sender host name (default is auto which means use the name of the actual host)")
    ("sender-host", po::value<std::string>()->notifier([&destination](const auto& v) { destination.set_string_data("host", v); }),
	"Source/sender host name (default is auto which means use the name of the actual host)")
    ;
  // clang-format on
  return desc;
}
po::options_description add_query_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
  // clang-format off
  po::options_description desc("Query options");
  desc.add_options()
    ("command,c", po::value<std::string >()->notifier([&builder](const auto& v) { builder.set_command(v); }),
	"The name of the command that the remote daemon should run")
    ("argument,a", po::value<std::vector<std::string> >()->notifier([&builder](const auto& v) { builder.set_arguments(v); }),
	"Set command line arguments")
    ("separator", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_separator(v); }),
	"Separator to use for the batch command (default is |)")
    ("batch", po::value<std::vector<std::string> >()->notifier([&builder](const auto& v) { builder.set_batch(v); }),
	"Add multiple records using the separator format is: command|argument|argument")
    ;
  // clang-format on
  return desc;
}
po::options_description add_submit_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
  po::options_description desc("Submit options");
  // clang-format off
  desc.add_options()
    ("command,c", po::value<std::string >()->notifier([&builder](const auto& v) { builder.set_command(v); }),
	"The name of the command that the remote daemon should run")
    ("alias,a", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_command(v); }),
	"Same as command")
    ("message,m", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_message(v); }),
	"Message")
    ("result,r", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_result(v); }),
	"Result code either a number or OK, WARN, CRIT, UNKNOWN")
    ("separator", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_separator(v); }),
	"Separator to use for the batch command (default is |)")
    ("batch", po::value<std::vector<std::string> >()->notifier([&builder](const auto& v) { builder.set_batch(v); }),
	"Add multiple records using the separator format is: command|result|message")
    ;
  // clang-format on
  return desc;
}
po::options_description add_exec_options(client::destination_container &source, client::destination_container &destination, payload_builder &builder) {
  po::options_description desc("Execute options");
  // clang-format off
  desc.add_options()
    ("command,c", po::value<std::string >()->notifier([&builder](const auto& v) { builder.set_command(v); }),
	"The name of the command that the remote daemon should run")
    ("argument", po::value<std::vector<std::string> >()->notifier([&builder](const auto& v) { builder.set_arguments(v); }),
	"Set command line arguments")
    ("separator", po::value<std::string>()->notifier([&builder](const auto& v) { builder.set_separator(v); }),
	"Separator to use for the batch command (default is |)")
    ("batch", po::value<std::vector<std::string> >()->notifier([&builder](const auto& v) { builder.set_batch(v); }),
	"Add multiple records using the separator format is: command|argument|argument")
    ;
  // clang-format on
  return desc;
}

std::string client::configuration::add_command(const std::string& name, const std::string &args) {
  command_container data;
  bool first = true;
  for (const std::string &s : str::utils::parse_command(args)) {
    if (first) {
      data.command = s;
      first = false;
    } else {
      data.arguments.push_back(s);
    }
  }

  std::string key = boost::algorithm::to_lower_copy(name);
  data.key = key;
  commands[data.key] = data;
  return key;
}

client::destination_container client::configuration::get_target(const std::string& name) const {
  destination_container d;
  object_handler_type::object_instance op = targets.find_object(name);
  if (op) {
    d.apply(op);
  } else {
    op = targets.find_object("default");
    if (op) d.apply(op);
  }
  return d;
}

client::destination_container client::configuration::get_sender() const {
  destination_container s;
  s.set_address(default_sender);
  return s;
}

void client::configuration::do_query(const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response) {
  PB::Commands::QueryResponseMessage local_response;

  std::string target = "default";
  if (!request.header().recipient_id().empty())
    target = request.header().recipient_id();
  else if (!request.header().destination_id().empty())
    target = request.header().destination_id();

  for (const std::string& t : str::utils::split_lst(target, std::string(","))) {
    destination_container d = get_target(t);
    destination_container s = get_sender();

    // Next apply the header object
    d.apply(t, request.header());
    s.apply(request.header().sender_id(), request.header());
    std::string command = request.header().command();

    if (!command.empty()) {
      // If we have a header command treat the data as a batch
      i_do_query(s, d, command, request, response, true);
    } else {
      // Parse each objects command and execute them
      for (int i = 0; i < request.payload_size(); i++) {
        ::PB::Commands::QueryRequestMessage local_request_message;
        const ::PB::Commands::QueryRequestMessage::Request &local_request = request.payload(i);
        local_request_message.mutable_header()->CopyFrom(request.header());
        local_request_message.add_payload()->CopyFrom(local_request);
        const std::string& command_name = local_request.command();
        ::PB::Commands::QueryResponseMessage local_response_message;
        i_do_query(s, d, command_name, local_request_message, local_response_message, false);
        for (int j = 0; j < local_response_message.payload_size(); j++) {
          response.add_payload()->CopyFrom(local_response_message.payload(j));
        }
      }
    }
  }
}

po::options_description client::configuration::create_descriptor(const std::string& command, client::destination_container &source,
                                                                 client::destination_container &destination) const {
  po::options_description desc = nscapi::program_options::create_desc(command);
  desc.add(add_common_options(source, destination));
  if (client_desc) desc.add(client_desc(source, destination));
  return desc;
}

void client::configuration::i_do_query(destination_container &s, destination_container &d, std::string command,
                                       const PB::Commands::QueryRequestMessage &request, PB::Commands::QueryResponseMessage &response, bool use_header) {
  try {
    bool custom_command = false;

    command_type::const_iterator cit = commands.find(command);
    if (cit != commands.end()) {
      command = cit->second.command;
      custom_command = true;
      // TODO: Build argument vector here!
    }
    if (command.substr(0, 8) == "forward_" || command.substr(command.size() - 8, 8) == "_forward") {
      for (const PB::Commands::QueryRequestMessage::Request &p : request.payload()) {
        if (p.arguments_size() > 0) {
          for (const std::string &a : p.arguments()) {
            if (a == "help-pb") {
              ::PB::Registry::ParameterDetails details;
              ::PB::Registry::ParameterDetail *td = details.add_parameter();
              td->set_name("*");
              td->set_short_description("This command will forward all arguments to remote system");
              nscapi::protobuf::functions::set_response_good_wdata(*response.add_payload(), details.SerializeAsString());
              return;
            }
          }
        }
      }
      if (!handler->query(s, d, request, response)) nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
    } else {
      po::options_description desc = create_descriptor(command, s, d);
      payload_builder builder;
      std::string x = command.substr(command.size() - 6, 6);
      if (command.substr(0, 6) == "check_" || command.substr(command.size() - 6, 6) == "_query") {
        builder.set_type(payload_builder::type_query);
        desc.add(add_query_options(s, d, builder));
      } else if (command.substr(0, 5) == "exec_") {
        builder.set_type(payload_builder::type_exec);
        desc.add(add_exec_options(s, d, builder));
      } else if (command.substr(0, 7) == "submit_") {
        builder.set_type(payload_builder::type_submit);
        desc.add(add_submit_options(s, d, builder));
      } else {
        return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
      }
      reader->process(desc, s, d);
      if (custom_command) {
        // TODO: Parse argument vector here
      } else if (use_header) {
        // TODO: Parse header here
      } else {
        boost::program_options::variables_map vm;
        for (int i = 0; i < request.payload_size(); i++) {
          ::PB::Commands::QueryResponseMessage::Response resp;
          // Apply any arguments from command line
          po::positional_options_description p;
          p.add("argument", -1);

          if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp, p)) {
            response.add_payload()->CopyFrom(resp);
            return;
          }
        }
      }
      if (client_pre) {
        if (!client_pre(s, d)) return;
      }

      if (builder.is_query()) {
        PB::Commands::QueryResponseMessage local_response;
        if (!handler->query(s, d, builder.query_message, local_response)) {
          return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
        }
        for (const ::PB::Commands::QueryResponseMessage::Response &td : local_response.payload()) {
          response.add_payload()->CopyFrom(td);
        }
      } else if (builder.is_exec()) {
        PB::Commands::ExecuteResponseMessage local_response;
        if (!handler->exec(s, d, builder.exec_message, local_response)) {
          return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
        }
        for (const ::PB::Commands::ExecuteResponseMessage::Response &td : local_response.payload()) {
          nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
        }
        // TODO: Convert reply to native reply
      } else if (builder.is_submit()) {
        PB::Commands::SubmitResponseMessage local_response;
        if (!handler->submit(s, d, builder.submit_message, local_response)) {
          return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
        }
        for (const ::PB::Commands::SubmitResponseMessage::Response& td : local_response.payload()) {
          nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
        }
      } else {
        return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
      }
    }
  } catch (const std::exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
  }
}

bool client::configuration::do_exec(const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response,
                                    const std::string &default_command_arg) {
  PB::Commands::ExecuteResponseMessage local_response;

  std::string target = "default";
  if (!request.header().recipient_id().empty())
    target = request.header().recipient_id();
  else if (!request.header().destination_id().empty())
    target = request.header().destination_id();

  for (const std::string t : str::utils::split_lst(target, std::string(","))) {
    destination_container d = get_target(t);
    destination_container s = get_sender();

    // Next apply the header object
    d.apply(t, request.header());
    s.apply(request.header().sender_id(), request.header());

    if (d.has_data("command")) {
      std::string command = d.get_string_data("command");
      // If we have a header command treat the data as a batch
      return i_do_exec(s, d, command, request, response, true);
    } else {
      bool found = false;
      // Parse each objects command and execute them
      for (int i = 0; i < request.payload_size(); i++) {
        ::PB::Commands::ExecuteRequestMessage local_request_message;
        const ::PB::Commands::ExecuteRequestMessage::Request &local_request = request.payload(i);
        local_request_message.mutable_header()->CopyFrom(request.header());
        local_request_message.add_payload()->CopyFrom(local_request);
        std::string command = local_request.command();
        if (command.empty()) command = default_command_arg;
        ::PB::Commands::ExecuteResponseMessage local_response_message;
        if (i_do_exec(s, d, command, local_request_message, local_response_message, false)) {
          found = true;
        }
        for (int j = 0; j < local_response_message.payload_size(); j++) {
          response.add_payload()->CopyFrom(local_response_message.payload(j));
        }
      }
      if (!found) {
        nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "failed");
      }
      return found;
    }
  }
  return false;
}

bool client::configuration::i_do_exec(destination_container &s, destination_container &d, std::string command,
                                      const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response, bool use_header) {
  try {
    bool custom_command = false;

    command_type::const_iterator cit = commands.find(command);
    if (cit != commands.end()) {
      command = cit->second.command;
      custom_command = true;
      // TODO: Build argument vector here!
    }
    if (command.substr(0, 8) == "forward_") {
      if (!handler->exec(s, d, request, response)) {
        nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
        return true;
      }
    } else {
      boost::program_options::variables_map vm;
      po::options_description desc = create_descriptor(command, s, d);
      payload_builder builder;
      if (command.substr(0, 6) == "check_" || command.empty()) {
        builder.set_type(payload_builder::type_query);
        desc.add(add_query_options(s, d, builder));
      } else if (command.substr(0, 5) == "exec_") {
        builder.set_type(payload_builder::type_exec);
        desc.add(add_exec_options(s, d, builder));
      } else if (command.substr(0, 7) == "submit_" || command.substr(command.size() - 7, 7) == "_submit") {
        builder.set_type(payload_builder::type_submit);
        desc.add(add_submit_options(s, d, builder));
      } else {
        nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Module does not know of any command called: " + command);
        return false;
      }
      reader->process(desc, s, d);
      if (custom_command) {
        // TODO: Parse argument vector here
      } else if (use_header) {
        // TODO: Parse header here
      } else {
        for (int i = 0; i < request.payload_size(); i++) {
          ::PB::Commands::ExecuteResponseMessage::Response resp;
          // Apply any arguments from command line
          // TODO: This is broken as it overwrite the source/targets
          if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
            response.add_payload()->CopyFrom(resp);
            return true;
          }
        }
      }
      if (d.has_data("$target.id$")) {
        std::string t = d.get_string_data("$target.id$");

        // If we have a target, apply it
        object_handler_type::object_instance op = targets.find_object(t);
        if (op) {
          d.apply(op);

          // Next apply the header object
          d.apply(t, request.header());
        }

        // If we have --target speciied apply the target and reapply the command line
        if (custom_command) {
          // TODO: Parse argument vector here
        } else if (use_header) {
          // TODO: Parse header here
        } else {
          for (int i = 0; i < request.payload_size(); i++) {
            ::PB::Commands::ExecuteResponseMessage::Response resp;
            // Apply any arguments from command line
            // TODO: This is broken as it overwrite the source/targets
            if (!nscapi::program_options::process_arguments_from_request(vm, desc, request.payload(i), resp)) {
              response.add_payload()->CopyFrom(resp);
              return true;
            }
          }
        }
      }

      if (builder.type == payload_builder::type_query) {
        PB::Commands::QueryResponseMessage local_response;
        if (!handler->query(s, d, builder.query_message, local_response)) {
          nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
          return true;
        }
        for (const ::PB::Commands::QueryResponseMessage::Response& td : local_response.payload()) {
          nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
        }
      } else if (builder.type == payload_builder::type_exec) {
        PB::Commands::ExecuteResponseMessage local_response;
        if (!handler->exec(s, d, builder.exec_message, local_response)) {
          nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
          return true;
        }
        for (const ::PB::Commands::ExecuteResponseMessage::Response &td : local_response.payload()) {
          response.add_payload()->CopyFrom(td);
        }
      } else if (builder.type == payload_builder::type_submit) {
        PB::Commands::SubmitResponseMessage local_response;
        if (!handler->submit(s, d, builder.submit_message, local_response)) {
          nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
          return true;
        }
        for (const ::PB::Commands::SubmitResponseMessage::Response &td : local_response.payload()) {
          nscapi::protobuf::functions::copy_response(command, response.add_payload(), td);
        }
      }
    }
    return true;
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
    return true;
  }
}

void client::configuration::do_submit_item(const PB::Commands::SubmitRequestMessage &request, const destination_container& s, destination_container d,
                                           PB::Commands::SubmitResponseMessage &response) {
  // Parse each objects command and execute them
  for (const ::PB::Commands::QueryResponseMessage::Response &local_request : request.payload()) {
    ::PB::Commands::SubmitRequestMessage local_request_message;
    local_request_message.mutable_header()->CopyFrom(request.header());
    local_request_message.add_payload()->CopyFrom(local_request);
    ::PB::Commands::SubmitResponseMessage local_response_message;
    i_do_submit(s, d, "forward_raw", local_request_message, local_response_message, false);
    for (const ::PB::Commands::SubmitResponseMessage_Response &p : local_response_message.payload()) {
      response.add_payload()->CopyFrom(p);
    }
  }
}

void client::configuration::do_submit(const PB::Commands::SubmitRequestMessage &request, PB::Commands::SubmitResponseMessage &response) {
  PB::Commands::ExecuteResponseMessage local_response;

  std::string target = "default";
  if (!request.header().recipient_id().empty()) {
    std::string r = request.header().recipient_id();
    target = request.header().recipient_id();
  } else if (!request.header().destination_id().empty())
    target = request.header().destination_id();

  for (const std::string &t : str::utils::split_lst(target, std::string(","))) {
    destination_container d = get_target(t);
    destination_container s = get_sender();

    // Next apply the header object
    d.apply(t, request.header());
    s.apply(request.header().sender_id(), request.header());

    if (d.has_data("command")) {
      std::string command = d.get_string_data("command");
      // If we have a header command treat the data as a batch
      i_do_submit(s, d, command, request, response, true);
    } else {
      do_submit_item(request, s, d, response);
    }
  }
}

void client::configuration::i_do_submit(const destination_container &s, destination_container &d, std::string command,
                                        const PB::Commands::SubmitRequestMessage &request, PB::Commands::SubmitResponseMessage &response, bool use_header) {
  try {
    boost::program_options::variables_map vm;

    const command_type::const_iterator cit = commands.find(command);
    if (cit != commands.end()) {
      command = cit->second.command;
      // TODO: Build argument vector here!
    }
    if (command.substr(0, 8) == "forward_") {
      if (!handler->submit(s, d, request, response)) return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " failed");
    } else {
      return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), command + " not found");
    }
  } catch (const std::exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response.add_payload(), "Exception processing command line: " + utf8::utf8_from_native(e.what()));
  }
}

void client::configuration::do_metrics(const PB::Metrics::MetricsMessage &request) const {
  std::string target = "default";
  if (!request.header().recipient_id().empty())
    target = request.header().recipient_id();
  else if (!request.header().destination_id().empty())
    target = request.header().destination_id();

  for (const std::string& t : str::utils::split_lst(target, std::string(","))) {
    destination_container d = get_target(t);
    destination_container s = get_sender();

    // Next apply the header object
    d.apply(t, request.header());
    s.apply(request.header().sender_id(), request.header());

    handler->metrics(s, d, request);
  }
}

void client::configuration::finalize(const boost::shared_ptr<nscapi::settings_proxy>& settings) {
  targets.add_samples(settings);
  targets.add_missing(settings, "default", "");
}
void payload_builder::set_result(const std::string &value) {
  if (is_submit()) {
    get_submit_payload()->set_result(nscapi::protobuf::functions::parse_nagios(value));
  } else if (is_exec()) {
    throw client::cli_exception("result not supported for exec");
  } else {
    throw client::cli_exception("result not supported for query");
  }
}

void payload_builder::set_batch(const std::vector<std::string> &data) {
  if (is_submit()) {
    for (const std::string &e : data) {
      submit_payload = submit_message.add_payload();
      std::vector<std::string> line;
      boost::iter_split(line, e, boost::algorithm::first_finder(separator));
      if (line.size() >= 3) set_message(line[2]);
      if (line.size() >= 2) set_result(line[1]);
      if (!line.empty()) set_command(line[0]);
    }
  } else if (type == type_exec) {
    for (const std::string &e : data) {
      exec_payload = exec_message.add_payload();
      std::list<std::string> line;
      boost::iter_split(line, e, boost::algorithm::first_finder(separator));
      if (!line.empty()) {
        set_command(line.front());
        line.pop_front();
      }
      for (const std::string &a : line) {
        get_exec_payload()->add_arguments(a);
      }
    }
  } else {
    for (const std::string &e : data) {
      query_payload = query_message.add_payload();
      std::list<std::string> line;
      boost::iter_split(line, e, boost::algorithm::first_finder(separator));
      if (!line.empty()) {
        set_command(line.front());
        line.pop_front();
      }
      for (const std::string &a : line) {
        get_query_payload()->add_arguments(a);
      }
    }
  }
}
