// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only
#pragma once

#include <Windows.h>
// Declarations only: Ex functions are resolved at runtime, including in XP builds.
#ifndef CLUSAPI_VERSION
#define CLUSAPI_VERSION 0x00000700
#endif
#include <clusapi.h>

#include "cluster_source.hpp"

namespace check_cluster {
namespace native {
// A narrow read-only API table also lets tests exercise native error handling.
struct api {
  decltype(&OpenClusterEx) open_cluster = nullptr;
  decltype(&CloseCluster) close_cluster = nullptr;
  decltype(&ClusterOpenEnum) open_enum = nullptr;
  decltype(&ClusterEnum) enumerate = nullptr;
  decltype(&ClusterCloseEnum) close_enum = nullptr;
  decltype(&OpenClusterGroupEx) open_group = nullptr;
  decltype(&CloseClusterGroup) close_group = nullptr;
  decltype(&GetClusterGroupState) group_state = nullptr;
  decltype(&OpenClusterResourceEx) open_resource = nullptr;
  decltype(&CloseClusterResource) close_resource = nullptr;
  decltype(&GetClusterResourceState) resource_state = nullptr;
  decltype(&ClusterResourceControl) resource_control = nullptr;
  decltype(&OpenClusterNodeEx) open_node = nullptr;
  decltype(&CloseClusterNode) close_node = nullptr;
  decltype(&GetClusterNodeState) node_state = nullptr;
  decltype(&OpenClusterNetworkEx) open_network = nullptr;
  decltype(&CloseClusterNetwork) close_network = nullptr;
  decltype(&GetClusterNetworkState) network_state = nullptr;
};
std::vector<record> fetch(const api &functions, object_kind kind);
}  // namespace native
}  // namespace check_cluster
