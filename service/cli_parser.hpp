/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <boost/program_options.hpp>
#include <boost/noncopyable.hpp>
#include "NSClient++.h"

namespace po = boost::program_options;
class cli_parser : public boost::noncopyable {
	NSClient* core_;
	po::options_description common_light;
	po::options_description common;
	po::options_description settings;
	po::options_description service;
	po::options_description client;
	po::options_description unittest;
	po::options_description test;

	bool help;
	bool version;
	bool log_debug;
	bool no_stderr;
	std::vector<std::string> log_level;
	std::vector<std::string> defines;
	std::string settings_store;
	std::vector<std::string> unknown_options;

	typedef boost::function<int(int, char**)> handler_function;
	typedef std::map<std::string, handler_function> handler_map;
	typedef std::map<std::string, std::string> alias_map;

	void init_logger();
	handler_map get_handlers();
	alias_map get_aliases();
	bool process_common_options(std::string context, po::options_description &desc);

public:
	cli_parser(NSClient* core);
	int parse(int argc, char* argv[]);

private:
	po::basic_parsed_options<char> do_parse(int argc, char* argv[], po::options_description &desc);

	void display_help();
	int parse_help(int argc, char* argv[]);
	int parse_test(int argc, char* argv[]);
	int parse_settings(int argc, char* argv[]);
	int parse_service(int argc, char* argv[]);
	int parse_client(int argc, char* argv[], std::string module_ = "");
	int parse_unittest(int argc, char* argv[]);
	//int exec_client_mode(client_arguments &args);
	std::string get_description(std::string key);
	std::string describe(std::string key);
	std::string describe(std::string key, std::string alias);
};