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
#include <protobuf/diagnostics.pb.h>
#pragma warning(pop)
#else
#include <protobuf/diagnostics.pb.h>
#endif

#include <string>

// Diagnostics (PB::Diagnostics): point-in-time tables modules keep up to date
// and the core collects on an interval. Header-only and protobuf-only, like
// failures.hpp: the core answers NSAPIDiagnosticsQuery with these messages and
// a consumer - the REST controller, the `nscp test` console, a module - parses
// them. Producers build sections with nscapi/nscapi_diagnostics_helper.hpp.
namespace nscapi {
namespace diagnostics {

// A request for the latest snapshot, serialised, ready for
// NSAPIDiagnosticsQuery. `section` asks for one section only (empty for all).
inline std::string make_request(const std::string &section = "") {
  PB::Diagnostics::DiagnosticsRequestMessage message;
  PB::Diagnostics::DiagnosticsRequestMessage::Request *payload = message.add_payload();
  if (!section.empty()) payload->set_section(section);
  return message.SerializeAsString();
}

// The lower-case word for a column type, the way REST and the fleet report
// spell it.
inline const char *column_type_name(const PB::Diagnostics::Column::Type type) {
  switch (type) {
    case PB::Diagnostics::Column::INT:
      return "int";
    case PB::Diagnostics::Column::FLOAT:
      return "float";
    case PB::Diagnostics::Column::TIMESTAMP:
      return "timestamp";
    case PB::Diagnostics::Column::BOOL:
      return "bool";
    default:
      return "string";
  }
}

}  // namespace diagnostics
}  // namespace nscapi
