// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The firewall rule set, enumerated through INetFwPolicy2::Rules. This is the
// same store `Get-NetFirewallRule` reads; no WMI dependency, and it sees rules
// from every profile in one pass.

#include <Windows.h>
#include <netfw.h>

#include <str/utf8.hpp>

#include "check_firewall_rules.hpp"

namespace firewall_rules_source {

namespace {

// Take ownership of a BSTR the API just handed us and return it as UTF-8.
std::string from_bstr(BSTR value) {
  if (value == nullptr) return {};
  const std::string result = utf8::cvt<std::string>(std::wstring(value, SysStringLen(value)));
  SysFreeString(value);
  return result;
}

// Most INetFwRule string properties are optional and come back as NULL; read
// them uniformly rather than repeating the null dance per property.
template <class Getter>
std::string read_string(INetFwRule *rule, Getter getter) {
  BSTR value = nullptr;
  if (FAILED((rule->*getter)(&value))) return {};
  return from_bstr(value);
}

firewall_rules_filter::filter_obj_ptr read_rule(INetFwRule *rule) {
  auto obj = std::make_shared<firewall_rules_filter::filter_obj>();

  obj->name = read_string(rule, &INetFwRule::get_Name);
  obj->description = read_string(rule, &INetFwRule::get_Description);
  obj->group = read_string(rule, &INetFwRule::get_Grouping);
  obj->local_ports = read_string(rule, &INetFwRule::get_LocalPorts);
  obj->remote_ports = read_string(rule, &INetFwRule::get_RemotePorts);
  obj->local_addresses = read_string(rule, &INetFwRule::get_LocalAddresses);
  obj->remote_addresses = read_string(rule, &INetFwRule::get_RemoteAddresses);
  obj->application = read_string(rule, &INetFwRule::get_ApplicationName);
  obj->service = read_string(rule, &INetFwRule::get_ServiceName);

  VARIANT_BOOL enabled = VARIANT_FALSE;
  if (SUCCEEDED(rule->get_Enabled(&enabled))) obj->enabled = (enabled != VARIANT_FALSE) ? 1 : 0;
  VARIANT_BOOL edge = VARIANT_FALSE;
  if (SUCCEEDED(rule->get_EdgeTraversal(&edge))) obj->edge_traversal = (edge != VARIANT_FALSE) ? 1 : 0;

  NET_FW_RULE_DIRECTION direction = NET_FW_RULE_DIR_IN;
  if (SUCCEEDED(rule->get_Direction(&direction))) obj->direction = (direction == NET_FW_RULE_DIR_IN) ? "in" : "out";

  NET_FW_ACTION action = NET_FW_ACTION_BLOCK;
  if (SUCCEEDED(rule->get_Action(&action))) obj->action = (action == NET_FW_ACTION_ALLOW) ? "allow" : "block";

  LONG protocol = firewall_rules_filter::protocol_any;
  if (SUCCEEDED(rule->get_Protocol(&protocol))) obj->protocol = protocol;

  long profiles = 0;
  if (SUCCEEDED(rule->get_Profiles(&profiles))) obj->profile_mask = profiles;

  return obj;
}

}  // namespace

void gather(std::vector<firewall_rules_filter::filter_obj_ptr> &out, std::string &error) {
  const bool com_inited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  INetFwPolicy2 *policy = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), reinterpret_cast<void **>(&policy));
  if (FAILED(hr) || policy == nullptr) {
    error = "Failed to access the Windows firewall policy (INetFwPolicy2)";
    if (com_inited) CoUninitialize();
    return;
  }

  INetFwRules *rules = nullptr;
  if (FAILED(policy->get_Rules(&rules)) || rules == nullptr) {
    error = "Failed to read the Windows firewall rule set (INetFwPolicy2::Rules)";
    policy->Release();
    if (com_inited) CoUninitialize();
    return;
  }

  IUnknown *unknown = nullptr;
  IEnumVARIANT *enumerator = nullptr;
  if (SUCCEEDED(rules->get__NewEnum(&unknown)) && unknown != nullptr) {
    unknown->QueryInterface(__uuidof(IEnumVARIANT), reinterpret_cast<void **>(&enumerator));
    unknown->Release();
  }
  if (enumerator == nullptr) {
    error = "Failed to enumerate the Windows firewall rules";
    rules->Release();
    policy->Release();
    if (com_inited) CoUninitialize();
    return;
  }

  for (;;) {
    VARIANT item;
    VariantInit(&item);
    ULONG fetched = 0;
    if (enumerator->Next(1, &item, &fetched) != S_OK || fetched == 0) {
      VariantClear(&item);
      break;
    }
    INetFwRule *rule = nullptr;
    if (item.vt == VT_DISPATCH && item.pdispVal != nullptr) {
      item.pdispVal->QueryInterface(__uuidof(INetFwRule), reinterpret_cast<void **>(&rule));
    }
    if (rule != nullptr) {
      out.push_back(read_rule(rule));
      rule->Release();
    }
    VariantClear(&item);
  }

  enumerator->Release();
  rules->Release();
  policy->Release();
  if (com_inited) CoUninitialize();
}

}  // namespace firewall_rules_source
