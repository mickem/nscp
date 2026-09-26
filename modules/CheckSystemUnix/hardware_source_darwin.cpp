// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The hardware readers and the host facts on macOS.
//
//  - Batteries: the IOKit power sources (charge, source, charging, time
//    remaining) and the AppleSmartBattery registry entry (capacities, voltage,
//    current). Both are public API and readable unprivileged.
//  - Temperatures and CPU frequency: nothing. The SMC sensors are reachable
//    only through the undocumented AppleSMC user client or the private
//    IOHIDEventSystem, and Apple silicon's per-cluster clocks only through
//    powermetrics as root or the private IOReport framework; all of them
//    change across releases. Both checks therefore report their documented
//    no-data result on a Mac rather than a guess.
//  - Host facts: uname, sysctl and the os-version mapping check_os_version
//    uses.

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "check_battery.h"
#include "check_cpu_frequency.h"
#include "check_os_version.h"
#include "check_temperature.h"
#include "system_facts.h"

namespace {

// A number from a CF dictionary, when present and numeric.
bool cf_number(CFDictionaryRef dict, CFStringRef key, long long &out) {
  if (dict == nullptr) return false;
  const CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (value == nullptr || CFGetTypeID(value) != CFNumberGetTypeID()) return false;
  return CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberLongLongType, &out);
}

bool cf_bool(CFDictionaryRef dict, CFStringRef key, bool &out) {
  if (dict == nullptr) return false;
  const CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (value == nullptr) return false;
  if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
    out = CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    return true;
  }
  long long n = 0;
  if (CFGetTypeID(value) == CFNumberGetTypeID() && CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberLongLongType, &n)) {
    out = n != 0;
    return true;
  }
  return false;
}

std::string cf_string(CFDictionaryRef dict, CFStringRef key) {
  if (dict == nullptr) return "";
  const CFTypeRef value = CFDictionaryGetValue(dict, key);
  if (value == nullptr || CFGetTypeID(value) != CFStringGetTypeID()) return "";
  char buffer[256] = {0};
  if (!CFStringGetCString(static_cast<CFStringRef>(value), buffer, sizeof(buffer), kCFStringEncodingUTF8)) return "";
  return buffer;
}

// The internal battery's registry properties (AppleSmartBattery), owned by
// the caller; nullptr on a Mac without one.
CFMutableDictionaryRef copy_smart_battery() {
  io_service_t service = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("AppleSmartBattery"));
  if (service == IO_OBJECT_NULL) return nullptr;
  CFMutableDictionaryRef props = nullptr;
  if (IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault, 0) != KERN_SUCCESS) props = nullptr;
  IOObjectRelease(service);
  return props;
}

// The capacities, voltage and current of the internal battery, in the units
// battery_info uses (mWh, mW). The registry reports mAh, mV and mA; Apple
// silicon keeps the raw capacities under AppleRaw* and turns MaxCapacity and
// CurrentCapacity into percentages, Intel Macs report mAh in the plain keys.
void apply_smart_battery(CFDictionaryRef props, battery_check::battery_info &info) {
  long long voltage_mv = 0;
  if (!cf_number(props, CFSTR("Voltage"), voltage_mv) || voltage_mv <= 0) return;
  const auto to_mwh = [voltage_mv](const long long mah) { return mah * voltage_mv / 1000; };

  long long design = 0, full = 0, current = 0;
  if (cf_number(props, CFSTR("DesignCapacity"), design) && design > 0) info.design_capacity = to_mwh(design);
  if (!cf_number(props, CFSTR("AppleRawMaxCapacity"), full)) {
    if (!cf_number(props, CFSTR("MaxCapacity"), full) || full <= 100) full = 0;
  }
  if (full > 0) info.full_capacity = to_mwh(full);
  if (!cf_number(props, CFSTR("AppleRawCurrentCapacity"), current)) {
    if (!cf_number(props, CFSTR("CurrentCapacity"), current) || full == 0) current = 0;
  }
  if (current > 0) info.remaining_capacity = to_mwh(current);
  if (info.design_capacity > 0 && info.full_capacity > 0) info.health_percent = info.full_capacity * 100 / info.design_capacity;

  long long amperage = 0;
  if (cf_number(props, CFSTR("InstantAmperage"), amperage) || cf_number(props, CFSTR("Amperage"), amperage)) {
    const long long rate = (amperage < 0 ? -amperage : amperage) * voltage_mv / 1000;
    if (info.status == "charging") info.charge_rate = rate;
    if (info.status == "discharging") info.discharge_rate = rate;
  }
}

}  // namespace

battery_check::batteries_type battery_check::read_battery() {
  batteries_type result;
  CFTypeRef blob = IOPSCopyPowerSourcesInfo();
  if (blob == nullptr) return result;
  CFArrayRef sources = IOPSCopyPowerSourcesList(blob);
  if (sources == nullptr) {
    CFRelease(blob);
    return result;
  }

  // What the machine runs on right now: "AC Power", "Battery Power" or "UPS
  // Power". It is the same for every battery, as the Linux reader folds the
  // mains adapter into each one.
  std::string providing;
  if (CFStringRef type = IOPSGetProvidingPowerSourceType(blob)) {
    char buffer[64] = {0};
    if (CFStringGetCString(type, buffer, sizeof(buffer), kCFStringEncodingUTF8)) providing = buffer;
  }
  const std::string power_source = providing == kIOPMACPowerKey ? "ac" : providing == kIOPMBatteryPowerKey ? "battery" : "unknown";

  CFMutableDictionaryRef smart = nullptr;
  const CFIndex count = CFArrayGetCount(sources);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef desc = IOPSGetPowerSourceDescription(blob, CFArrayGetValueAtIndex(sources, i));
    if (desc == nullptr) continue;
    const std::string type = cf_string(desc, CFSTR(kIOPSTypeKey));
    // Internal batteries and UPSes; the other types are accessories (a
    // keyboard's battery), not what powers the machine.
    if (type != kIOPSInternalBatteryType && type != kIOPSUPSType) continue;

    battery_info info;
    info.name = cf_string(desc, CFSTR(kIOPSNameKey));
    if (info.name.empty()) info.name = type;
    info.power_source = power_source;

    bool present = true;
    cf_bool(desc, CFSTR(kIOPSIsPresentKey), present);
    long long current = 0, max = 0;
    if (cf_number(desc, CFSTR(kIOPSCurrentCapacityKey), current) && cf_number(desc, CFSTR(kIOPSMaxCapacityKey), max) && max > 0) {
      info.charge_percent = current * 100 / max;
      if (info.charge_percent > 100) info.charge_percent = 100;
    }

    bool charging = false, charged = false;
    cf_bool(desc, CFSTR(kIOPSIsChargingKey), charging);
    cf_bool(desc, CFSTR(kIOPSIsChargedKey), charged);
    const std::string state = cf_string(desc, CFSTR(kIOPSPowerSourceStateKey));
    if (charging) {
      info.status = "charging";
    } else if (state == kIOPSBatteryPowerValue) {
      info.status = "discharging";
    } else if (charged || info.charge_percent >= 100) {
      info.status = "full";
    } else if (state == kIOPSACPowerValue) {
      // On mains and not charging: held back by optimised charging or a
      // charge limit.
      info.status = "not_charging";
    }

    // Minutes; -1 while macOS is still estimating. Only meaningful on
    // battery, like the Linux reader's time_to_empty.
    long long minutes = -1;
    if (info.status == "discharging" && cf_number(desc, CFSTR(kIOPSTimeToEmptyKey), minutes) && minutes > 0) info.time_remaining = minutes * 60;

    if (type == kIOPSInternalBatteryType) {
      if (smart == nullptr) smart = copy_smart_battery();
      if (smart != nullptr) apply_smart_battery(smart, info);
    }

    info.battery_present = present && info.charge_percent >= 0;
    result.push_back(info);
  }

  if (smart != nullptr) CFRelease(smart);
  CFRelease(sources);
  CFRelease(blob);
  return result;
}

temperature_check::zones_type temperature_check::read_temperature() { return zones_type(); }

cpu_frequency_check::cpus_type cpu_frequency_check::read_cpu_frequency() { return cpus_type(); }

namespace {

std::string sysctl_string(const char *name) {
  std::size_t length = 0;
  if (sysctlbyname(name, nullptr, &length, nullptr, 0) != 0 || length == 0) return "";
  std::string value(length, '\0');
  if (sysctlbyname(name, &value[0], &length, nullptr, 0) != 0) return "";
  value.resize(std::strlen(value.c_str()));
  return value;
}

bool sysctl_int(const char *name, long long &out) {
  int value = 0;
  std::size_t length = sizeof(value);
  if (sysctlbyname(name, &value, &length, nullptr, 0) != 0) return false;
  out = value;
  return true;
}

}  // namespace

host_facts::facts system_facts::gather() {
  inputs in;
  utsname uts;
  std::memset(&uts, 0, sizeof(uts));
  if (::uname(&uts) == 0) {
    in.uname_sysname = uts.sysname;
    in.uname_release = uts.release;
    in.uname_machine = uts.machine;
  }

  char hostname[256] = {0};
  if (::gethostname(hostname, sizeof(hostname) - 1) != 0) hostname[0] = '\0';
  in.hostname = hostname;
  // A Mac's default name ends in the multicast-DNS pseudo-domain; that says
  // it has no DNS domain, not that it is in one called "local".
  const std::string local = ".local";
  if (in.hostname.size() > local.size() && in.hostname.compare(in.hostname.size() - local.size(), local.size(), local) == 0) {
    in.hostname.resize(in.hostname.size() - local.size());
  }

  in.release = os_version::read_os_release();
  // The firmware counterpart of the SMBIOS strings: every Mac is made by
  // Apple, and hw.model is the model identifier ("Mac14,2"; "VirtualMac2,1"
  // in a guest of Apple's Virtualization framework).
  in.vendor = "Apple";
  in.model = sysctl_string("hw.model");

  long long vmm = 0;
  if (sysctl_int("kern.hv_vmm_present", vmm)) in.hypervisor = vmm != 0;

  long long cores = 0;
  if (sysctl_int("hw.logicalcpu", cores) && cores > 0) in.cpu_cores = static_cast<long>(cores);
  unsigned long long memsize = 0;
  std::size_t length = sizeof(memsize);
  if (sysctlbyname("hw.memsize", &memsize, &length, nullptr, 0) == 0) in.memory_bytes = memsize;

  return build(in);
}
