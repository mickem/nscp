// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows implementations of the posture checks:
//   check_nla            -> COM INetworkListManager
//   check_antivirus      -> WMI root\SecurityCenter2 AntiVirusProduct
//   check_bitlocker      -> WMI root\CIMV2\Security\MicrosoftVolumeEncryption
//   check_secureboot     -> registry SecureBoot\State
//   check_defender       -> WMI root\Microsoft\Windows\Defender MSFT_MpComputerStatus
//   check_local_accounts -> WMI Win32_UserAccount (LocalAccount=TRUE)
//   check_group_members  -> NetLocalGroupGetMembers

#include <Windows.h>
#include <lm.h>
#include <netlistmgr.h>
#include <sddl.h>

#include <str/utf8.hpp>
#include <win/registry.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_defender.hpp"
#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
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

namespace defender_source {

namespace {
// Read helpers that tolerate a missing/unreadable property so one absent field
// (older Defender builds vary) does not blank the whole status.
long long read_bool(const wmi_impl::row &r, const char *name) {
  try {
    return r.get_int(name) != 0 ? 1 : 0;  // WMI VT_BOOL comes back as -1/0 via get_int
  } catch (...) {
    return 0;
  }
}
// Ages are uint days; a "never scanned" value is the uint sentinel (~4.29e9),
// which we normalise to -1 ("unknown/never") so thresholds do not falsely trip.
long long read_age(const wmi_impl::row &r, const char *name) {
  try {
    const long long v = r.get_int(name);
    return (v < 0 || v > 1000000) ? -1 : v;
  } catch (...) {
    return -1;
  }
}
std::string read_str(const wmi_impl::row &r, const char *name) {
  try {
    return r.get_string(name);
  } catch (...) {
    return {};
  }
}
}  // namespace

void gather(std::vector<defender_filter::filter_obj_ptr> &out, std::string & /*error*/) {
  try {
    // SELECT * and read defensively: property availability varies by Defender
    // version, and a named-column SELECT would fail the whole query if one is absent.
    wmi_impl::query wmi_q("select * from MSFT_MpComputerStatus", "root\\Microsoft\\Windows\\Defender", "", "");
    wmi_impl::row_enumerator rows = wmi_q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      auto o = std::make_shared<defender_filter::filter_obj>();
      o->enabled = (read_bool(r, "AntivirusEnabled") || read_bool(r, "AMServiceEnabled")) ? 1 : 0;
      o->realtime_enabled = read_bool(r, "RealTimeProtectionEnabled");
      o->tamper_protection = read_bool(r, "IsTamperProtected");
      o->signature_age = read_age(r, "AntivirusSignatureAge");
      o->quick_scan_age = read_age(r, "QuickScanAge");
      o->full_scan_age = read_age(r, "FullScanAge");
      o->engine_version = read_str(r, "AMEngineVersion");
      o->signature_version = read_str(r, "AntivirusSignatureVersion");
      o->product_version = read_str(r, "AMProductVersion");
      out.push_back(o);
    }
  } catch (const wmi_impl::wmi_exception &) {
    // Defender namespace/class unavailable (not installed, or a third-party AV
    // owns protection): report no rows so the check reports UNKNOWN via
    // empty-state rather than failing.
  }
}

}  // namespace defender_source

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

namespace local_accounts_source {

namespace {
// WMI VT_BOOL comes back as -1/0 through get_int; normalise to 1/0. Tolerates a
// missing/unreadable property by returning `def`.
long long read_bool(const wmi_impl::row &r, const char *name, long long def) {
  try {
    return r.get_int(name) != 0 ? 1 : 0;
  } catch (...) {
    return def;
  }
}
}  // namespace

void gather(std::vector<local_accounts_filter::filter_obj_ptr> &out, std::string &error) {
  try {
    wmi_impl::query wmi_q(
        "select Name, SID, Disabled, Lockout, PasswordExpires, PasswordRequired from Win32_UserAccount where LocalAccount=TRUE", "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = wmi_q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      auto o = std::make_shared<local_accounts_filter::filter_obj>();
      o->name = r.get_string("Name");
      o->sid = r.get_string("SID");
      o->disabled = read_bool(r, "Disabled", 0);
      o->locked = read_bool(r, "Lockout", 0);
      o->password_expires = read_bool(r, "PasswordExpires", 0);
      // Default to "required" when unknown, so an unreadable field never fakes a
      // passwordless-account finding.
      o->password_required = read_bool(r, "PasswordRequired", 1);
      o->rid = local_accounts_filter::rid_from_sid(o->sid);
      out.push_back(o);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    error = std::string("Failed to query local user accounts: ") + e.what();
  }
}

}  // namespace local_accounts_source

namespace group_members_source {

namespace {
std::string sid_usage_to_string(SID_NAME_USE u) {
  switch (u) {
    case SidTypeUser:
      return "user";
    case SidTypeGroup:
      return "group";
    case SidTypeDomain:
      return "domain";
    case SidTypeAlias:
      return "alias";
    case SidTypeWellKnownGroup:
      return "wellknown";
    case SidTypeDeletedAccount:
      return "deleted";
    case SidTypeComputer:
      return "computer";
    default:
      return "unknown";
  }
}
}  // namespace

void gather(const std::string &group, std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error) {
  const std::wstring wgroup = utf8::cvt<std::wstring>(group);
  LOCALGROUP_MEMBERS_INFO_2 *buf = nullptr;
  DWORD entries_read = 0, total_entries = 0;
  DWORD_PTR resume = 0;
  const NET_API_STATUS status =
      NetLocalGroupGetMembers(nullptr, wgroup.c_str(), 2, reinterpret_cast<LPBYTE *>(&buf), MAX_PREFERRED_LENGTH, &entries_read, &total_entries, &resume);
  if (status == NERR_GroupNotFound) {
    error = "Local group not found: " + group;
    if (buf) NetApiBufferFree(buf);
    return;
  }
  if (status != NERR_Success && status != ERROR_MORE_DATA) {
    error = "Failed to read members of local group '" + group + "' (status " + std::to_string(status) + ")";
    if (buf) NetApiBufferFree(buf);
    return;
  }
  for (DWORD i = 0; i < entries_read; ++i) {
    auto o = std::make_shared<group_members_filter::filter_obj>();
    o->group = group;
    o->member = buf[i].lgrmi2_domainandname ? utf8::cvt<std::string>(buf[i].lgrmi2_domainandname) : std::string();
    const std::size_t slash = o->member.find('\\');
    if (slash != std::string::npos) {
      o->domain = o->member.substr(0, slash);
      o->name = o->member.substr(slash + 1);
    } else {
      o->name = o->member;
    }
    o->type = sid_usage_to_string(buf[i].lgrmi2_sidusage);
    LPWSTR sid_str = nullptr;
    if (buf[i].lgrmi2_sid && ConvertSidToStringSidW(buf[i].lgrmi2_sid, &sid_str) && sid_str) {
      o->sid = utf8::cvt<std::string>(sid_str);
      LocalFree(sid_str);
    }
    out.push_back(o);
  }
  if (buf) NetApiBufferFree(buf);
}

}  // namespace group_members_source
