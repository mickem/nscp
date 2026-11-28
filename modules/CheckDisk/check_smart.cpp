// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_smart.hpp"

#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>

namespace smart_check {

using parsers::where::evaluation_context;
using parsers::where::type_bool;
using parsers::where::type_int;
using parsers::where::type_size;

namespace {
typedef std::shared_ptr<smart_drive> obj_type;

std::string human_bytes(const long long bytes, const evaluation_context &context) {
  if (bytes < 0) return "";
  return str::format::format_byte_units(bytes, context->get_number_format());
}

// The default thresholds. A counter the drive does not keep reads 0 and a
// measurement it does not report reads -1, so none of these fires on a drive
// that simply lacks the attribute.
const char *default_warning =
    "smart_state = 'denied' or smart_state = 'error' or failing_attributes > 0 or reallocated_sectors > 0 or pending_sectors > 0 or uncorrectable > 0 or "
    "reported_uncorrectable > 0 or media_errors > 0 or wear > 80";
const char *default_critical = "predict_failure = 1";
}  // namespace

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("drive", [](obj_type o) { return o->drive; }, "Device the drive was read through (\\\\.\\PhysicalDrive0, /dev/sda, /dev/nvme0)")
      .add_string_var("name", [](obj_type o) { return o->get_name(); }, "The device followed by the model in parentheses, or the device alone when the model is unknown")
      .add_string_var("model", [](obj_type o) { return o->model; }, "Drive model as the drive reports it")
      .add_string_var("serial", [](obj_type o) { return o->serial; }, "Drive serial number")
      .add_string_var("firmware", [](obj_type o) { return o->firmware; }, "Drive firmware revision")
      .add_string_var("protocol", [](obj_type o) { return o->protocol; },
                      "How the drive is attached: ata (ATA/SATA), nvme, scsi (SAS and other SCSI disks), virtual or unknown")
      .add_string_var("media", [](obj_type o) { return o->media; },
                      "ssd, hdd or unknown: the drive's reported rotation rate (ATA IDENTIFY word 217); every NVMe drive is ssd")
      .add_string_var("source", [](obj_type o) { return o->source; },
                      "Where the SMART data was read from: ioctl or wmi on Windows, sg_io or nvme_ioctl on Linux; empty when nothing was read")
      .add_string_var("smart_state", [](obj_type o) { return to_string(o->state); },
                      "Whether SMART data could be read: available, unsupported (the device has no SMART, e.g. a virtual or RAID volume), "
                      "denied (insufficient privileges) or error")
      .add_string_var("smart_error", [](obj_type o) { return o->error; }, "Why SMART data could not be read; empty when smart_state is available")
      .add_string_var("health", [](obj_type o) { return to_string(o->health); },
                      "The drive's own verdict: PASSED, FAILED or UNKNOWN (ATA SMART RETURN STATUS, any NVMe critical warning, or WMI PredictFailure)")
      .add_string_var("failing_list", [](obj_type o) { return o->get_failing_list(); },
                      "Names of the ATA attributes whose normalised value is at or below the vendor threshold (smartctl's FAILING_NOW)")
      .add_string_var("critical_warning_text", [](obj_type o) { return o->get_critical_warning_text(); },
                      "The NVMe critical warning bits spelled out (available spare below threshold, temperature out of range, ...)")
      .add_string_var("attributes", [](obj_type o) { return o->get_attribute_list(); },
                      "Every ATA attribute the drive reports, as name=raw pairs")
      .add_string_var("summary", [](obj_type o) { return o->get_summary(); },
                      "One-line description: the verdict, any problems, temperature and wear (\"PASSED, 34C, 3% worn\")");

  registry_.add_int_var("predict_failure", type_bool, [](obj_type o) { return o->get_predict_failure(); },
                        "True when the drive predicts its own failure (health is FAILED)")
      .no_perf();
  registry_.add_int_var("failing_attributes", type_int, [](obj_type o) { return o->get_failing_attributes(); },
                        "Number of ATA attributes at or below their vendor threshold")
      .add_int_perf("", "", "_failing_attributes");
  registry_.add_int_var("temperature", type_int, [](obj_type o) { return o->temperature; },
                        "Drive temperature in degrees Celsius (ATA 194/190, NVMe composite); -1 when not reported")
      .add_int_perf("C", "", "_temperature");
  registry_.add_int_var("power_on_hours", type_int, [](obj_type o) { return o->power_on_hours; }, "Power-on hours (ATA 9, NVMe); -1 when not reported")
      .add_int_perf("", "", "_power_on_hours");
  registry_.add_int_var("power_cycles", type_int, [](obj_type o) { return o->power_cycles; }, "Power cycle count (ATA 12, NVMe); -1 when not reported")
      .add_int_perf("c", "", "_power_cycles");
  registry_.add_int_var("wear", type_int, [](obj_type o) { return o->wear; },
                        "Percent of the rated write endurance used: NVMe percentage used (may exceed 100), or 100 minus the normalised "
                        "life-remaining attribute of an ATA SSD (231, 233, 177, 202 or 169); -1 when not reported, and always -1 unless media is ssd")
      .add_int_perf("%", "", "_wear");
  registry_.add_int_var("available_spare", type_int, [](obj_type o) { return o->available_spare; },
                        "NVMe available spare capacity in percent; -1 when not reported")
      .add_int_perf("%", "", "_available_spare");
  registry_.add_int_var("available_spare_threshold", type_int, [](obj_type o) { return o->available_spare_threshold; },
                        "NVMe available spare threshold in percent, below which the drive raises a critical warning; -1 when not reported")
      .no_perf();
  registry_.add_int_var("reallocated_sectors", type_int, [](obj_type o) { return o->reallocated_sectors; },
                        "Sectors remapped to spare area after a failure (ATA 5); 0 when not kept")
      .add_int_perf("", "", "_reallocated_sectors");
  registry_.add_int_var("reallocation_events", type_int, [](obj_type o) { return o->reallocation_events; },
                        "Remap operations performed (ATA 196); 0 when not kept")
      .add_int_perf("", "", "_reallocation_events");
  registry_.add_int_var("pending_sectors", type_int, [](obj_type o) { return o->pending_sectors; },
                        "Unstable sectors waiting to be remapped (ATA 197); 0 when not kept")
      .add_int_perf("", "", "_pending_sectors");
  registry_.add_int_var("uncorrectable", type_int, [](obj_type o) { return o->uncorrectable; },
                        "Sectors the offline scan could not read (ATA 198); 0 when not kept")
      .add_int_perf("", "", "_uncorrectable");
  registry_.add_int_var("reported_uncorrectable", type_int, [](obj_type o) { return o->reported_uncorrectable; },
                        "Errors that could not be recovered with ECC (ATA 187); 0 when not kept")
      .add_int_perf("", "", "_reported_uncorrectable");
  registry_.add_int_var("crc_errors", type_int, [](obj_type o) { return o->crc_errors; },
                        "Interface CRC errors (ATA 199) - usually a cable or backplane, not the drive; 0 when not kept")
      .add_int_perf("c", "", "_crc_errors");
  registry_.add_int_var("media_errors", type_int, [](obj_type o) { return o->media_errors; },
                        "NVMe unrecovered data integrity errors; 0 when not kept")
      .add_int_perf("", "", "_media_errors");
  registry_.add_int_var("error_log_entries", type_int, [](obj_type o) { return o->error_log_entries; },
                        "NVMe error information log entries over the drive's life; 0 when not kept")
      .add_int_perf("c", "", "_error_log_entries");
  registry_.add_int_var("unsafe_shutdowns", type_int, [](obj_type o) { return o->unsafe_shutdowns; },
                        "NVMe unsafe shutdowns (power lost before the drive was told); 0 when not kept")
      .add_int_perf("c", "", "_unsafe_shutdowns");
  registry_.add_int_var("critical_warning", type_int, [](obj_type o) { return o->critical_warning; },
                        "NVMe critical warning bit field (0 = none): 1 spare, 2 temperature, 4 reliability, 8 read-only, 16 backup, 32 PMR")
      .no_perf();
  registry_.add_int_var("data_written", type_size, [](obj_type o) { return o->data_written; },
                        "Bytes written by the host over the drive's life (NVMe); -1 when not reported")
      .add_int_perf("B", "", "_data_written");
  registry_.add_int_var("data_read", type_size, [](obj_type o) { return o->data_read; },
                        "Bytes read by the host over the drive's life (NVMe); -1 when not reported")
      .add_int_perf("B", "", "_data_read");

  registry_.add_human_string_context("data_written", [](obj_type o, evaluation_context c) { return human_bytes(o->data_written, c); },
                                     "Bytes written over the drive's life as a human-readable size")
      .add_human_string_context("data_read", [](obj_type o, evaluation_context c) { return human_bytes(o->data_read, c); },
                                "Bytes read over the drive's life as a human-readable size");
  // clang-format on
}

void check_smart_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                      const std::function<std::vector<smart_drive>()> &reader) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // The default filter drops devices that have no SMART at all (virtio and
  // other virtual disks, hardware RAID volumes), so a VM lands in the empty
  // state rather than warning. A drive whose data could not be *read*
  // (privileges, an I/O error) stays in and warns.
  filter_helper.add_options(default_warning, default_critical, "smart_state != 'unsupported'", filter.get_filter_syntax(), "unknown");
  // -w / -c bound the temperature: it is the one measurement here with no
  // vendor threshold behind it, and the only thing a bare number can mean.
  filter_helper.add_short_threshold("temperature", modern_filter::shorthand::above);
  filter_helper.add_short_warning(std::string("${temperature} or ") + default_warning);
  filter_helper.add_short_critical(std::string("${temperature} or ") + default_critical);
  // Temperature is graphed for every drive; the counters are graphed because
  // the default thresholds reference them.
  filter_helper.set_default_perf_config("extra(temperature)");
  filter_helper.add_syntax("${status}: ${problem_list}", "${name}: ${summary}", "${drive}", "No drives with SMART data found", "%(status): %(list)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  for (const smart_drive &drive : reader()) {
    const std::shared_ptr<smart_drive> record(new smart_drive(drive));
    filter.match(record);
  }

  filter_helper.post_process(filter);
}

void check_smart(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_smart_from(request, response, &read_drives);
}

}  // namespace smart_check
