// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// What `nscp icinga-agent import` reads out of an Icinga 2 agent's own
// configuration, so NSClient++ can take its place as the same endpoint:
// the agent's name from constants.conf and its parent from zones.conf.
//
// Not an Icinga DSL parser: it reads the two files the node wizard writes,
// and anything it cannot make out is left for the command line to say.

#include <stdexcept>
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

// The import cannot go ahead as asked; the message names the option that
// settles it.
struct import_error : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// What the command line said; empty where it said nothing.
struct import_options {
  std::string endpoint_name;
  std::string parent_endpoint;
  std::string parent_host;
  std::string parent_port;
};

// Who the agent is and who it reports to.
struct import_plan {
  std::string endpoint_name;
  std::string parent_endpoint;
  std::string parent_host;
  std::string parent_port;  // empty: leave the setting at its default, 5665
};

// This agent's name: the option, else NodeName in constants.conf, else the
// name of the only key among the certificates (`key_names`, without ".key").
// Throws import_error when none of them says.
std::string resolve_endpoint_name(const import_options &options, const std::string &constants_conf, const std::vector<std::string> &key_names);

// The parent to connect to. Without --parent-endpoint it is the one other
// Endpoint in zones.conf; with it, that endpoint's host and port are still
// taken from zones.conf when it is there. Options win over the file. Throws
// import_error when there is no parent, more than one to choose from, or no
// address to connect to (the parent connected to the Icinga agent).
import_plan plan_import(const import_options &options, const std::string &endpoint_name, const std::string &zones_conf);

}  // namespace icinga_agent
