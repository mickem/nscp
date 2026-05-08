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
#include <string>
#include <utility>
#include <vector>

namespace process {
class process_exception final : public std::exception {
  std::string error;

 public:
  //////////////////////////////////////////////////////////////////////////
  /// Constructor takes an error message.
  /// @param error the error message
  ///
  /// @author mickem
  explicit process_exception(std::string error) : error(std::move(error)) {}
  ~process_exception() noexcept override = default;

  //////////////////////////////////////////////////////////////////////////
  /// Retrieve the error message from the exception.
  /// @return the error message
  ///
  /// @author mickem
  const char *what() const noexcept override { return error.c_str(); }
};

class exec_arguments {
 public:
  exec_arguments(std::string root_path_, std::string command_, const unsigned int timeout_, std::string encoding, std::string session, bool, const bool fork,
                 bool kill_tree)
      : root_path(std::move(root_path_)),
        command(std::move(command_)),
        timeout(timeout_),
        encoding(std::move(encoding)),
        session(std::move(session)),
        display(false),
        ignore_perf(false),
        fork(fork),
        kill_tree(kill_tree) {}

  std::string alias;
  std::string root_path;
  std::string command;
  // When non-empty, argv is the authoritative argument vector and the launcher
  // must use it directly (Unix: execvp, Windows: lpApplicationName + a
  // per-argument-quoted command line). The string `command` is then only kept
  // for logging. When argv is empty the launcher falls back to interpreting
  // `command` as a single string - that path goes through /bin/sh on Unix and
  // through Windows command-line parsing on Win32, both of which expose
  // attacker-controlled substitutions to shell-style metacharacter handling.
  // Callers that have any operator-controlled argument substitution MUST
  // populate argv.
  std::vector<std::string> argv;
  unsigned int timeout;
  std::string user;
  std::string domain;
  std::string password;
  std::string encoding;
  std::string session;
  bool display;
  bool ignore_perf;
  bool fork;
  bool kill_tree;
};
void kill_all();
int execute_process(const exec_arguments &args, std::string &output);
}  // namespace process