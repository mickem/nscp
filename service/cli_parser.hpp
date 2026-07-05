// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>

#include "NSClient++.h"

namespace po = boost::program_options;
class cli_parser : public boost::noncopyable {
  std::shared_ptr<NSClient> core_;
  po::options_description common_light;
  po::options_description common;
  po::options_description settings;
  po::options_description service;
  po::options_description client;
  po::options_description test;

  bool help;
  bool version;
  bool log_debug;
  std::string log_backend;
  bool no_stderr;
  std::vector<std::string> log_level;
  std::vector<std::string> defines;
  // Raw KEY=VALUE strings collected from --path-override arguments. Parsed into a map
  // and pushed to NSClient (and through to path_manager) in
  // process_common_options.
  std::vector<std::string> path_overrides;
  std::string settings_store;
  std::vector<std::string> unknown_options;

  typedef boost::function<int(int, char**)> handler_function;
  typedef std::map<std::string, handler_function> handler_map;
  typedef std::map<std::string, std::string> alias_map;

  void init_logger() const;
  handler_map get_handlers();
  static alias_map get_aliases();
  bool process_common_options(const std::string& context, const po::options_description& desc);

 public:
  cli_parser(const std::shared_ptr<NSClient>& core);
  int parse(int argc, char* argv[]);

 private:
  po::basic_parsed_options<char> do_parse(int argc, char* argv[], const po::options_description& desc);

  void display_help();
  int parse_help(int argc, char* argv[]);
  int parse_test(int argc, char* argv[]);
  int parse_settings(int argc, char* argv[]);
  int parse_service(int argc, char* argv[]);
  int parse_client(int argc, char* argv[], const std::string& module_ = "");
  int parse_unittest(int argc, char* argv[]);
  // int exec_client_mode(client_arguments &args);
  static std::string get_description(const std::string& key);
  static std::string describe(const std::string& key);
  static std::string describe(const std::string& key, const std::string& alias);
};