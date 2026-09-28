// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <str/utf8.hpp>
#include <win/eventlog/modern_eventlog.hpp>

#include "check_nps_internal.hpp"

namespace check_nps {
// These positions match the value paths passed to EvtCreateRenderContext.
enum event_field { provider_field, id_field, client_field, address_field, network_policy_field, proxy_policy_field, reason_field, event_field_count };

inline std::string event_string(const eventlog::api::EVT_VARIANT &value) {
  switch (value.Type) {
    case eventlog::api::EvtVarTypeNull:
      return "";
    case eventlog::api::EvtVarTypeString:
      if (value.StringVal) return utf8::cvt<std::string>(value.StringVal);
      break;
  }
  throw std::runtime_error("Unexpected NPS event field type");
}

inline event parse_event_values(const eventlog::api::EVT_VARIANT *values, DWORD count) {
  if (count != event_field_count) throw std::runtime_error("Incomplete NPS event values");
  // System/EventID is UInt16. Some event sources expose integer values as UInt32.
  const auto &id = values[id_field];
  const DWORD event_id = id.Type == eventlog::api::EvtVarTypeUInt16 ? id.UInt16Val
                        : id.Type == eventlog::api::EvtVarTypeUInt32 ? id.UInt32Val
                                                                   : 0;
  if (event_id < 6272 || event_id > 6275) throw std::runtime_error("Unexpected NPS audit event ID");
  const auto &reason = values[reason_field];
  const std::string reason_text = reason.Type == eventlog::api::EvtVarTypeUInt32 ? std::to_string(reason.UInt32Val) : event_string(reason);
  return make_event(event_string(values[provider_field]), static_cast<int>(event_id), event_string(values[client_field]),
                    event_string(values[address_field]), event_string(values[network_policy_field]), event_string(values[proxy_policy_field]), reason_text);
}
}  // namespace check_nps
