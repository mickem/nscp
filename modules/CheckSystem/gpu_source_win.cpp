// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows data sources of check_gpu: the "GPU ..." performance counter
// sets through PDH, and the adapters' names and memory sizes through DXGI.

#include <boost/algorithm/string/trim.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <str/utf8.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_object_gather.hpp>
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

// Names and capacities of the adapters. Best effort: the counters are the
// check's data and this only decorates them, so every failure yields the
// adapters found so far and the rest are reported under their luid.
//
// dxgi.dll is loaded on demand rather than linked: the module also has to
// load on systems that predate DXGI 1.1, where a static import would keep
// every CheckSystem check from loading for the sake of this one.
std::vector<adapter_info> enumerate_adapters() {
  std::vector<adapter_info> out;
  const library_handle dxgi(LoadLibraryW(L"dxgi.dll"));
  if (dxgi.handle == nullptr) return out;
  typedef HRESULT(WINAPI * create_factory_fn)(REFIID, void **);
  const create_factory_fn create_factory = reinterpret_cast<create_factory_fn>(GetProcAddress(dxgi.handle, "CreateDXGIFactory1"));
  if (create_factory == nullptr) return out;

  // Declared after `dxgi` on purpose: locals are destroyed in reverse, and
  // the interfaces have to be released while the library is still loaded.
  com_release<IDXGIFactory1> factory;
  if (FAILED(create_factory(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory.ptr))) || factory.ptr == nullptr) return out;

  for (UINT index = 0;; ++index) {
    com_release<IDXGIAdapter1> adapter;
    // DXGI_ERROR_NOT_FOUND past the last adapter; anything else ends the walk too.
    if (FAILED(factory.ptr->EnumAdapters1(index, &adapter.ptr)) || adapter.ptr == nullptr) break;
    DXGI_ADAPTER_DESC1 desc;
    if (FAILED(adapter.ptr->GetDesc1(&desc))) continue;
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
    info.dedicated_total = static_cast<long long>(desc.DedicatedVideoMemory);
    info.shared_total = static_cast<long long>(desc.SharedSystemMemory);
    out.push_back(info);
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

  check_gpu_from(request, response, build_adapters(snapshot, enumerate_adapters()));
}

}  // namespace gpu_check
