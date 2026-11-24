// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_smart.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

// nscapi::plugin_singleton is defined once per test binary: in
// check_disk_io_test.cpp (Windows) or check_disk_unix_test.cpp (Unix).

using smart_check::apply_ata_attributes;
using smart_check::apply_nvme_health_log;
using smart_check::parse_ata_attributes;
using smart_check::smart_attribute;
using smart_check::smart_drive;
using smart_check::smart_health;
using smart_check::smart_state;

namespace {

typedef std::vector<unsigned char> bytes;

struct ata_row {
  int id;
  int flags;
  int current;
  int worst;
  unsigned long long raw;
  int threshold;
};

// Lays the rows out as the SMART READ DATA / READ THRESHOLDS sectors do.
void make_ata(const std::vector<ata_row> &rows, bytes &data, bytes &thresholds) {
  data.assign(512, 0);
  thresholds.assign(512, 0);
  data[0] = 0x10;
  thresholds[0] = 0x10;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const std::size_t o = 2 + i * 12;
    const ata_row &r = rows[i];
    data[o] = static_cast<unsigned char>(r.id);
    data[o + 1] = static_cast<unsigned char>(r.flags & 0xFF);
    data[o + 2] = static_cast<unsigned char>((r.flags >> 8) & 0xFF);
    data[o + 3] = static_cast<unsigned char>(r.current);
    data[o + 4] = static_cast<unsigned char>(r.worst);
    for (int b = 0; b < 6; ++b) data[o + 5 + b] = static_cast<unsigned char>((r.raw >> (8 * b)) & 0xFF);
    // Thresholds in reverse order, to prove they are matched by id.
    const std::size_t t = 2 + (rows.size() - 1 - i) * 12;
    thresholds[t] = static_cast<unsigned char>(r.id);
    thresholds[t + 1] = static_cast<unsigned char>(r.threshold);
  }
}

smart_drive healthy_hdd() {
  bytes data, thresholds;
  make_ata({{1, 0x000F, 200, 200, 0, 51},
            {5, 0x0033, 200, 200, 0, 140},
            {9, 0x0032, 90, 90, 0x00AB00001234ULL, 0},  // high bytes are vendor noise
            {12, 0x0032, 100, 100, 321, 0},
            {194, 0x0022, 112, 98, 0x002D0014001FULL, 0},  // 31C, min/max packed above
            {197, 0x0032, 200, 200, 0, 0},
            {198, 0x0030, 100, 253, 0, 0},
            {199, 0x0032, 200, 200, 7, 0}},
           data, thresholds);
  smart_drive d;
  d.drive = "/dev/sda";
  d.model = "WDC WD40EFRX";
  d.protocol = "ata";
  d.state = smart_state::available;
  const bool failed = false;
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), &failed);
  return d;
}

bytes nvme_log() {
  bytes log(512, 0);
  log[0] = 0;                  // critical warning
  log[1] = (273 + 41) & 0xFF;  // 314 K
  log[2] = (273 + 41) >> 8;
  log[3] = 100;    // available spare
  log[4] = 10;     // threshold
  log[5] = 3;      // percentage used
  log[48] = 0x10;  // 16 data units written
  log[112] = 55;   // power cycles
  log[128] = 0x10;
  log[129] = 0x27;  // 10000 power-on hours
  log[144] = 4;     // unsafe shutdowns
  log[176] = 9;     // error log entries
  return log;
}

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

PB::Common::ResultCode run_check(const std::vector<smart_drive> &drives, const std::vector<std::string> &args,
                                 PB::Commands::QueryResponseMessage::Response &response) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_smart");
  for (const std::string &a : args) request.add_arguments(a);
  smart_check::check_smart_from(request, &response, [&drives]() { return drives; });
  return response.result();
}

}  // namespace

// --- decoders ----------------------------------------------------------------

TEST(SmartAta, ParsesTableAndMatchesThresholdsById) {
  bytes data, thresholds;
  make_ata({{5, 0x0033, 100, 100, 12, 36}, {194, 0x0022, 60, 50, 40, 0}}, data, thresholds);
  const std::vector<smart_attribute> a = parse_ata_attributes(data, thresholds);
  ASSERT_EQ(a.size(), 2u);
  EXPECT_EQ(a[0].id, 5);
  EXPECT_EQ(a[0].name, "Reallocated_Sector_Ct");
  EXPECT_TRUE(a[0].prefailure());
  EXPECT_EQ(a[0].current, 100);
  EXPECT_EQ(a[0].raw, 12u);
  EXPECT_EQ(a[0].threshold, 36);
  EXPECT_FALSE(a[0].failing_now());
  EXPECT_EQ(a[1].name, "Temperature_Celsius");
  EXPECT_FALSE(a[1].prefailure());
  EXPECT_EQ(a[1].threshold, 0);
}

TEST(SmartAta, ThresholdsAreOptional) {
  bytes data, thresholds;
  make_ata({{5, 0x0033, 1, 1, 0, 36}}, data, thresholds);
  const std::vector<smart_attribute> a = parse_ata_attributes(data, bytes());
  ASSERT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0].threshold, 0);
  EXPECT_FALSE(a[0].failing_now());  // no threshold, never failing
}

TEST(SmartAta, RejectsShortBuffer) { EXPECT_THROW(parse_ata_attributes(bytes(100, 0), bytes()), std::invalid_argument); }

TEST(SmartAta, UnknownAttributeGetsGenericName) { EXPECT_EQ(smart_check::attribute_name(123), "Attribute_123"); }

TEST(SmartAta, DerivesCountersAndMeasurements) {
  const smart_drive d = healthy_hdd();
  EXPECT_EQ(d.health, smart_health::passed);
  EXPECT_EQ(d.temperature, 31);
  EXPECT_EQ(d.power_on_hours, 0x1234);  // low 32 bits only
  EXPECT_EQ(d.power_cycles, 321);
  EXPECT_EQ(d.reallocated_sectors, 0);
  EXPECT_EQ(d.crc_errors, 7);
  EXPECT_EQ(d.wear, smart_check::not_reported);  // an HDD has no life-left attribute
  EXPECT_EQ(d.get_failing_attributes(), 0);
  EXPECT_EQ(d.get_summary(), "PASSED, 31C");
}

TEST(SmartAta, SsdWearFromLifeRemaining) {
  bytes data, thresholds;
  make_ata({{177, 0x0013, 93, 93, 120, 0}}, data, thresholds);
  smart_drive d;
  d.media = "ssd";
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(d.wear, 7);
}

TEST(SmartAta, WearIsOnlyReadOnSolidStateDrives) {
  // 177 and friends mean other things on some hard drives.
  bytes data, thresholds;
  make_ata({{177, 0x0013, 5, 5, 120, 0}}, data, thresholds);
  for (const char *media : {"hdd", "unknown"}) {
    smart_drive d;
    d.media = media;
    apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
    EXPECT_EQ(d.wear, smart_check::not_reported) << media;
  }
}

TEST(SmartAta, WearIgnoresAZeroNormalisedValue) {
  // A normalised value is 1..253; 0 is an unused slot, not a worn-out drive.
  // The next candidate (233) is used instead.
  bytes data, thresholds;
  make_ata({{231, 0x0013, 0, 0, 0, 0}, {233, 0x0032, 98, 98, 0, 0}}, data, thresholds);
  smart_drive d;
  d.media = "ssd";
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(d.wear, 2);

  make_ata({{231, 0x0013, 0, 0, 0, 0}}, data, thresholds);
  smart_drive only_zero;
  only_zero.media = "ssd";
  apply_ata_attributes(only_zero, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(only_zero.wear, smart_check::not_reported);
}

TEST(SmartAta, MediaFromRotationRate) {
  EXPECT_EQ(smart_check::detail::media_from_rotation_rate(1), "ssd");
  EXPECT_EQ(smart_check::detail::media_from_rotation_rate(7200), "hdd");
  EXPECT_EQ(smart_check::detail::media_from_rotation_rate(0), "unknown");
  EXPECT_EQ(smart_check::detail::media_from_rotation_rate(0x0002), "unknown");  // reserved
  EXPECT_EQ(smart_check::detail::media_from_rotation_rate(0xFFFF), "unknown");  // reserved
}

TEST(SmartAta, SgTransportFailure) {
  using smart_check::detail::sg_transport_failed;
  EXPECT_FALSE(sg_transport_failed(0, 0));
  EXPECT_FALSE(sg_transport_failed(0, 0x08));   // DRIVER_SENSE: sense data came back, as CK_COND asks
  for (unsigned code = 1; code <= 7; ++code) {  // DRIVER_BUSY .. DRIVER_HARD
    EXPECT_TRUE(sg_transport_failed(0, code)) << code;
    EXPECT_TRUE(sg_transport_failed(0, code | 0x08)) << code;
  }
  EXPECT_TRUE(sg_transport_failed(7, 0));  // DID_ERROR
}

TEST(SmartAta, ReadFailureStateByBus) {
  using smart_check::detail::ata_read_failure_state;
  // Only an ATA/SATA disk that did not simply refuse is an error (and warns).
  EXPECT_EQ(ata_read_failure_state(true, false), smart_state::error);
  EXPECT_EQ(ata_read_failure_state(true, true), smart_state::unsupported);
  // USB, SD, SAS, iSCSI, RAID: whatever the failure, the device has no ATA SMART to offer.
  EXPECT_EQ(ata_read_failure_state(false, false), smart_state::unsupported);
  EXPECT_EQ(ata_read_failure_state(false, true), smart_state::unsupported);
}

TEST(SmartAta, ImplausibleTemperatureIsNotReported) {
  bytes data, thresholds;
  make_ata({{194, 0x0022, 100, 100, 0xFA, 0}}, data, thresholds);
  smart_drive d;
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(d.temperature, smart_check::not_reported);
}

TEST(SmartAta, StatusFailedWins) {
  bytes data, thresholds;
  make_ata({{5, 0x0033, 100, 100, 0, 36}}, data, thresholds);
  smart_drive d;
  d.state = smart_state::available;
  const bool failed = true;
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), &failed);
  EXPECT_EQ(d.health, smart_health::failed);
  EXPECT_EQ(d.get_predict_failure(), 1);
}

TEST(SmartAta, FailingPrefailAttributeWithoutStatusIsFailed) {
  bytes data, thresholds;
  make_ata({{5, 0x0033, 30, 30, 1800, 36}, {197, 0x0032, 200, 200, 8, 0}}, data, thresholds);
  smart_drive d;
  d.state = smart_state::available;
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(d.health, smart_health::failed);
  EXPECT_EQ(d.get_failing_attributes(), 1);
  EXPECT_EQ(d.get_failing_list(), "Reallocated_Sector_Ct");
  EXPECT_EQ(d.get_summary(), "FAILED: Reallocated_Sector_Ct failing, 1800 reallocated, 8 pending");
}

TEST(SmartAta, FailingOldAgeAttributeIsNotAFailedDrive) {
  bytes data, thresholds;
  make_ata({{190, 0x0022, 30, 30, 70, 45}}, data, thresholds);  // too hot, old-age
  smart_drive d;
  apply_ata_attributes(d, parse_ata_attributes(data, thresholds), nullptr);
  EXPECT_EQ(d.health, smart_health::passed);
  EXPECT_EQ(d.get_failing_attributes(), 1);
}

TEST(SmartAta, IdentifyStringsAreByteSwapped) {
  const unsigned char words[] = {'D', 'W', ' ', 'C', 'D', 'W', ' ', ' '};
  EXPECT_EQ(smart_check::detail::ata_string(words, sizeof(words)), "WDC WD");
}

TEST(SmartAta, ReturnStatusRegisters) {
  EXPECT_EQ(smart_check::detail::ata_status_from_registers(0x4F, 0xC2), 0);
  EXPECT_EQ(smart_check::detail::ata_status_from_registers(0xF4, 0x2C), 1);
  EXPECT_EQ(smart_check::detail::ata_status_from_registers(0x00, 0x00), -1);
}

TEST(SmartAta, ReturnStatusFromDescriptorSense) {
  unsigned char sense[22] = {0x72, 0x01, 0x00, 0x1D, 0, 0, 0, 14, 0x09, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x00, 0x2C, 0xA0, 0x50};
  EXPECT_EQ(smart_check::detail::ata_status_from_sense(sense, sizeof(sense)), 1);
  sense[17] = 0x4F;
  sense[19] = 0xC2;
  EXPECT_EQ(smart_check::detail::ata_status_from_sense(sense, sizeof(sense)), 0);
}

TEST(SmartAta, ReturnStatusFromFixedSense) {
  unsigned char sense[18] = {0x70, 0, 0x01, 0, 0x50, 0, 0, 10, 0x00, 0x00, 0x4F, 0xC2, 0, 0x1D, 0, 0, 0, 0};
  EXPECT_EQ(smart_check::detail::ata_status_from_sense(sense, sizeof(sense)), 0);
  EXPECT_EQ(smart_check::detail::ata_status_from_sense(sense, 4), -1);
  EXPECT_EQ(smart_check::detail::ata_status_from_sense(nullptr, 0), -1);
}

TEST(SmartNvme, DecodesHealthLog) {
  smart_drive d;
  d.state = smart_state::available;
  apply_nvme_health_log(d, nvme_log());
  EXPECT_EQ(d.health, smart_health::passed);
  EXPECT_EQ(d.media, "ssd");
  EXPECT_EQ(d.temperature, 41);
  EXPECT_EQ(d.available_spare, 100);
  EXPECT_EQ(d.available_spare_threshold, 10);
  EXPECT_EQ(d.wear, 3);
  EXPECT_EQ(d.data_written, 16LL * 512000);
  EXPECT_EQ(d.data_read, 0);
  EXPECT_EQ(d.power_cycles, 55);
  EXPECT_EQ(d.power_on_hours, 10000);
  EXPECT_EQ(d.unsafe_shutdowns, 4);
  EXPECT_EQ(d.error_log_entries, 9);
  EXPECT_EQ(d.media_errors, 0);
  EXPECT_EQ(d.get_summary(), "PASSED, 41C, 3% worn");
}

TEST(SmartNvme, CriticalWarningFails) {
  bytes log = nvme_log();
  log[0] = 0x05;  // spare below threshold + reliability degraded
  smart_drive d;
  d.state = smart_state::available;
  apply_nvme_health_log(d, log);
  EXPECT_EQ(d.health, smart_health::failed);
  EXPECT_EQ(d.get_critical_warning_text(), "available spare below threshold, reliability degraded");
}

TEST(SmartNvme, HugeCounterSaturates) {
  bytes log = nvme_log();
  log[48 + 8] = 1;  // high half of data units written
  smart_drive d;
  apply_nvme_health_log(d, log);
  EXPECT_GT(d.data_written, 0);
}

TEST(SmartNvme, RejectsShortLog) {
  smart_drive d;
  EXPECT_THROW(apply_nvme_health_log(d, bytes(64, 0)), std::invalid_argument);
}

TEST(SmartSummary, NotReadable) {
  smart_drive d;
  d.state = smart_state::denied;
  d.error = "permission denied";
  EXPECT_EQ(d.get_summary(), "SMART not readable: permission denied");
  d.state = smart_state::unsupported;
  d.error = "virtual disk";
  EXPECT_EQ(d.get_summary(), "SMART not supported (virtual disk)");
}

// --- the check ---------------------------------------------------------------

TEST(SmartCheck, HealthyDrivesAreOk) {
  smart_drive nvme;
  nvme.drive = "/dev/nvme0";
  nvme.model = "Samsung SSD 980";
  nvme.state = smart_state::available;
  apply_nvme_health_log(nvme, nvme_log());
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({healthy_hdd(), nvme}, {}, response), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(response), "OK: /dev/sda (WDC WD40EFRX): PASSED, 31C, /dev/nvme0 (Samsung SSD 980): PASSED, 41C, 3% worn");
}

TEST(SmartCheck, ReallocatedSectorsWarn) {
  smart_drive d = healthy_hdd();
  d.reallocated_sectors = 8;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({d}, {}, response), PB::Common::ResultCode::WARNING);
  EXPECT_NE(join_lines(response).find("/dev/sda (WDC WD40EFRX): PASSED: 8 reallocated, 31C"), std::string::npos) << join_lines(response);
}

TEST(SmartCheck, PredictedFailureIsCritical) {
  smart_drive d = healthy_hdd();
  d.health = smart_health::failed;
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({d}, {}, response), PB::Common::ResultCode::CRITICAL);
}

TEST(SmartCheck, UnsupportedDrivesAreFilteredOut) {
  smart_drive vm;
  vm.drive = "/dev/vda";
  vm.protocol = "virtual";
  vm.state = smart_state::unsupported;
  vm.error = "virtual disk";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({vm}, {}, response), PB::Common::ResultCode::UNKNOWN);
  EXPECT_NE(join_lines(response).find("No drives with SMART data found"), std::string::npos) << join_lines(response);

  PB::Commands::QueryResponseMessage::Response ok_response;
  EXPECT_EQ(run_check({vm}, {"empty-state=ok"}, ok_response), PB::Common::ResultCode::OK);

  // Listed on request, an unsupported drive is not a problem.
  PB::Commands::QueryResponseMessage::Response listed;
  EXPECT_EQ(run_check({vm}, {"filter=none", "top-syntax=${list}"}, listed), PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(listed), "/dev/vda: SMART not supported (virtual disk)");
}

TEST(SmartCheck, UnreadableDriveWarns) {
  smart_drive d;
  d.drive = "/dev/sdb";
  d.model = "ST4000";
  d.state = smart_state::denied;
  d.error = "permission denied on /dev/sdb";
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({d}, {}, response), PB::Common::ResultCode::WARNING);
  EXPECT_NE(join_lines(response).find("SMART not readable: permission denied on /dev/sdb"), std::string::npos) << join_lines(response);
}

TEST(SmartCheck, ShortThresholdBoundsTemperature) {
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(run_check({healthy_hdd()}, {"-w", "30", "-c", "50"}, response), PB::Common::ResultCode::WARNING) << join_lines(response);
  PB::Commands::QueryResponseMessage::Response hot;
  EXPECT_EQ(run_check({healthy_hdd()}, {"-w", "20", "-c", "30"}, hot), PB::Common::ResultCode::CRITICAL) << join_lines(hot);
  // -w keeps the rest of the default warning.
  smart_drive d = healthy_hdd();
  d.pending_sectors = 2;
  PB::Commands::QueryResponseMessage::Response pending;
  EXPECT_EQ(run_check({d}, {"-w", "60"}, pending), PB::Common::ResultCode::WARNING) << join_lines(pending);
}

TEST(SmartCheck, ValuedBooleanAndKeywordsOverRestStyleArguments) {
  smart_drive d = healthy_hdd();
  PB::Commands::QueryResponseMessage::Response response;
  EXPECT_EQ(
      run_check({d}, {"warning=none", "critical=predict_failure = 1", "detail-syntax=${drive}|${health}|${temperature}|${crc_errors}", "top-syntax=${list}"},
                response),
      PB::Common::ResultCode::OK);
  EXPECT_EQ(join_lines(response), "/dev/sda|PASSED|31|7");
}

TEST(SmartCheck, ReaderIsNotCalledForBadOptions) {
  bool called = false;
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_smart");
  request.add_arguments("warning=no_such_keyword > 1");
  PB::Commands::QueryResponseMessage::Response response;
  smart_check::check_smart_from(request, &response, [&called]() {
    called = true;
    return std::vector<smart_drive>();
  });
  EXPECT_FALSE(called);
  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN);
}
