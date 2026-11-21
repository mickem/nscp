// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// What `nscp icinga-agent import` reads out of an Icinga 2 agent's own
// configuration, so NSClient++ can take its place as the same endpoint:
// the agent's name from constants.conf and its parent from zones.conf.
//
// Not an Icinga DSL parser: it reads the two files the node wizard writes,
// and anything it cannot make out is left for the command line to say.

#include <string>
#include <vector>

namespace icinga_agent {

struct icinga_endpoint {
  std::string name;           // empty when the object is named by a constant
  bool is_node_name = false;  // `object Endpoint NodeName`: the agent itself
  std::string host;           // empty: the agent does not connect to it
  std::string port;           // empty: Icinga's default, 5665
};

// The text without `//`, `#` and `/* */` comments. Quoted strings are kept
// as they are, comment characters included.
std::string strip_icinga_comments(const std::string &text);

// `const NodeName = "..."` in constants.conf; empty when it is not set (the
// node wizard leaves it commented out when the name is the host's FQDN).
std::string parse_node_name(const std::string &constants_conf);

// Every `object Endpoint` in zones.conf, in the order written.
std::vector<icinga_endpoint> parse_endpoints(const std::string &zones_conf);

// The endpoints that are not the agent itself: its parents.
std::vector<icinga_endpoint> parent_endpoints(const std::vector<icinga_endpoint> &endpoints, const std::string &node_name);

}  // namespace icinga_agent
