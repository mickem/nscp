// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#include "cluster_source_win.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <str/utf8.hpp>

namespace check_cluster {
namespace {
std::runtime_error failure(const std::string &operation, DWORD code) {
  wchar_t message[1024] = {};
  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, message, 1024, nullptr);
  std::string reason = utf8::cvt<std::string>(std::wstring(message));
  while (!reason.empty() && (reason.back() == '\r' || reason.back() == '\n')) reason.pop_back();
  return std::runtime_error(operation + " (Windows error " + std::to_string(code) + ")" + (reason.empty() ? "" : ": " + reason));
}

std::runtime_error object_failure(const char *operation, const std::wstring &name, DWORD code) {
  // Capture the error at the call site before UTF-16 conversion can call Win32.
  return failure(std::string(operation) + " '" + utf8::cvt<std::string>(name) + "'", code);
}

template <typename Handle, typename Close>
class scoped_handle {
 public:
  scoped_handle(Handle handle, Close close) : handle_(handle), close_(close) {}
  ~scoped_handle() {
    if (handle_) close_(handle_);
  }
  scoped_handle(const scoped_handle &) = delete;
  scoped_handle &operator=(const scoped_handle &) = delete;
  Handle get() const { return handle_; }

 private:
  Handle handle_;
  Close close_;
};

// Cap both retry count and allocation when objects change during enumeration.
void grow(std::vector<wchar_t> &buffer, DWORD required) {
  const size_t size = (std::max)(buffer.size() * 2, static_cast<size_t>(required) + 1);
  if (size > 1024 * 1024) throw std::runtime_error("Cluster API returned an excessive string length");
  buffer.resize(size);
}

template <typename Fn>
Fn resolve(HMODULE module, const char *name) {
  const auto address = GetProcAddress(module, name);
  if (!address) throw failure(std::string("Cluster API not available: ") + name, GetLastError());
  Fn result;
  static_assert(sizeof(result) == sizeof(address), "Function pointer size mismatch");
  std::memcpy(&result, &address, sizeof(result));
  return result;
}

record read_group(const native::api &api, HCLUSTER cluster, const std::wstring &name) {
  const auto handle = api.open_group(cluster, name.c_str(), GENERIC_READ, nullptr);
  if (!handle) throw object_failure("OpenClusterGroupEx", name, GetLastError());
  scoped_handle<HGROUP, decltype(api.close_group)> guard(handle, api.close_group);
  std::vector<wchar_t> owner(256);
  for (int attempt = 0; attempt < 8; ++attempt) {
    DWORD size = static_cast<DWORD>(owner.size());
    const auto state = api.group_state(handle, owner.data(), &size);
    if (state != ClusterGroupStateUnknown) return {utf8::cvt<std::string>(name), utf8::cvt<std::string>(std::wstring(owner.data())), "", "", state};
    const DWORD error = GetLastError();
    if (error != ERROR_MORE_DATA) throw failure("GetClusterGroupState '" + utf8::cvt<std::string>(name) + "'", error);
    grow(owner, size);
  }
  throw std::runtime_error("Cluster group owner kept changing while reading");
}

std::string resource_type(const native::api &api, HRESOURCE resource) {
  std::vector<wchar_t> type(256);
  for (int attempt = 0; attempt < 8; ++attempt) {
    DWORD bytes = 0;
    const DWORD error = api.resource_control(resource, nullptr, CLUSCTL_RESOURCE_GET_RESOURCE_TYPE, nullptr, 0, type.data(),
                                             static_cast<DWORD>(type.size() * sizeof(wchar_t)), &bytes);
    if (error == ERROR_SUCCESS) return utf8::cvt<std::string>(std::wstring(type.data()));
    if (error != ERROR_MORE_DATA) throw failure("CLUSCTL_RESOURCE_GET_RESOURCE_TYPE", error);
    grow(type, bytes / sizeof(wchar_t));
  }
  throw std::runtime_error("Cluster resource type kept changing while reading");
}

record read_resource(const native::api &api, HCLUSTER cluster, const std::wstring &name) {
  const auto handle = api.open_resource(cluster, name.c_str(), GENERIC_READ, nullptr);
  if (!handle) throw object_failure("OpenClusterResourceEx", name, GetLastError());
  scoped_handle<HRESOURCE, decltype(api.close_resource)> guard(handle, api.close_resource);
  std::vector<wchar_t> owner(256), group(256);
  for (int attempt = 0; attempt < 8; ++attempt) {
    DWORD owner_size = static_cast<DWORD>(owner.size()), group_size = static_cast<DWORD>(group.size());
    const auto state = api.resource_state(handle, owner.data(), &owner_size, group.data(), &group_size);
    if (state != ClusterResourceStateUnknown) {
      return {utf8::cvt<std::string>(name), utf8::cvt<std::string>(std::wstring(owner.data())), utf8::cvt<std::string>(std::wstring(group.data())),
              resource_type(api, handle), state};
    }
    const DWORD error = GetLastError();
    if (error != ERROR_MORE_DATA) throw failure("GetClusterResourceState '" + utf8::cvt<std::string>(name) + "'", error);
    grow(owner, owner_size);
    grow(group, group_size);
  }
  throw std::runtime_error("Cluster resource ownership kept changing while reading");
}

record read_node(const native::api &api, HCLUSTER cluster, const std::wstring &name) {
  const auto handle = api.open_node(cluster, name.c_str(), GENERIC_READ, nullptr);
  if (!handle) throw object_failure("OpenClusterNodeEx", name, GetLastError());
  scoped_handle<HNODE, decltype(api.close_node)> guard(handle, api.close_node);
  const auto state = api.node_state(handle);
  if (state == ClusterNodeStateUnknown) throw object_failure("GetClusterNodeState", name, GetLastError());
  return {utf8::cvt<std::string>(name), "", "", "", state};
}

record read_network(const native::api &api, HCLUSTER cluster, const std::wstring &name) {
  const auto handle = api.open_network(cluster, name.c_str(), GENERIC_READ, nullptr);
  if (!handle) throw object_failure("OpenClusterNetworkEx", name, GetLastError());
  scoped_handle<HNETWORK, decltype(api.close_network)> guard(handle, api.close_network);
  const auto state = api.network_state(handle);
  if (state == ClusterNetworkStateUnknown) throw object_failure("GetClusterNetworkState", name, GetLastError());
  return {utf8::cvt<std::string>(name), "", "", "", state};
}
}  // namespace

std::vector<record> native::fetch(const api &api, object_kind kind) {
  const auto cluster = api.open_cluster(nullptr, GENERIC_READ, nullptr);
  if (!cluster) throw failure("OpenClusterEx (local cluster unavailable or inaccessible)", GetLastError());
  scoped_handle<HCLUSTER, decltype(api.close_cluster)> cluster_guard(cluster, api.close_cluster);
  DWORD enum_type = CLUSTER_ENUM_GROUP;
  switch (kind) {
    case object_kind::group:
      enum_type = CLUSTER_ENUM_GROUP;
      break;
    case object_kind::resource:
      enum_type = CLUSTER_ENUM_RESOURCE;
      break;
    case object_kind::node:
      enum_type = CLUSTER_ENUM_NODE;
      break;
    case object_kind::network:
      enum_type = CLUSTER_ENUM_NETWORK;
      break;
  }
  const auto enumeration = api.open_enum(cluster, enum_type);
  if (!enumeration) throw failure("ClusterOpenEnum", GetLastError());
  scoped_handle<HCLUSENUM, decltype(api.close_enum)> enum_guard(enumeration, api.close_enum);
  std::vector<record> rows;
  std::vector<wchar_t> name(256);
  for (DWORD index = 0;; ++index) {
    DWORD error = ERROR_MORE_DATA;
    for (int attempt = 0; attempt < 8 && error == ERROR_MORE_DATA; ++attempt) {
      DWORD size = static_cast<DWORD>(name.size()), returned_type = 0;
      error = api.enumerate(enumeration, index, &returned_type, name.data(), &size);
      if (error == ERROR_MORE_DATA) grow(name, size);
    }
    if (error == ERROR_NO_MORE_ITEMS) return rows;
    if (error != ERROR_SUCCESS) throw failure("ClusterEnum", error);
    const std::wstring object_name(name.data());
    switch (kind) {
      case object_kind::group:
        rows.push_back(read_group(api, cluster, object_name));
        break;
      case object_kind::resource:
        rows.push_back(read_resource(api, cluster, object_name));
        break;
      case object_kind::node:
        rows.push_back(read_node(api, cluster, object_name));
        break;
      case object_kind::network:
        rows.push_back(read_network(api, cluster, object_name));
        break;
    }
  }
}

std::vector<record> fetch_local(object_kind kind) {
  // Use an absolute system path: neither the working directory nor PATH may
  // supply this DLL. No ClusAPI import is added to CheckWindowsApps itself.
  wchar_t directory[MAX_PATH + 1] = {};
  const UINT length = GetSystemDirectoryW(directory, MAX_PATH + 1);
  if (!length || length > MAX_PATH) throw failure("GetSystemDirectoryW", GetLastError());
  const auto module = LoadLibraryW((std::wstring(directory) + L"\\clusapi.dll").c_str());
  if (!module) throw failure("Failover Clustering API not available (clusapi.dll)", GetLastError());
  scoped_handle<HMODULE, decltype(&FreeLibrary)> library(module, &FreeLibrary);
  native::api api;
#define CLUSTER_RESOLVE(member, function) api.member = resolve<decltype(api.member)>(module, #function)
  CLUSTER_RESOLVE(open_cluster, OpenClusterEx);
  CLUSTER_RESOLVE(close_cluster, CloseCluster);
  CLUSTER_RESOLVE(open_enum, ClusterOpenEnum);
  CLUSTER_RESOLVE(enumerate, ClusterEnum);
  CLUSTER_RESOLVE(close_enum, ClusterCloseEnum);
  CLUSTER_RESOLVE(open_group, OpenClusterGroupEx);
  CLUSTER_RESOLVE(close_group, CloseClusterGroup);
  CLUSTER_RESOLVE(group_state, GetClusterGroupState);
  CLUSTER_RESOLVE(open_resource, OpenClusterResourceEx);
  CLUSTER_RESOLVE(close_resource, CloseClusterResource);
  CLUSTER_RESOLVE(resource_state, GetClusterResourceState);
  CLUSTER_RESOLVE(resource_control, ClusterResourceControl);
  CLUSTER_RESOLVE(open_node, OpenClusterNodeEx);
  CLUSTER_RESOLVE(close_node, CloseClusterNode);
  CLUSTER_RESOLVE(node_state, GetClusterNodeState);
  CLUSTER_RESOLVE(open_network, OpenClusterNetworkEx);
  CLUSTER_RESOLVE(close_network, CloseClusterNetwork);
  CLUSTER_RESOLVE(network_state, GetClusterNetworkState);
#undef CLUSTER_RESOLVE
  return native::fetch(api, kind);
}
}  // namespace check_cluster
