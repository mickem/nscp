// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Aggregator for the request and response types so existing
// `#include <net/http/http_packet.hpp>` users keep working. New code should
// include the specific type it needs directly.
#include <net/http/http_request.hpp>
#include <net/http/http_response.hpp>
