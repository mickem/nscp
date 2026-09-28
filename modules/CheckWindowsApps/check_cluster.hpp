// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <nscapi/protobuf/command.hpp>

#include "cluster_source.hpp"

namespace check_cluster {
// The same parser, filter and renderer are used with native and test sources.
void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, object_kind kind,
                const source &fetch);
}  // namespace check_cluster
