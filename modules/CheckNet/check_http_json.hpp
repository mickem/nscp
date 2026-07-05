/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * JSON response-body value extraction for check_http, kept in its own
 * translation unit so it can be unit-tested without the HTTP/TLS client stack.
 */

#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace check_net {
namespace check_http_json {

// Extracted values keyed by alias. Numeric and boolean JSON values land in
// `numbers` (and a string form in `strings`); string values land only in
// `strings`; arrays/objects land in `strings` as their serialized JSON.
struct extraction {
  std::map<std::string, double> numbers;
  std::map<std::string, std::string> strings;
};

// Parse `body` as JSON and, for each (alias, path), extract the value at that
// dotted path (numeric segments index into arrays; single-quote a segment that
// contains a dot, e.g. a.'b.c'.0). Returns false if `body` is not valid JSON;
// missing paths are simply left absent.
bool extract(const std::string &body, const std::vector<std::pair<std::string, std::string>> &alias_paths, extraction &out);

// Split a dotted JSON path into segments, honouring single-quoted segments.
// Exposed for unit testing.
std::vector<std::string> split_path(const std::string &path);

}  // namespace check_http_json
}  // namespace check_net
