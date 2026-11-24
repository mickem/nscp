// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows data sources of check_gpu: the "GPU ..." performance counter
// sets through PDH, and the adapters' names and memory sizes through DXGI.

#include <boost/algorithm/string/trim.hpp>
#include <cstdio>
#include <nscapi/protobuf/functions_response.hpp>
#include <str/utf8.hpp>
#include <string>
#include <vector>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
#include <win/registry.hpp>
#include <win/windows.hpp>

#include <dxgi.h>

#include "check_gpu.hpp"

namespace gpu_check {

namespace {

// Releases a COM interface on scope exit.
template <class T>
struct com_release {
  T *ptr = nullptr;
  com_release() = default;
  com_release(const com_release &) = delete;
  com_release &operator=(const com_release &) = delete;
  ~com_release() {
    if (ptr != nullptr) ptr->Release();
  }
};

struct library_handle {
  HMODULE handle;
  explicit library_handle(HMODULE handle) : handle(handle) {}
  library_handle(const library_handle &) = delete;
  library_handle &operator=(const library_handle &) = delete;
  ~library_handle() {
    if (handle != nullptr) FreeLibrary(handle);
  }
};

// DXGI_ADAPTER_FLAG_SOFTWARE, spelled out: the SDK the legacy (XP) build uses
// predates the name.
const UINT adapter_flag_software = 2;
// DXGI_ERROR_NOT_FOUND, spelled out for the same reason: older SDKs define it
// in dxgitype.h, newer ones in winerror.h.
const HRESULT dxgi_error_not_found = static_cast<HRESULT>(0x887A0002L);

std::string hresult_text(const HRESULT hr) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
  return buf;
}

// The display adapter setup class; one driver key (0000, 0001, ...) per
// adapter driver installed.
const char display_class_key[] = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";

// The memory sizes the display adapter drivers record. Only read by a 32-bit
// build, where DXGI cannot express a size of 4 GB or more (memory_total()).
// Best effort: a key that cannot be read is skipped, and a missing figure
// leaves the DXGI figure (or unknown) in place.
std::vector<driver_key> read_driver_keys() {
  std::vector<driver_key> out;
  const std::string base = display_class_key;
  for (const win_registry::key_info &child : win_registry::enum_sub_keys(HKEY_LOCAL_MACHINE, base, "HKLM\\" + base, "HKLM", 1, 0)) {
    const std::string path = base + "\\" + child.name;
    const win_registry::value_info size = win_registry::read_value(HKEY_LOCAL_MACHINE, path, "HardwareInformation.qwMemorySize");
    if (!size.exists || size.type != REG_QWORD || size.int_value <= 0) continue;
    const win_registry::value_info id = win_registry::read_value(HKEY_LOCAL_MACHINE, path, "MatchingDeviceId");
    if (!id.exists || id.string_value.empty()) continue;
    driver_key key;
    key.matching_device_id = id.string_value;
    key.memory_size = static_cast<unsigned long long>(size.int_value);
    out.push_back(key);
  }
  return out;
}

// Names and capacities of the adapters, in DXGI enumeration order. Any
// failure is returned as the enumeration's error rather than as a shorter
// list: an adapter missing from it would be reported as "unknown" with no
// capacity, which nothing in the default filter or thresholds would notice.
//
// dxgi.dll is loaded on demand rather than linked: the module also has to
// load on systems that predate DXGI 1.1, where a static import would keep
// every CheckSystem check from loading for the sake of this one.
adapter_enumeration enumerate_adapters() {
  adapter_enumeration out;
  const library_handle dxgi(LoadLibraryW(L"dxgi.dll"));
  if (dxgi.handle == nullptr) {
    out.error = "dxgi.dll could not be loaded: error " + std::to_string(static_cast<unsigned long>(GetLastError()));
    return out;
  }
  typedef HRESULT(WINAPI * create_factory_fn)(REFIID, void **);
  const create_factory_fn create_factory = reinterpret_cast<create_factory_fn>(GetProcAddress(dxgi.handle, "CreateDXGIFactory1"));
  if (create_factory == nullptr) {
    out.error = "dxgi.dll does not export CreateDXGIFactory1";
    return out;
  }

  // Declared after `dxgi` on purpose: locals are destroyed in reverse, and
  // the interfaces have to be released while the library is still loaded.
  com_release<IDXGIFactory1> factory;
  const HRESULT created = create_factory(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory.ptr));
  if (FAILED(created) || factory.ptr == nullptr) {
    out.error = "CreateDXGIFactory1 failed: " + hresult_text(created);
    return out;
  }

  // SIZE_T is 32 bits wide in the x86 agent; only then are the driver keys
  // needed to tell a 4 GB+ card's size.
  const unsigned size_bits = static_cast<unsigned>(sizeof(SIZE_T) * 8);
  std::vector<driver_key> driver_keys;
  if (size_bits < 64) driver_keys = read_driver_keys();

  for (UINT index = 0;; ++index) {
    com_release<IDXGIAdapter1> adapter;
    const HRESULT found = factory.ptr->EnumAdapters1(index, &adapter.ptr);
    // DXGI_ERROR_NOT_FOUND past the last adapter.
    if (found == dxgi_error_not_found) break;
    if (FAILED(found) || adapter.ptr == nullptr) {
      out.error = "IDXGIFactory1::EnumAdapters1(" + std::to_string(index) + ") failed: " + hresult_text(found);
      out.adapters.clear();
      return out;
    }
    DXGI_ADAPTER_DESC1 desc;
    const HRESULT described = adapter.ptr->GetDesc1(&desc);
    if (FAILED(described)) {
      out.error = "IDXGIAdapter1::GetDesc1 failed for adapter " + std::to_string(index) + ": " + hresult_text(described);
      out.adapters.clear();
      return out;
    }
    adapter_info info;
    info.luid = format_luid(static_cast<unsigned long>(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart);
    // Description is a fixed 128 character array, not guaranteed to be terminated.
    std::wstring name;
    for (const WCHAR c : desc.Description) {
      if (c == L'\0') break;
      name += c;
    }
    info.name = utf8::cvt<std::string>(name);
    boost::algorithm::trim(info.name);
    info.software = (desc.Flags & adapter_flag_software) != 0;
    unsigned long long driver_size = 0;
    if (!info.software) {
      driver_size =
          driver_memory_size(driver_keys, static_cast<unsigned>(desc.VendorId), static_cast<unsigned>(desc.DeviceId), static_cast<unsigned>(desc.SubSysId));
    }
    info.dedicated_total = memory_total(static_cast<unsigned long long>(desc.DedicatedVideoMemory), size_bits, driver_size);
    // No driver figure for shared memory: it is a share of system RAM.
    info.shared_total = memory_total(static_cast<unsigned long long>(desc.SharedSystemMemory), size_bits);
    out.adapters.push_back(info);
  }
  return out;
}

std::string counters_error(const PDH::pdh_exception &e) {
  // PDH messages come from FormatMessage and end in \r\n; trim so the check
  // output stays a single line.
  std::string reason = e.reason();
  boost::algorithm::trim(reason);
  return "GPU performance counters are not available - they need Windows 10 1709 / Windows Server 2019 or later (" + reason + ")";
}

}  // namespace

void check_gpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  counter_snapshot snapshot;
  try {
    snapshot.adapter_memory = PDH::gather_object_instances("GPU Adapter Memory", {"Dedicated Usage", "Shared Usage", "Total Committed"}, false);
    // Utilization Percentage is a rate over the engines' running time: it
    // needs two samples a second apart, or every engine reads 0.
    snapshot.engine = PDH::gather_object_instances("GPU Engine", {"Utilization Percentage"}, true);
  } catch (const PDH::pdh_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, counters_error(e));
  }
  // These two only add detail (local_used, processes): a host that lacks
  // them still reports its adapters.
  try {
    snapshot.local_memory = PDH::gather_object_instances("GPU Local Adapter Memory", {"Local Usage"}, false);
  } catch (const PDH::pdh_exception &) {
  }
  try {
    snapshot.process_memory = PDH::gather_object_instances("GPU Process Memory", {"Dedicated Usage", "Shared Usage"}, false);
  } catch (const PDH::pdh_exception &) {
  }

  check_gpu_from(request, response, snapshot, enumerate_adapters());
}

}  // namespace gpu_check
