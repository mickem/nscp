// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The Icinga 2 cluster protocol, the part of it an agent needs. Nothing in
// these files touches a socket or the core, so each piece unit tests on its
// own:
//
//   icinga_agent_netstring  framing (`<length>:<json>,`)
//   icinga_agent_messages   the messages an agent sends
//   icinga_agent_json       reading Icinga's loosely typed JSON
//   icinga_agent_macros     `$macro$` expansion
//   icinga_agent_arguments  a CheckCommand's arguments -> query arguments
//   icinga_agent_execute    event::ExecuteCommand -> the query to run
//   icinga_agent_fingerprint the `parent fingerprint` setting

#include "icinga_agent_arguments.hpp"
#include "icinga_agent_execute.hpp"
#include "icinga_agent_fingerprint.hpp"
#include "icinga_agent_json.hpp"
#include "icinga_agent_macros.hpp"
#include "icinga_agent_messages.hpp"
#include "icinga_agent_netstring.hpp"
