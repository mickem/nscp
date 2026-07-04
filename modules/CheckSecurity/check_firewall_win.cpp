/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

// Windows firewall profile state via the INetFwPolicy2 COM interface (the same
// data `Get-NetFirewallProfile` exposes), no WMI dependency.

#include "check_firewall.hpp"

#include <Windows.h>
#include <netfw.h>

namespace firewall_source {

namespace {

firewall_filter::filter_obj_ptr make_profile(INetFwPolicy2 *policy, NET_FW_PROFILE_TYPE2 type, const std::string &name) {
  auto obj = std::make_shared<firewall_filter::filter_obj>();
  obj->profile = name;

  VARIANT_BOOL enabled = VARIANT_FALSE;
  if (SUCCEEDED(policy->get_FirewallEnabled(type, &enabled))) obj->enabled = (enabled != VARIANT_FALSE) ? 1 : 0;

  NET_FW_ACTION action = NET_FW_ACTION_MAX;
  if (SUCCEEDED(policy->get_DefaultInboundAction(type, &action))) obj->inbound = (action == NET_FW_ACTION_ALLOW) ? "allow" : "block";
  if (SUCCEEDED(policy->get_DefaultOutboundAction(type, &action))) obj->outbound = (action == NET_FW_ACTION_ALLOW) ? "allow" : "block";
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

  out.push_back(make_profile(policy, NET_FW_PROFILE2_DOMAIN, "Domain"));
  out.push_back(make_profile(policy, NET_FW_PROFILE2_PRIVATE, "Private"));
  out.push_back(make_profile(policy, NET_FW_PROFILE2_PUBLIC, "Public"));

  policy->Release();
  if (com_inited) CoUninitialize();
}

}  // namespace firewall_source
