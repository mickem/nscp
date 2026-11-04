// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines_protobuf.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4018)
#pragma warning(disable : 4100)
#pragma warning(disable : 4913)
#pragma warning(disable : 4512)
#pragma warning(disable : 4244)
#pragma warning(disable : 4127)
#pragma warning(disable : 4251)
#pragma warning(disable : 4275)
#pragma warning(disable : 4996)
#include <protobuf/failures.pb.h>
#pragma warning(pop)
#else
#include <protobuf/failures.pb.h>
#endif

#include <string>

// The failed-check log (PB::Failures): the last N non-OK results the core
// dispatched. Header-only and protobuf-only, because both sides need it: the
// core builds the request/response pair in NSAPIFailuresQuery, and a consumer
// - the REST controller, the `nscp test` console, a module - parses what it
// gets back. The store itself is service/failed_check_store.hpp.
namespace nscapi {
namespace failures {

// A GET or CLEAR request, serialised, ready for NSAPIFailuresQuery. `since` is
// the last index the caller has seen (0 for everything); `limit` caps the
// count (0 for no cap).
inline std::string make_request(const PB::Failures::FailuresRequestMessage::Request::Command command, const unsigned long long since = 0,
                                const unsigned int limit = 0) {
  PB::Failures::FailuresRequestMessage message;
  PB::Failures::FailuresRequestMessage::Request *payload = message.add_payload();
  payload->set_command(command);
  if (since > 0) payload->set_since(since);
  if (limit > 0) payload->set_limit(limit);
  return message.SerializeAsString();
}

// The Nagios status word for a result code, the way every other listing in
// NSClient++ spells it.
inline const char *status_name(const PB::Common::ResultCode code) {
  switch (code) {
    case PB::Common::ResultCode::OK:
      return "OK";
    case PB::Common::ResultCode::WARNING:
      return "WARNING";
    case PB::Common::ResultCode::CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

}  // namespace failures
}  // namespace nscapi
