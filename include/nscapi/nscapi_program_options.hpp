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

#include <nscapi/nscapi_protobuf_command.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <str/utils.hpp>
#include <utf8.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4512)
#pragma warning(disable : 4100)
#include <boost/program_options.hpp>
#pragma warning(pop)
#pragma warning(push)
#pragma warning(disable : 4505)
#else
#include <boost/program_options.hpp>
#endif
#include <utility>
#include <vector>

namespace nscapi {
namespace program_options {

typedef std::map<std::string, std::string> field_map;

namespace po = boost::program_options;

class program_options_exception : public std::exception {
  std::string error;

 public:
  //////////////////////////////////////////////////////////////////////////
  /// Constructor takes an error message.
  /// @param error the error message
  ///
  /// @author mickem
  explicit program_options_exception(std::string error) : error(std::move(error)) {}
  ~program_options_exception() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////
  /// Retrieve the error message from the exception.
  /// @return the error message
  ///
  /// @author mickem
  const char *what() const noexcept override { return error.c_str(); }
};

std::vector<po::option> option_parser_kvp(std::vector<std::string> &args, const std::string &break_at);

class basic_command_line_parser : public po::basic_command_line_parser<char> {
 public:
  std::vector<std::basic_string<char> > make_vector(const PB::Commands::QueryRequestMessage::Request &request) {
    std::vector<std::basic_string<char> > result;
    for (int i = 0; i < request.arguments_size(); i++) {
      result.push_back(request.arguments(i));
    }
    return result;
  }
  std::vector<std::basic_string<char> > make_vector(const PB::Commands::ExecuteRequestMessage::Request &request) {
    std::vector<std::basic_string<char> > result;
    for (int i = 0; i < request.arguments_size(); i++) {
      result.push_back(request.arguments(i));
    }
    return result;
  }
  std::vector<std::basic_string<char> > make_vector(const std::string &arguments) {
    std::vector<std::basic_string<char> > result;
    str::utils::parse_command(arguments, result);
    return result;
  }
  basic_command_line_parser(const PB::Commands::QueryRequestMessage::Request &request) : po::basic_command_line_parser<char>(make_vector(request)) {}
  basic_command_line_parser(const PB::Commands::ExecuteRequestMessage::Request &request) : po::basic_command_line_parser<char>(make_vector(request)) {}
  basic_command_line_parser(const std::string &arguments) : po::basic_command_line_parser<char>(make_vector(arguments)) {}
  basic_command_line_parser(const std::vector<std::string> &arguments) : po::basic_command_line_parser<char>(arguments) {}
};

void add_help(po::options_description &desc);
po::options_description create_desc(const std::string command);
po::options_description create_desc(const PB::Commands::QueryRequestMessage::Request &request);
po::options_description create_desc(const PB::Commands::ExecuteRequestMessage::Request &request);

/* Given a string 'par', that contains no newline characters
   outputs it to 'os' with wordwrapping, that is, as several
   line.

   Each output line starts with 'indent' space characters,
   following by characters from 'par'. The total length of
   line is no longer than 'line_length'.

*/
void format_paragraph(std::ostream &os, std::string par, std::size_t indent, std::size_t line_length);

void format_description(std::ostream &os, const std::string &desc, std::size_t first_column_width, unsigned line_length);

std::string strip_default_value(const std::string &arg);

std::string help(const po::options_description &desc, const std::string &extra_info = "");

std::string help_short(const po::options_description &desc, const std::string &extra_info = "");

template <class T>
void invalid_syntax(const po::options_description &desc, const std::string &, const std::string &error, T &response) {
  nscapi::protobuf::functions::set_response_bad(response, help_short(desc, error));
}

std::string make_csv(const std::string &s);
std::string help_csv(const po::options_description &desc, const std::string &);
std::string help_pb(const po::options_description &desc, const field_map &fields);
std::string help_pb(const po::options_description &desc);
std::string help_show_default(const po::options_description &desc);

typedef std::vector<std::string> unrecognized_map;

template <class U>
bool du_parse(po::variables_map &vm, const po::options_description &desc, U &response) {
  if (vm.count("show-default")) {
    nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
    return false;
  }
  if (vm.count("help-pb")) {
    nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc));
    return false;
  }
  if (vm.count("help-short")) {
    nscapi::protobuf::functions::set_response_good(response, help_short(desc));
    return false;
  }
  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(response, help(desc));
    return false;
  }
  return true;
}
template <class U>
bool du_parse(po::variables_map &vm, const po::options_description &desc, const field_map &fields, U &response) {
  if (vm.count("show-default")) {
    nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
    return false;
  }
  if (vm.count("help-pb")) {
    nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc, fields));
    return false;
  }
  if (vm.count("help-short")) {
    nscapi::protobuf::functions::set_response_good(response, help_short(desc));
    return false;
  }
  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(response, help(desc));
    return false;
  }
  return true;
}

template <class T, class U>
bool process_arguments_unrecognized(po::variables_map &vm, unrecognized_map &unrecognized, const po::options_description &desc, const T &request, U &response) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    if (request.arguments_size() > 0) {
      const std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.allow_unregistered().run();
    po::store(parsed, vm);
    po::notify(vm);
    unrecognized_map un = po::collect_unrecognized(parsed.options, po::include_positional);
    unrecognized.insert(unrecognized.end(), un.begin(), un.end());

    return du_parse<U>(vm, desc, response);
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(),
                                            "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}
template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    return du_parse<U>(vm, desc, response);
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}

template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map fields, const T &request, U &response) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    return du_parse<U>(vm, desc, fields, response);
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}

template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response,
                                    po::positional_options_description p) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    cmd.positional(p);

    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-')) cmd.extra_style_parser([&p](auto &args) { return option_parser_kvp(args, p.name_for_position(0)); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    return du_parse<U>(vm, desc, response);
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(),
                                            "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}

template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map &fields, const T &request, U &response,
                                    const po::positional_options_description &p) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    cmd.positional(p);

    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-')) cmd.extra_style_parser([&p](auto &args) { return option_parser_kvp(args, p.name_for_position(0)); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    return du_parse<U>(vm, desc, fields, response);
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(),
                                            "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}

template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response, bool allow_unknown,
                                    std::vector<std::string> &extra) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    if (allow_unknown) cmd.allow_unregistered();

    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (!du_parse<U>(vm, desc, response)) {
      return false;
    }
    if (allow_unknown) {
      std::vector<std::string> extra2 = po::collect_unrecognized(parsed.options, po::include_positional);
      extra.insert(extra.begin(), extra2.begin(), extra2.end());
    }
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(),
                                            "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}
template <class T, class U>
bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map &fields, const T &request, U &response,
                                    bool allow_unknown, std::vector<std::string> &extra) {
  try {
    basic_command_line_parser cmd(request);
    cmd.options(desc);
    if (allow_unknown) cmd.allow_unregistered();

    if (request.arguments_size() > 0) {
      std::string a = request.arguments(0);
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (!du_parse<U>(vm, desc, fields, response)) {
      return false;
    }
    if (allow_unknown) {
      std::vector<std::string> extra2 = po::collect_unrecognized(parsed.options, po::include_positional);
      extra.insert(extra.begin(), extra2.begin(), extra2.end());
    }
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(),
                                            "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
    return false;
  }
}
template <class T>
bool process_arguments_from_vector(po::variables_map &vm, const po::options_description &desc, const std::string &command,
                                   const std::vector<std::string> &arguments, T &response) {
  try {
    basic_command_line_parser cmd(arguments);
    cmd.options(desc);
    if (arguments.size() > 0) {
      std::string a = arguments[0];
      if (a.size() <= 2 || (a[0] != '-' && a[1] != '-')) cmd.extra_style_parser([](auto &args) { return option_parser_kvp(args, ""); });
    }

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("show-default")) {
      nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
      return false;
    }
    if (vm.count("help-pb")) {
      nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc));
      return false;
    }
    if (vm.count("help-short")) {
      nscapi::protobuf::functions::set_response_good(response, help_short(desc));
      return false;
    }
    if (vm.count("help")) {
      nscapi::protobuf::functions::set_response_good(response, help(desc));
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, command, "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()),
                                            response);
    return false;
  }
}

struct alias_option {
  std::string key;
  std::string alias;
  std::string value;
};
struct standard_filter_config {
  std::string filter_string;
  std::string warn_string;
  std::string crit_string;
  std::string ok_string;
  std::string syntax_top;
  std::string syntax_ok;
  std::string syntax_empty;
  std::string syntax_detail;
  std::string empty_state;
};

void add_standard_filter(po::options_description &desc, standard_filter_config &filter, std::string default_top_syntax, std::string top_keylist,
                         std::string default_syntax, std::string keylist);
}  // namespace program_options
}  // namespace nscapi
#ifdef WIN32
#pragma warning(pop)
#endif
