// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/optional.hpp>
#include <string>

void save_credential(const std::string &alias, const std::string &password);
boost::optional<std::string> read_credential(const std::string &alias);