// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows firewall profile state: the local store via the INetFwPolicy2 COM
// interface, overlaid with any Group Policy resultant values so the reported
// state is the effective one (what `Get-NetFirewallProfile -PolicyStore
// ActiveStore` shows). No WMI dependency.

#include "check_firewall.hpp"

#include <Windows.h>
#include <netfw.h>

namespace firewall_source {

namespace {

boost::optional<bool> read_policy_dword(HKEY key, const char *name) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  if (RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size) != ERROR_SUCCESS || type != REG_DWORD) return boost::none;
  return value != 0;
}

// Read the GP resultant values the policy engine writes for one profile.
// A missing key or value means "not configured" in GP.
policy_override read_policy_override(const std::string &profile_name) {
  policy_override gp;
  const std::string path = "SOFTWARE\\Policies\\Microsoft\\WindowsFirewall\\" + profile_name + "Profile";
  HKEY key = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return gp;
  gp.enabled = read_policy_dword(key, "EnableFirewall");
  gp.inbound_block = read_policy_dword(key, "DefaultInboundAction");
  gp.outbound_block = read_policy_dword(key, "DefaultOutboundAction");
  RegCloseKey(key);
  return gp;
}

firewall_filter::filter_obj_ptr make_profile(INetFwPolicy2 *policy, NET_FW_PROFILE_TYPE2 type, const std::string &name, long active_types) {
  auto obj = std::make_shared<firewall_filter::filter_obj>();
  obj->profile = name;
  obj->active = (active_types & type) != 0 ? 1 : 0;

  VARIANT_BOOL enabled = VARIANT_FALSE;
  if (SUCCEEDED(policy->get_FirewallEnabled(type, &enabled))) obj->enabled = (enabled != VARIANT_FALSE) ? 1 : 0;

  NET_FW_ACTION action = NET_FW_ACTION_MAX;
  if (SUCCEEDED(policy->get_DefaultInboundAction(type, &action))) obj->inbound = (action == NET_FW_ACTION_ALLOW) ? "allow" : "block";
  if (SUCCEEDED(policy->get_DefaultOutboundAction(type, &action))) obj->outbound = (action == NET_FW_ACTION_ALLOW) ? "allow" : "block";

  apply_policy_override(*obj, read_policy_override(name));
  return obj;
}

}  // namespace

void gather(std::vector<firewall_filter::filter_obj_ptr> &out, std::string &error) {
  const bool com_inited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  INetFwPolicy2 *policy = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), reinterpret_cast<void **>(&policy));
  if (FAILED(hr) || policy == nullptr) {
    error = "Failed to access the Windows firewall policy (INetFwPolicy2)";
    if (com_inited) CoUninitialize();
    return;
  }

  // Which profile(s) NLA currently applies (bitmask; with no connected
  // network Windows reports Public).
  long active_types = 0;
  if (FAILED(policy->get_CurrentProfileTypes(&active_types))) active_types = 0;

  out.push_back(make_profile(policy, NET_FW_PROFILE2_DOMAIN, "Domain", active_types));
  out.push_back(make_profile(policy, NET_FW_PROFILE2_PRIVATE, "Private", active_types));
  out.push_back(make_profile(policy, NET_FW_PROFILE2_PUBLIC, "Public", active_types));

  policy->Release();
  if (com_inited) CoUninitialize();
}

}  // namespace firewall_source
