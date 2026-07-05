// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/program_options.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace compat {
void log_args(const PB::Commands::QueryRequestMessage::Request &request);
void addAllNumeric(boost::program_options::options_description &desc, const std::string suffix = "");
void addOldNumeric(boost::program_options::options_description &desc);
void addShowAll(boost::program_options::options_description &desc);
void do_matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string key, std::string &target, const std::string var,
                          const std::string bound, const std::string op);
void matchFirstNumeric(const boost::program_options::variables_map &vm, const std::string upper, const std::string lower, std::string &warn, std::string &crit,
                       const std::string suffix = "");
void matchFirstOldNumeric(const boost::program_options::variables_map &vm, const std::string var, std::string &warn, std::string &crit);
void matchShowAll(const boost::program_options::variables_map &vm, PB::Commands::QueryRequestMessage::Request &request, std::string prefix = "${status}: ");
bool hasFirstNumeric(const boost::program_options::variables_map &vm, const std::string suffix);

inline void inline_addarg(PB::Commands::QueryRequestMessage::Request &request, const std::string &str) {
  if (!str.empty()) request.add_arguments(str);
}
inline void inline_addarg(PB::Commands::QueryRequestMessage::Request &request, const std::string &prefix, const std::string &str) {
  if (!str.empty()) request.add_arguments(prefix + str);
}
}  // namespace compat