// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The platform-neutral half of check_smart: the record a drive is reported as,
// and the decoders for the two on-the-wire formats the per-OS readers fetch
// (smart_source_win.cpp, smart_source_linux.cpp):
//
//  * ATA/SATA: the 512-byte SMART READ DATA and SMART READ THRESHOLDS sectors
//    (ATA8-ACS, the "vendor specific" attribute table every vendor fills in the
//    same 12-byte layout), plus the SMART RETURN STATUS verdict.
//  * NVMe: the 512-byte SMART / Health Information log page (log id 02h).
//
// The same 512-byte ATA blobs are what Windows exposes as
// MSStorageDriver_FailurePredictData / ...Thresholds.VendorSpecific in
// root\WMI, so the WMI fallback goes through the same decoder.
namespace smart_check {

// One row of the ATA attribute table.
struct smart_attribute {
  int id = 0;
  std::string name;
  int flags = 0;
  int current = 0;        // normalised value, 1..253 (higher is better)
  int worst = 0;          // lowest normalised value seen
  int threshold = 0;      // the vendor's failure threshold for `current`, 0 = never fails
  std::uint64_t raw = 0;  // 48-bit raw value, vendor-specific meaning

  bool prefailure() const { return (flags & 0x01) != 0; }
  // smartctl's FAILING_NOW: the normalised value is at or below a non-zero threshold.
  bool failing_now() const { return threshold > 0 && current > 0 && current <= threshold; }
};

// Whether SMART data could be read for a drive, and if not why. The check's
// default filter drops `unsupported` (virtual disks, USB bridges, RAID volumes
// that do not pass commands through), so a VM does not look like a problem;
// `denied` and `error` stay in and warn, because a check that silently reads
// nothing is worse than one that says it cannot.
enum class smart_state { available, unsupported, denied, error };
std::string to_string(smart_state s);

// The verdict the drive gives about itself: ATA RETURN STATUS / NVMe critical
// warning / WMI PredictFailure.
enum class smart_health { passed, failed, unknown };
std::string to_string(smart_health h);

// A value the drive does not report is -1 for measurements (temperature,
// wear, hours) and 0 for error counters (an NVMe drive has no "reallocated
// sectors", and 0 is what a counter of nothing reads).
constexpr long long not_reported = -1;

struct smart_drive {
  std::string drive;  // \\.\PhysicalDrive0, /dev/sda, /dev/nvme0
  std::string model;
  std::string serial;
  std::string firmware;
  std::string protocol;  // ata, nvme, scsi, virtual, unknown
  // ssd, hdd or unknown: ATA IDENTIFY word 217 (nominal media rotation rate),
  // ssd for every NVMe drive. Gates `wear`, whose attribute ids are not
  // reserved for life-remaining on spinning drives.
  std::string media = "unknown";
  std::string source;  // how it was read: ioctl, wmi, sg_io, nvme_ioctl
  smart_state state = smart_state::unsupported;
  std::string error;  // why state != available
  smart_health health = smart_health::unknown;

  // Measurements: not_reported when the drive does not say.
  long long temperature = not_reported;  // degrees Celsius
  long long power_on_hours = not_reported;
  long long power_cycles = not_reported;
  long long wear = not_reported;                       // percent of rated endurance used (SSD)
  long long available_spare = not_reported;            // NVMe, percent
  long long available_spare_threshold = not_reported;  // NVMe, percent
  long long data_written = not_reported;               // bytes (NVMe data units written)
  long long data_read = not_reported;                  // bytes (NVMe data units read)

  // Error counters: 0 when the drive does not keep them.
  long long reallocated_sectors = 0;     // ATA 5
  long long reallocation_events = 0;     // ATA 196
  long long pending_sectors = 0;         // ATA 197
  long long uncorrectable = 0;           // ATA 198 (offline uncorrectable)
  long long reported_uncorrectable = 0;  // ATA 187
  long long crc_errors = 0;              // ATA 199 (interface CRC errors: cabling)
  long long media_errors = 0;            // NVMe media and data integrity errors
  long long error_log_entries = 0;       // NVMe number of error information log entries
  long long unsafe_shutdowns = 0;        // NVMe unsafe shutdowns
  long long critical_warning = 0;        // NVMe critical warning bit field

  std::vector<smart_attribute> attributes;

  long long get_predict_failure() const { return health == smart_health::failed ? 1 : 0; }
  long long get_failing_attributes() const;
  std::string get_failing_list() const;
  std::string get_attribute_list() const;
  std::string get_critical_warning_text() const;
  // One human line: "PASSED, 34C, 3% worn" or "FAILED: Reallocated_Sector_Ct failing".
  std::string get_summary() const;
  std::string get_name() const { return model.empty() ? drive : drive + " (" + model + ")"; }
  // What the filter engine names the record by in its own messages.
  std::string show() const { return drive; }
};

// The smartctl name of a well-known ATA attribute ("Reallocated_Sector_Ct"),
// or "Attribute_<id>" for one with no common meaning.
std::string attribute_name(int id);

// Decodes a SMART READ DATA sector and, when given (may be empty), the
// matching SMART READ THRESHOLDS sector. Entries with id 0 are skipped. Throws
// std::invalid_argument for a buffer shorter than the 362 bytes the table spans.
std::vector<smart_attribute> parse_ata_attributes(const std::vector<unsigned char> &data, const std::vector<unsigned char> &thresholds);

// Fills the drive's derived values (temperature, hours, counters, wear) from
// drive.attributes, and the health verdict: `status_failed` is the SMART
// RETURN STATUS result when the reader got one. Without one, a pre-failure
// attribute at or below its threshold is the drive's own definition of failed.
void apply_ata_attributes(smart_drive &drive, const std::vector<smart_attribute> &attributes, const bool *status_failed);

// Decodes an NVMe SMART / Health Information log page into the drive. Throws
// std::invalid_argument for a buffer shorter than the 512-byte page.
void apply_nvme_health_log(smart_drive &drive, const std::vector<unsigned char> &log);

// Drives the platform found, in device order. Every drive the OS has is
// returned, including the ones whose state is `unsupported`; the per-OS file
// decides which devices count as drives at all.
std::vector<smart_drive> read_drives();

namespace detail {
// ATA IDENTIFY strings are byte-swapped in pairs and space padded.
std::string ata_string(const unsigned char *words, std::size_t bytes);
// Trim spaces and NULs from both ends.
std::string trim(const std::string &s);

// SMART RETURN STATUS answers in the LBA mid/high registers: 4Fh/C2h when no
// threshold is exceeded, F4h/2Ch when one is. 0 = passed, 1 = failed, -1 =
// neither (the command did not reach the drive, or a bridge mangled it).
int ata_status_from_registers(unsigned char lba_mid, unsigned char lba_high);

// The same verdict from the sense data an ATA PASS-THROUGH with CK_COND
// returns: descriptor-format sense carrying an ATA Status Return descriptor
// (09h), or fixed-format sense with the registers in the information and
// command-specific fields (SAT-2 12.2.2.6). -1 when neither is present.
int ata_status_from_sense(const unsigned char *sense, std::size_t length);

// ATA IDENTIFY word 217: 1 is a non-rotating (solid-state) device, 0401h to
// FFFEh a spinning one in rpm; 0 (not reported) and the reserved values read
// as unknown.
std::string media_from_rotation_rate(unsigned word217);

// Whether an SG_IO call failed in transport rather than in the command: a
// non-zero host status, or a driver status whose low three bits carry an error
// (DRIVER_BUSY .. DRIVER_HARD). DRIVER_SENSE (08h) alone only says that sense
// data came back, which CK_COND asks for.
bool sg_transport_failed(unsigned host_status, unsigned driver_status);

// The state of a drive whose SMART read failed. Only a drive on an ATA/SATA
// bus is expected to answer, so only there is a failure an `error` (which
// warns); behind anything else - USB, SD, SAS, iSCSI, RAID, an unknown bus -
// or when the driver refused the command outright, the device simply does
// not offer ATA SMART and is `unsupported`.
smart_state ata_read_failure_state(bool ata_bus, bool refused_by_driver);
}  // namespace detail

}  // namespace smart_check
