// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <Request.h>
#include <StreamResponse.h>

#include <boost/algorithm/string/trim.hpp>
#include <cstdint>
#include <cstdio>
#include <string>

// Conditional GET for the read-mostly JSON endpoints (facts, metrics).
//
// The validator is a hash of the body the controller is about to send, not of
// some upstream revision: a facts body carries the round's `collected` time
// and per-set errors next to the document, and a metrics body is a different
// shape with `?meta=1`, so only the bytes themselves say whether two answers
// are the same. Hashing the body costs one pass over a string that is about to
// be written to the socket anyway.
//
// FNV-1a rather than a cryptographic hash: an ETag only has to tell two
// representations of the same URL apart, OpenSSL is optional in this tree, and
// a stable function keeps the tag the same across restarts, so a client's
// cached copy survives an agent restart that changed nothing.
namespace web_etag {

inline std::string for_body(const std::string &body) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char c : body) {
    hash ^= c;
    hash *= 1099511628211ULL;
  }
  char buffer[19];
  std::snprintf(buffer, sizeof(buffer), "\"%016llx\"", static_cast<unsigned long long>(hash));
  return buffer;
}

// RFC 9110 13.1.2: If-None-Match is a comma separated list of entity tags, or
// `*`, compared weakly - a `W/` prefix on either side is ignored. Our tags are
// quoted hex, so a comma never occurs inside one and a plain split is exact.
inline bool matches(const std::string &if_none_match, const std::string &etag) {
  const std::string ours = etag.compare(0, 2, "W/") == 0 ? etag.substr(2) : etag;
  std::string::size_type start = 0;
  while (start <= if_none_match.size()) {
    std::string::size_type end = if_none_match.find(',', start);
    if (end == std::string::npos) end = if_none_match.size();
    std::string candidate = boost::algorithm::trim_copy(if_none_match.substr(start, end - start));
    if (candidate == "*") return true;
    if (candidate.compare(0, 2, "W/") == 0) candidate = candidate.substr(2);
    if (!candidate.empty() && candidate == ours) return true;
    start = end + 1;
  }
  return false;
}

// Send `body` as the answer to a GET, or a bodyless 304 when the client
// already holds it. Only for a successful answer: an error must never carry a
// validator, or a client could pin a 403 or 500 as "not modified".
//
// `private, no-cache` lets the browser keep the copy and revalidate it on
// every use (which is what makes the web UI's polling cheap) while telling any
// shared cache in between not to store an authenticated inventory at all.
inline void send(const Mongoose::Request &request, Mongoose::StreamResponse &response, const std::string &body) {
  const std::string etag = for_body(body);
  response.setHeader("ETag", etag);
  response.setHeader("Cache-Control", "private, no-cache");
  // The server defaults a 200 to JSON but anything above 299 to text/plain,
  // and a cache may merge a 304's headers into the copy it holds. Say what the
  // body is here, so the 304 does not relabel a cached JSON answer.
  if (!response.hasHeader("Content-Type")) response.setHeader("Content-Type", "application/json");
  if (matches(request.readHeader("If-None-Match"), etag)) {
    response.setCode(304, "Not Modified");
    return;
  }
  response.append(body);
}

}  // namespace web_etag
