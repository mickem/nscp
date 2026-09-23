// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "system_facts.hpp"

// Windows.h must precede intrin.h; the capital W keeps clang-format's
// case-sensitive include sort from breaking that order.
#include <Windows.h>
#include <intrin.h>

#include <cstring>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <string>
#include <win/sysinfo/win_sysinfo.hpp>

#include "check_hostname.hpp"

namespace system_facts {
namespace {

// Read one REG_SZ under HKLM, returning an empty string when the key, the
// value or the type is not what we expect. The 64-bit view is forced so a
// 32-bit agent on a 64-bit host reads the same SMBIOS strings as a 64-bit one
// (the same reason detect_sql_server_tag passes KEY_WOW64_64KEY).
std::string read_hklm_string(const wchar_t *path, const wchar_t *value) {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) return "";
  wchar_t buffer[512] = {0};
  DWORD size = sizeof(buffer) - sizeof(wchar_t);
  DWORD type = 0;
  const LONG status = RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS || type != REG_SZ) return "";
  // RegQueryValueExW does not promise a terminator; the buffer is zeroed and
  // one wchar_t shorter than its real size above, so the string always ends.
  std::string result = utf8::cvt<std::string>(std::wstring(buffer));
  // SMBIOS fields are padded and the placeholders OEMs leave behind are worse
  // than nothing ("To Be Filled By O.E.M.", "System Product Name").
  const std::string::size_type end = result.find_last_not_of(" \t");
  result = end == std::string::npos ? "" : result.substr(0, end + 1);
  if (result == "To Be Filled By O.E.M." || result == "System manufacturer" || result == "System Product Name" || result == "Default string" ||
      result == "Not Applicable" || result == "None") {
    return "";
  }
  return result;
}

// The architecture the *machine* is, not the one this process was built for:
// GetNativeSystemInfo sees through WOW64, so a 32-bit agent on a 64-bit host
// still publishes x86_64.
std::string native_arch() {
  SYSTEM_INFO info;
  ZeroMemory(&info, sizeof(info));
  GetNativeSystemInfo(&info);
  switch (info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      return host_facts::normalize_arch("AMD64");
    case PROCESSOR_ARCHITECTURE_ARM64:
      return host_facts::normalize_arch("ARM64");
    case PROCESSOR_ARCHITECTURE_ARM:
      return host_facts::normalize_arch("ARM");
    case PROCESSOR_ARCHITECTURE_INTEL:
      return host_facts::normalize_arch("x86");
    case PROCESSOR_ARCHITECTURE_IA64:
      return host_facts::normalize_arch("IA64");
    default:
      return "";
  }
}

// Ask the CPU whether it is running under a hypervisor, and which one.
// Leaf 1 ECX bit 31 is the "hypervisor present" bit - it is architecturally
// reserved on real hardware, so every hypervisor that wants to be found sets
// it - and leaf 0x40000000 then returns a 12-byte vendor id in EBX:ECX:EDX.
//
// Returns the raw id, or an empty string when no hypervisor is announcing
// itself. Distinguishing "no hypervisor" from "a hypervisor we cannot name"
// is the caller's job: only the first may conclude bare metal.
std::string cpuid_hypervisor_id(bool &hypervisor_present) {
  hypervisor_present = false;
#if defined(_M_IX86) || defined(_M_X64)
  int regs[4] = {0, 0, 0, 0};
  __cpuid(regs, 1);
  if ((static_cast<unsigned int>(regs[2]) & (1u << 31)) == 0) return "";
  hypervisor_present = true;

  __cpuid(regs, 0x40000000);
  // EAX holds the highest leaf the hypervisor supports; a value below
  // 0x40000000 means it answered the leaf without implementing it and the
  // vendor id registers hold garbage.
  if (static_cast<unsigned int>(regs[0]) < 0x40000000u) return "";
  char id[13] = {0};
  std::memcpy(id + 0, &regs[1], 4);  // EBX
  std::memcpy(id + 4, &regs[2], 4);  // ECX
  std::memcpy(id + 8, &regs[3], 4);  // EDX
  return std::string(id, 12);
#else
  // ARM64 has no CPUID; the DMI strings are the only source there.
  return "";
#endif
}

}  // namespace

host_facts::facts gather() {
  host_facts::facts f;
  f.os_family = "windows";

  if (const OSVERSIONINFOEX *info = windows::system_info::get_versioninfo()) {
    f.os_version = str::xtos(info->dwMajorVersion) + "." + str::xtos(info->dwMinorVersion) + "." + str::xtos(info->dwBuildNumber);
  }
  f.os_name = windows::system_info::get_version_string();
  f.arch = native_arch();

  const long cores = windows::system_info::get_numberOfProcessorscores();
  if (cores > 0) f.cpu_cores = cores;

  const windows::system_info::memory_usage memory = windows::system_info::get_memory();
  f.memory_gb = host_facts::memory_gb_from_bytes(memory.physical.total);

  f.manufacturer = read_hklm_string(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemManufacturer");
  f.model = read_hklm_string(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName");

  bool hypervisor_present = false;
  const std::string id = cpuid_hypervisor_id(hypervisor_present);
  // SMBIOS first, CPUID second. The hypervisor bit is set on the machine
  // *hosting* a hypervisor too: turning on Hyper-V, WSL2, Windows Sandbox or
  // virtualization-based security moves Windows into the root partition,
  // where it reads its own bit and the "Microsoft Hv" vendor id. Trusting
  // CPUID first tags every developer laptop and every Hyper-V host as a VM,
  // which makes "is this bare metal" useless on Windows.
  f.virtualization = host_facts::virtualization_from_dmi(f.manufacturer, f.model);
  if (f.virtualization.empty() && host_facts::dmi_names_physical_hardware(f.manufacturer, f.model)) {
    // The firmware named an OEM and a product, and neither is a hypervisor's.
    // That outranks the bit.
    f.virtualization = "none";
  }
  if (f.virtualization.empty() && hypervisor_present) {
    // SMBIOS said nothing usable and a hypervisor is announcing itself: name
    // it from CPUID, or admit it is one we cannot name. "virtual" is what the
    // unix module concludes from the /proc/cpuinfo `hypervisor` flag, which
    // is this same bit.
    const std::string named = host_facts::virtualization_from_hypervisor_id(id);
    f.virtualization = named.empty() ? "virtual" : named;
  }
#if defined(_M_IX86) || defined(_M_X64)
  // No hypervisor announced itself and SMBIOS named nothing. Only say "none"
  // where the bit was genuinely readable and clear: on ARM64 there is no bit,
  // so an unrecognised VM stays unknown rather than being called bare metal.
  if (f.virtualization.empty()) f.virtualization = "none";
#endif

  // GetComputerNameEx only; no resolver, no WMI (see check_hostname.hpp).
  const hostname_check::host_identity identity = hostname_check::gather_identity();
  f.domain = identity.domain;

  return f;
}

}  // namespace system_facts
