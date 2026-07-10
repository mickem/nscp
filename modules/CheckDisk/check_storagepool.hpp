// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <nscapi/protobuf/command.hpp>
#include <string>

namespace storagepool_check {

// One Storage Spaces pool (MSFT_StoragePool in root\Microsoft\Windows\Storage).
struct storagepool {
  std::string name;
  std::string health_status;       // Healthy / Warning / Unhealthy / Unknown
  std::string operational_status;  // Synthesised: OK / ReadOnly / <health>
  long long capacity;              // Size (total bytes)
  long long used;                  // AllocatedSize (bytes)
  bool is_readonly;

  storagepool() : capacity(0), used(0), is_readonly(false) {}

  // MSFT_StoragePool.HealthStatus (0=Healthy,1=Warning,2=Unhealthy).
  static std::string map_health_status(long long v) {
    switch (v) {
      case 0:
        return "Healthy";
      case 1:
        return "Warning";
      case 2:
        return "Unhealthy";
      default:
        return "Unknown";
    }
  }

  std::string get_name() const { return name; }
  std::string get_health_status() const { return health_status; }
  std::string get_operational_status() const { return operational_status; }
  long long get_capacity() const { return capacity; }
  long long get_used() const { return used; }
  long long get_free() const { return capacity - used; }
  long long get_free_pct() const { return capacity == 0 ? 0 : (get_free() * 100 / capacity); }
  long long get_used_pct() const { return capacity == 0 ? 0 : (used * 100 / capacity); }
  long long get_is_readonly() const { return is_readonly ? 1 : 0; }

  std::string show() const { return name; }
};

typedef std::list<storagepool> pools_type;

// Platform data acquisition: WMI on Windows, empty list on Unix.
pools_type query();

namespace check {
void check_storagepool(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace check

}  // namespace storagepool_check
