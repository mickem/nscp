// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "mach_stats_darwin.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_set.h>

#include <cstring>

namespace mach_stats {

bool read_vm_statistics(vm_statistics64_data_t &stats, unsigned long long &page_size, std::string &error) {
  std::memset(&stats, 0, sizeof(stats));
  vm_size_t size = 0;
  if (host_page_size(mach_host_self(), &size) != KERN_SUCCESS || size == 0) {
    error = "host_page_size failed";
    return false;
  }
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  const kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count);
  if (kr != KERN_SUCCESS) {
    error = "host_statistics64 failed: " + std::string(mach_error_string(kr));
    return false;
  }
  page_size = size;
  return true;
}

bool read_thread_count(long long &threads, std::string &error) {
  processor_set_name_t pset = MACH_PORT_NULL;
  kern_return_t kr = processor_set_default(mach_host_self(), &pset);
  if (kr != KERN_SUCCESS) {
    error = "processor_set_default failed: " + std::string(mach_error_string(kr));
    return false;
  }
  processor_set_load_info_data_t info;
  std::memset(&info, 0, sizeof(info));
  mach_msg_type_number_t count = PROCESSOR_SET_LOAD_INFO_COUNT;
  kr = processor_set_statistics(pset, PROCESSOR_SET_LOAD_INFO, reinterpret_cast<processor_set_info_t>(&info), &count);
  mach_port_deallocate(mach_task_self(), pset);
  if (kr != KERN_SUCCESS) {
    error = "processor_set_statistics failed: " + std::string(mach_error_string(kr));
    return false;
  }
  threads = info.thread_count;
  return true;
}

}  // namespace mach_stats
