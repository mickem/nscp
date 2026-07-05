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

// Windows implementations of the posture checks:
//   check_nla        -> COM INetworkListManager
//   check_antivirus  -> WMI root\SecurityCenter2 AntiVirusProduct
//   check_bitlocker  -> WMI root\CIMV2\Security\MicrosoftVolumeEncryption
//   check_secureboot -> registry SecureBoot\State

#include <Windows.h>
#include <netlistmgr.h>

#include <str/utf8.hpp>
#include <win/registry.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_nla.hpp"
#include "check_secureboot.hpp"

namespace nla_source {

std::string category_name(NLM_NETWORK_CATEGORY c) {
  switch (c) {
    case NLM_NETWORK_CATEGORY_PUBLIC:
      return "public";
    case NLM_NETWORK_CATEGORY_PRIVATE:
      return "private";
    case NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED:
      return "domain";
    default:
      return "unknown";
  }
}

void gather(std::vector<nla_filter::filter_obj_ptr> &out, std::string &error) {
  const bool com_inited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

  INetworkListManager *mgr = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_INetworkListManager, reinterpret_cast<void **>(&mgr));
  if (FAILED(hr) || mgr == nullptr) {
    error = "Failed to access the Network List Manager (INetworkListManager)";
    if (com_inited) CoUninitialize();
    return;
  }

  IEnumNetworks *nets = nullptr;
  if (SUCCEEDED(mgr->GetNetworks(NLM_ENUM_NETWORK_ALL, &nets)) && nets != nullptr) {
    for (;;) {
      INetwork *net = nullptr;
      ULONG fetched = 0;
      if (nets->Next(1, &net, &fetched) != S_OK || fetched == 0 || net == nullptr) break;

      auto o = std::make_shared<nla_filter::filter_obj>();
      BSTR name = nullptr;
      if (SUCCEEDED(net->GetName(&name)) && name != nullptr) {
        o->network = utf8::cvt<std::string>(std::wstring(name, SysStringLen(name)));
        SysFreeString(name);
      }
      NLM_NETWORK_CATEGORY cat = NLM_NETWORK_CATEGORY_PUBLIC;
      if (SUCCEEDED(net->GetCategory(&cat))) o->category = category_name(cat);
      VARIANT_BOOL connected = VARIANT_FALSE;
      if (SUCCEEDED(net->get_IsConnected(&connected))) o->connected = (connected != VARIANT_FALSE) ? 1 : 0;

      out.push_back(o);
      net->Release();
    }
    nets->Release();
  }

  mgr->Release();
  if (com_inited) CoUninitialize();
}

}  // namespace nla_source

namespace antivirus_source {

void gather(std::vector<antivirus_filter::filter_obj_ptr> &out, std::string &error) {
  try {
    wmi_impl::query wmi_q("select displayName, productState from AntiVirusProduct", "root\\SecurityCenter2", "", "");
    wmi_impl::row_enumerator rows = wmi_q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      auto o = std::make_shared<antivirus_filter::filter_obj>();
      o->name = r.get_string("displayName");
      const long long ps = r.get_int("productState");
      o->product_state = ps;
      // Well-known Security Center productState decode: the 0x1000 bit means
      // real-time protection is on, the 0x10 bit means the definitions are stale.
      o->enabled = (ps & 0x1000) != 0 ? 1 : 0;
      o->up_to_date = (ps & 0x10) == 0 ? 1 : 0;
      out.push_back(o);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    error = std::string("Failed to query Security Center: ") + e.what();
  }
}

}  // namespace antivirus_source

namespace bitlocker_source {

void gather(std::vector<bitlocker_filter::filter_obj_ptr> &out, std::string &error) {
  try {
    wmi_impl::query wmi_q("select DriveLetter, ProtectionStatus from Win32_EncryptableVolume", "root\\CIMV2\\Security\\MicrosoftVolumeEncryption", "", "");
    wmi_impl::row_enumerator rows = wmi_q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      auto o = std::make_shared<bitlocker_filter::filter_obj>();
      o->drive = r.get_string("DriveLetter");
      o->protection_status = r.get_int("ProtectionStatus");
      o->is_protected = (o->protection_status == 1) ? 1 : 0;
      out.push_back(o);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    error = std::string("Failed to query BitLocker volumes: ") + e.what();
  }
}

}  // namespace bitlocker_source

namespace secureboot_source {

void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string & /*error*/) {
  auto o = std::make_shared<secureboot_filter::filter_obj>();
  const win_registry::value_info vi =
      win_registry::read_value(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", "UEFISecureBootEnabled");
  if (vi.exists && vi.type == REG_DWORD) {
    o->supported = 1;
    o->enabled = (vi.int_value == 1) ? 1 : 0;
  } else {
    // No value: legacy BIOS or the platform does not expose it.
    o->supported = 0;
    o->enabled = 0;
  }
  out.push_back(o);
}

}  // namespace secureboot_source
