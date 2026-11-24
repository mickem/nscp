// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "smart_data.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>

namespace smart_check {

std::string to_string(const smart_state s) {
  switch (s) {
    case smart_state::available:
      return "available";
    case smart_state::unsupported:
      return "unsupported";
    case smart_state::denied:
      return "denied";
    case smart_state::error:
    default:
      return "error";
  }
}

std::string to_string(const smart_health h) {
  switch (h) {
    case smart_health::passed:
      return "PASSED";
    case smart_health::failed:
      return "FAILED";
    case smart_health::unknown:
    default:
      return "UNKNOWN";
  }
}

namespace detail {
std::string trim(const std::string &s) {
  const char *ws = " \t\r\n";
  std::string out = s;
  // NULs pad the fixed-size fields of both ATA IDENTIFY and the Windows descriptor.
  while (!out.empty() && (out.back() == '\0' || std::string(ws).find(out.back()) != std::string::npos)) out.pop_back();
  std::size_t start = 0;
  while (start < out.size() && (out[start] == '\0' || std::string(ws).find(out[start]) != std::string::npos)) ++start;
  return out.substr(start);
}

std::string ata_string(const unsigned char *words, const std::size_t bytes) {
  std::string out;
  out.reserve(bytes);
  for (std::size_t i = 0; i + 1 < bytes; i += 2) {
    out.push_back(static_cast<char>(words[i + 1]));
    out.push_back(static_cast<char>(words[i]));
  }
  return trim(out);
}

int ata_status_from_registers(const unsigned char lba_mid, const unsigned char lba_high) {
  if (lba_mid == 0x4F && lba_high == 0xC2) return 0;
  if (lba_mid == 0xF4 && lba_high == 0x2C) return 1;
  return -1;
}

int ata_status_from_sense(const unsigned char *sense, const std::size_t length) {
  if (sense == nullptr || length < 8) return -1;
  const unsigned char response_code = sense[0] & 0x7F;
  if (response_code == 0x72 || response_code == 0x73) {
    // Descriptor format: walk the descriptors after the 8-byte header.
    const std::size_t end = std::min<std::size_t>(length, 8 + static_cast<std::size_t>(sense[7]));
    std::size_t d = 8;
    while (d + 1 < end) {
      const unsigned char code = sense[d];
      const std::size_t len = static_cast<std::size_t>(sense[d + 1]) + 2;
      if (code == 0x09 && len >= 14 && d + 13 < length) return ata_status_from_registers(sense[d + 9], sense[d + 11]);
      d += len;
    }
    return -1;
  }
  if ((response_code == 0x70 || response_code == 0x71) && length >= 12) {
    // Fixed format: information bytes 3..6 are ERROR, STATUS, DEVICE, COUNT;
    // command-specific bytes 8..11 are flags, LBA LOW, LBA MID, LBA HIGH.
    return ata_status_from_registers(sense[10], sense[11]);
  }
  return -1;
}

std::string media_from_rotation_rate(const unsigned word217) {
  if (word217 == 1) return "ssd";
  if (word217 >= 0x0401 && word217 <= 0xFFFE) return "hdd";
  return "unknown";
}

bool sg_transport_failed(const unsigned host_status, const unsigned driver_status) { return host_status != 0 || (driver_status & 0x07) != 0; }

smart_state ata_read_failure_state(const bool ata_bus, const bool refused_by_driver) {
  return ata_bus && !refused_by_driver ? smart_state::error : smart_state::unsupported;
}
}  // namespace detail

std::string attribute_name(const int id) {
  // The names smartctl prints by default (drivedb's generic entries), so a
  // reader can search for an attribute exactly as other tools show it.
  static const std::map<int, std::string> names = {
      {1, "Raw_Read_Error_Rate"},
      {2, "Throughput_Performance"},
      {3, "Spin_Up_Time"},
      {4, "Start_Stop_Count"},
      {5, "Reallocated_Sector_Ct"},
      {7, "Seek_Error_Rate"},
      {8, "Seek_Time_Performance"},
      {9, "Power_On_Hours"},
      {10, "Spin_Retry_Count"},
      {11, "Calibration_Retry_Count"},
      {12, "Power_Cycle_Count"},
      {13, "Read_Soft_Error_Rate"},
      {169, "Remaining_Lifetime_Perc"},
      {170, "Available_Reservd_Space"},
      {171, "Program_Fail_Count"},
      {172, "Erase_Fail_Count"},
      {173, "Wear_Leveling_Count"},
      {174, "Unexpect_Power_Loss_Ct"},
      {175, "Program_Fail_Count_Chip"},
      {177, "Wear_Leveling_Count"},
      {179, "Used_Rsvd_Blk_Cnt_Tot"},
      {180, "Unused_Rsvd_Blk_Cnt_Tot"},
      {181, "Program_Fail_Cnt_Total"},
      {182, "Erase_Fail_Count_Total"},
      {183, "Runtime_Bad_Block"},
      {184, "End-to-End_Error"},
      {187, "Reported_Uncorrect"},
      {188, "Command_Timeout"},
      {189, "High_Fly_Writes"},
      {190, "Airflow_Temperature_Cel"},
      {191, "G-Sense_Error_Rate"},
      {192, "Power-Off_Retract_Count"},
      {193, "Load_Cycle_Count"},
      {194, "Temperature_Celsius"},
      {195, "Hardware_ECC_Recovered"},
      {196, "Reallocated_Event_Count"},
      {197, "Current_Pending_Sector"},
      {198, "Offline_Uncorrectable"},
      {199, "UDMA_CRC_Error_Count"},
      {200, "Multi_Zone_Error_Rate"},
      {202, "Percent_Lifetime_Remain"},
      {220, "Disk_Shift"},
      {222, "Loaded_Hours"},
      {223, "Load_Retry_Count"},
      {224, "Load_Friction"},
      {225, "Load_Cycle_Count"},
      {226, "Load-in_Time"},
      {231, "SSD_Life_Left"},
      {232, "Available_Reservd_Space"},
      {233, "Media_Wearout_Indicator"},
      {240, "Head_Flying_Hours"},
      {241, "Total_LBAs_Written"},
      {242, "Total_LBAs_Read"},
      {254, "Free_Fall_Sensor"},
  };
  const auto it = names.find(id);
  if (it != names.end()) return it->second;
  return "Attribute_" + std::to_string(id);
}

namespace {
// The table: a 2-byte revision, then 30 entries of 12 bytes.
constexpr std::size_t table_offset = 2;
constexpr std::size_t entry_size = 12;
constexpr std::size_t entry_count = 30;
constexpr std::size_t table_end = table_offset + entry_size * entry_count;  // 362

std::uint64_t le(const std::vector<unsigned char> &b, const std::size_t offset, const std::size_t bytes) {
  std::uint64_t v = 0;
  for (std::size_t i = 0; i < bytes; ++i) v |= static_cast<std::uint64_t>(b[offset + i]) << (8 * i);
  return v;
}

// An NVMe 128-bit counter, clamped to what a long long holds: the high half
// is non-zero only after ~10^22 events.
long long le128(const std::vector<unsigned char> &b, const std::size_t offset) {
  const std::uint64_t low = le(b, offset, 8);
  const std::uint64_t high = le(b, offset + 8, 8);
  constexpr std::uint64_t max = static_cast<std::uint64_t>(9223372036854775807LL);
  if (high != 0 || low > max) return static_cast<long long>(max);
  return static_cast<long long>(low);
}

const smart_attribute *find(const std::vector<smart_attribute> &attributes, const int id) {
  for (const smart_attribute &a : attributes) {
    if (a.id == id) return &a;
  }
  return nullptr;
}

// Counters are kept in the low 32 bits by every vendor; several pack other
// things (a timestamp, the worst-case value) into the top 16.
long long low32(const smart_attribute &a) { return static_cast<long long>(a.raw & 0xFFFFFFFFULL); }

long long counter(const std::vector<smart_attribute> &attributes, const int id) {
  const smart_attribute *a = find(attributes, id);
  return a == nullptr ? 0 : low32(*a);
}
}  // namespace

std::vector<smart_attribute> parse_ata_attributes(const std::vector<unsigned char> &data, const std::vector<unsigned char> &thresholds) {
  if (data.size() < table_end) throw std::invalid_argument("SMART data is " + std::to_string(data.size()) + " bytes, expected 512");
  const bool have_thresholds = thresholds.size() >= table_end;
  std::vector<smart_attribute> out;
  for (std::size_t i = 0; i < entry_count; ++i) {
    const std::size_t o = table_offset + i * entry_size;
    const int id = data[o];
    if (id == 0) continue;
    smart_attribute a;
    a.id = id;
    a.name = attribute_name(id);
    a.flags = static_cast<int>(le(data, o + 1, 2));
    a.current = data[o + 3];
    a.worst = data[o + 4];
    a.raw = le(data, o + 5, 6);
    if (have_thresholds) {
      // The thresholds sector lists the same ids, normally in the same order;
      // look the id up rather than trust the position.
      for (std::size_t j = 0; j < entry_count; ++j) {
        const std::size_t t = table_offset + j * entry_size;
        if (thresholds[t] == id) {
          a.threshold = thresholds[t + 1];
          break;
        }
      }
    }
    out.push_back(a);
  }
  return out;
}

void apply_ata_attributes(smart_drive &drive, const std::vector<smart_attribute> &attributes, const bool *status_failed) {
  drive.attributes = attributes;

  drive.reallocated_sectors = counter(attributes, 5);
  drive.reallocation_events = counter(attributes, 196);
  drive.pending_sectors = counter(attributes, 197);
  drive.uncorrectable = counter(attributes, 198);
  drive.reported_uncorrectable = counter(attributes, 187);
  drive.crc_errors = counter(attributes, 199);

  if (const smart_attribute *a = find(attributes, 9)) drive.power_on_hours = low32(*a);
  if (const smart_attribute *a = find(attributes, 12)) drive.power_cycles = low32(*a);

  // Temperature: the low byte of 194, else of 190 (190 is 100 - temperature in
  // its normalised value on some drives, but its raw low byte is degrees on all
  // of them). A value outside what a running drive can be is a vendor encoding
  // this decoder does not know, and is not reported rather than reported wrong.
  for (const int id : {194, 190}) {
    if (const smart_attribute *a = find(attributes, id)) {
      const long long t = static_cast<long long>(a->raw & 0xFF);
      if (t > 0 && t < 150) {
        drive.temperature = t;
        break;
      }
    }
  }

  // SSD wear: the vendors that report a lifetime keep the percentage
  // *remaining* in the normalised value of one of these (Kingston/SandForce
  // 231, Intel 233, Samsung 177, Micron/Crucial 202, WD/SanDisk 169). The same
  // ids mean other things on some hard drives, so only a drive that says it
  // is solid-state is read; and a normalised value is 1..253, so 0 is an
  // unused slot rather than a worn-out drive.
  for (const int id : {231, 233, 177, 202, 169}) {
    if (drive.media != "ssd") break;
    if (const smart_attribute *a = find(attributes, id)) {
      if (a->current >= 1 && a->current <= 100) {
        drive.wear = 100 - a->current;
        break;
      }
    }
  }

  bool prefail_failing = false;
  for (const smart_attribute &a : attributes) {
    if (a.prefailure() && a.failing_now()) prefail_failing = true;
  }
  if (status_failed != nullptr) {
    drive.health = (*status_failed || prefail_failing) ? smart_health::failed : smart_health::passed;
  } else if (prefail_failing) {
    drive.health = smart_health::failed;
  } else {
    drive.health = attributes.empty() ? smart_health::unknown : smart_health::passed;
  }
}

void apply_nvme_health_log(smart_drive &drive, const std::vector<unsigned char> &log) {
  if (log.size() < 512) throw std::invalid_argument("NVMe health log is " + std::to_string(log.size()) + " bytes, expected 512");
  drive.media = "ssd";
  drive.critical_warning = log[0];
  const long long kelvin = static_cast<long long>(le(log, 1, 2));
  if (kelvin > 0) drive.temperature = kelvin - 273;
  drive.available_spare = log[3];
  drive.available_spare_threshold = log[4];
  // Percentage used may exceed 100 (the drive has outlived its rated
  // endurance); the spec caps it at 255.
  drive.wear = log[5];
  // A data unit is 1000 512-byte blocks.
  const long long units_read = le128(log, 32);
  const long long units_written = le128(log, 48);
  constexpr long long unit = 512000;
  constexpr long long cap = 9223372036854775807LL / unit;
  drive.data_read = units_read > cap ? cap * unit : units_read * unit;
  drive.data_written = units_written > cap ? cap * unit : units_written * unit;
  drive.power_cycles = le128(log, 112);
  drive.power_on_hours = le128(log, 128);
  drive.unsafe_shutdowns = le128(log, 144);
  drive.media_errors = le128(log, 160);
  drive.error_log_entries = le128(log, 176);
  // smartctl's verdict: any critical warning bit is a failed self-assessment.
  drive.health = drive.critical_warning != 0 ? smart_health::failed : smart_health::passed;
}

long long smart_drive::get_failing_attributes() const {
  long long n = 0;
  for (const smart_attribute &a : attributes) {
    if (a.failing_now()) ++n;
  }
  return n;
}

std::string smart_drive::get_failing_list() const {
  std::string out;
  for (const smart_attribute &a : attributes) {
    if (!a.failing_now()) continue;
    if (!out.empty()) out += ", ";
    out += a.name;
  }
  return out;
}

std::string smart_drive::get_attribute_list() const {
  std::ostringstream ss;
  bool first = true;
  for (const smart_attribute &a : attributes) {
    if (!first) ss << ", ";
    first = false;
    ss << a.name << "=" << a.raw;
  }
  return ss.str();
}

std::string smart_drive::get_critical_warning_text() const {
  static const char *bits[] = {"available spare below threshold", "temperature out of range",          "reliability degraded", "media read-only",
                               "volatile memory backup failed",   "persistent memory region read-only"};
  std::string out;
  for (int i = 0; i < 6; ++i) {
    if ((critical_warning & (1LL << i)) == 0) continue;
    if (!out.empty()) out += ", ";
    out += bits[i];
  }
  return out;
}

std::string smart_drive::get_summary() const {
  if (state == smart_state::unsupported) return "SMART not supported" + (error.empty() ? std::string() : " (" + error + ")");
  if (state != smart_state::available) return "SMART not readable: " + error;

  std::vector<std::string> problems;
  const std::string failing = get_failing_list();
  if (!failing.empty()) problems.push_back(failing + " failing");
  const std::string warning = get_critical_warning_text();
  if (!warning.empty()) problems.push_back(warning);
  if (reallocated_sectors > 0) problems.push_back(std::to_string(reallocated_sectors) + " reallocated");
  if (pending_sectors > 0) problems.push_back(std::to_string(pending_sectors) + " pending");
  if (uncorrectable > 0) problems.push_back(std::to_string(uncorrectable) + " uncorrectable");
  if (reported_uncorrectable > 0) problems.push_back(std::to_string(reported_uncorrectable) + " reported uncorrectable");
  if (media_errors > 0) problems.push_back(std::to_string(media_errors) + " media errors");

  std::string out = to_string(health);
  if (!problems.empty()) {
    out += ":";
    bool first = true;
    for (const std::string &p : problems) {
      out += first ? " " : ", ";
      out += p;
      first = false;
    }
  }
  if (temperature != not_reported) out += ", " + std::to_string(temperature) + "C";
  if (wear != not_reported) out += ", " + std::to_string(wear) + "% worn";
  return out;
}

}  // namespace smart_check
