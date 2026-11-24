// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Windows SMART reader. Each \\.\PhysicalDriveN is opened and asked
// directly:
//
//  * IOCTL_STORAGE_QUERY_PROPERTY (StorageDeviceProperty) for the bus type,
//    model, serial and firmware;
//  * on an NVMe bus, IOCTL_STORAGE_QUERY_PROPERTY with a protocol-specific
//    query for the SMART / Health Information log page (the inbox
//    stornvme.sys answers it since Windows 10);
//  * otherwise SMART_RCV_DRIVE_DATA (IDENTIFY, READ ATTRIBUTES, READ
//    THRESHOLDS) and SMART_SEND_DRIVE_COMMAND (RETURN STATUS), which the
//    inbox ATA/AHCI miniports pass through.
//
// A drive whose controller driver does not pass SMART commands through (many
// RAID and vendor drivers) can still publish the same attribute sectors via
// MSStorageDriver_FailurePredictStatus / ...Data / ...Thresholds in root\WMI;
// those are read once and used for every drive the IOCTLs could not answer.
// Both paths need an elevated agent, which the service is.

#include <windows.h>
#include <winioctl.h>

#include <cctype>
#include <cstddef>
#include <error/error.hpp>
#include <map>
#include <str/utf8.hpp>
#include <string>
#include <vector>
#include <win/wmi/wmi_query.hpp>

#include "smart_data.hpp"

namespace smart_check {

namespace {

// STORAGE_BUS_TYPE values; the XP-era SDK the 32-bit build uses stops at
// BusTypeSata, so the later ones are spelled out.
constexpr int bus_scsi = 1;
constexpr int bus_ata = 3;
constexpr int bus_iscsi = 9;
constexpr int bus_sas = 10;
constexpr int bus_sata = 11;
constexpr int bus_virtual = 14;
constexpr int bus_file_backed_virtual = 15;
constexpr int bus_spaces = 16;
constexpr int bus_nvme = 17;

// The protocol-specific property query (winioctl.h of the Windows 10 SDK),
// declared here for the same reason.
constexpr DWORD property_device_protocol_specific = 50;   // StorageDeviceProtocolSpecificProperty
constexpr DWORD property_adapter_protocol_specific = 49;  // StorageAdapterProtocolSpecificProperty
constexpr DWORD protocol_type_nvme = 3;                   // ProtocolTypeNvme
constexpr DWORD nvme_data_type_log_page = 2;              // NVMeDataTypeLogPage
constexpr DWORD nvme_log_page_health_info = 2;            // NVME_LOG_PAGE_HEALTH_INFO

struct protocol_specific_data {
  DWORD protocol_type;
  DWORD data_type;
  DWORD request_value;
  DWORD request_sub_value;
  DWORD data_offset;
  DWORD data_length;
  DWORD fixed_return_data;
  DWORD reserved[3];
};
struct property_query_header {
  DWORD property_id;
  DWORD query_type;  // PropertyStandardQuery = 0
};
struct protocol_data_descriptor {
  DWORD version;
  DWORD size;
  protocol_specific_data data;
};

struct handle_guard {
  HANDLE h;
  explicit handle_guard(const HANDLE h) : h(h) {}
  ~handle_guard() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
  handle_guard(const handle_guard &) = delete;
  handle_guard &operator=(const handle_guard &) = delete;
};

std::string last_error_text(const DWORD err) { return detail::trim(error::lookup::last_error(err)); }

std::string bus_protocol(const int bus) {
  switch (bus) {
    case bus_ata:
    case bus_sata:
      return "ata";
    case bus_nvme:
      return "nvme";
    case bus_scsi:
    case bus_sas:
    case bus_iscsi:
      return "scsi";
    case bus_virtual:
    case bus_file_backed_virtual:
    case bus_spaces:
      return "virtual";
    default:
      return "unknown";
  }
}

std::string descriptor_string(const std::vector<unsigned char> &buffer, const DWORD offset) {
  if (offset == 0 || offset >= buffer.size()) return "";
  std::string out;
  for (std::size_t i = offset; i < buffer.size() && buffer[i] != 0; ++i) out.push_back(static_cast<char>(buffer[i]));
  return detail::trim(out);
}

// Model, serial, firmware and bus from StorageDeviceProperty. Answered on a
// handle opened without access rights, so it works where SMART does not.
int read_descriptor(const HANDLE h, smart_drive &drive) {
  STORAGE_PROPERTY_QUERY query;
  ZeroMemory(&query, sizeof(query));
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;
  std::vector<unsigned char> buffer(1024, 0);
  DWORD returned = 0;
  if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr)) return -1;
  if (returned < sizeof(STORAGE_DEVICE_DESCRIPTOR)) return -1;
  buffer.resize(returned);
  const STORAGE_DEVICE_DESCRIPTOR *d = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR *>(buffer.data());
  const std::string vendor = descriptor_string(buffer, d->VendorIdOffset);
  const std::string product = descriptor_string(buffer, d->ProductIdOffset);
  drive.model = detail::trim((vendor.empty() || vendor == "ATA" ? std::string() : vendor + " ") + product);
  drive.firmware = descriptor_string(buffer, d->ProductRevisionOffset);
  drive.serial = descriptor_string(buffer, d->SerialNumberOffset);
  return static_cast<int>(d->BusType);
}

// --- NVMe --------------------------------------------------------------------

bool read_nvme_log(const HANDLE h, const DWORD property, std::vector<unsigned char> &log, DWORD &error) {
  const std::size_t header = sizeof(property_query_header);
  std::vector<unsigned char> buffer(header + sizeof(protocol_specific_data) + 512, 0);
  property_query_header *query = reinterpret_cast<property_query_header *>(buffer.data());
  query->property_id = property;
  query->query_type = 0;
  protocol_specific_data *request = reinterpret_cast<protocol_specific_data *>(buffer.data() + header);
  request->protocol_type = protocol_type_nvme;
  request->data_type = nvme_data_type_log_page;
  request->request_value = nvme_log_page_health_info;
  request->request_sub_value = 0;
  request->data_offset = sizeof(protocol_specific_data);
  request->data_length = 512;

  DWORD returned = 0;
  if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, buffer.data(), static_cast<DWORD>(buffer.size()), buffer.data(), static_cast<DWORD>(buffer.size()),
                       &returned, nullptr)) {
    error = GetLastError();
    return false;
  }
  const protocol_data_descriptor *answer = reinterpret_cast<const protocol_data_descriptor *>(buffer.data());
  const std::size_t data_start = offsetof(protocol_data_descriptor, data) + answer->data.data_offset;
  if (answer->data.data_offset < sizeof(protocol_specific_data) || answer->data.data_length < 512 || data_start + 512 > buffer.size()) {
    error = ERROR_INVALID_DATA;
    return false;
  }
  log.assign(buffer.begin() + data_start, buffer.begin() + data_start + 512);
  return true;
}

void read_nvme(const HANDLE h, smart_drive &drive) {
  std::vector<unsigned char> log;
  DWORD error = 0;
  if (!read_nvme_log(h, property_device_protocol_specific, log, error) && !read_nvme_log(h, property_adapter_protocol_specific, log, error)) {
    drive.state = error == ERROR_ACCESS_DENIED ? smart_state::denied : smart_state::error;
    drive.error = "NVMe health log query failed: " + last_error_text(error);
    return;
  }
  drive.source = "ioctl";
  drive.state = smart_state::available;
  apply_nvme_health_log(drive, log);
}

// --- ATA ---------------------------------------------------------------------

bool smart_ioctl(const HANDLE h, const DWORD code, const BYTE command, const BYTE features, std::vector<unsigned char> &out, const DWORD out_bytes,
                 DWORD &error) {
  SENDCMDINPARAMS in;
  ZeroMemory(&in, sizeof(in));
  in.cBufferSize = out_bytes;
  in.irDriveRegs.bFeaturesReg = features;
  in.irDriveRegs.bSectorCountReg = 1;
  in.irDriveRegs.bSectorNumberReg = 1;
  if (command == SMART_CMD) {
    in.irDriveRegs.bCylLowReg = SMART_CYL_LOW;
    in.irDriveRegs.bCylHighReg = SMART_CYL_HI;
  }
  in.irDriveRegs.bDriveHeadReg = 0xA0;
  in.irDriveRegs.bCommandReg = command;
  in.bDriveNumber = 0;

  std::vector<unsigned char> buffer(sizeof(SENDCMDOUTPARAMS) - 1 + out_bytes, 0);
  DWORD returned = 0;
  if (!DeviceIoControl(h, code, &in, sizeof(SENDCMDINPARAMS) - 1, buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr)) {
    error = GetLastError();
    return false;
  }
  const SENDCMDOUTPARAMS *answer = reinterpret_cast<const SENDCMDOUTPARAMS *>(buffer.data());
  if (answer->DriverStatus.bDriverError != 0) {
    error = ERROR_IO_DEVICE;
    return false;
  }
  out.assign(answer->bBuffer, answer->bBuffer + out_bytes);
  return true;
}

// true when the drive answered; `ata_error` holds why not otherwise.
bool read_ata(const HANDLE h, smart_drive &drive, DWORD &ata_error) {
  std::vector<unsigned char> identify, values, thresholds, registers;
  if (!smart_ioctl(h, SMART_RCV_DRIVE_DATA, ID_CMD, 0, identify, IDENTIFY_BUFFER_SIZE, ata_error)) return false;
  if (identify.size() >= 512) {
    const std::string serial = detail::ata_string(&identify[20], 20);
    const std::string firmware = detail::ata_string(&identify[46], 8);
    const std::string model = detail::ata_string(&identify[54], 40);
    if (!model.empty()) drive.model = model;
    if (!serial.empty()) drive.serial = serial;
    if (!firmware.empty()) drive.firmware = firmware;
    const unsigned w82 = identify[164] | (identify[165] << 8);
    const unsigned w85 = identify[170] | (identify[171] << 8);
    drive.media = detail::media_from_rotation_rate(identify[434] | (identify[435] << 8));
    if ((w82 & 1) == 0) {
      drive.protocol = "ata";
      drive.state = smart_state::unsupported;
      drive.error = "drive does not implement SMART";
      return true;
    }
    if ((w85 & 1) == 0) {
      drive.protocol = "ata";
      drive.state = smart_state::error;
      drive.error = "SMART is disabled on the drive";
      return true;
    }
  }
  if (!smart_ioctl(h, SMART_RCV_DRIVE_DATA, SMART_CMD, READ_ATTRIBUTES, values, READ_ATTRIBUTE_BUFFER_SIZE, ata_error)) return false;
  DWORD ignored = 0;
  if (!smart_ioctl(h, SMART_RCV_DRIVE_DATA, SMART_CMD, READ_THRESHOLDS, thresholds, READ_THRESHOLD_BUFFER_SIZE, ignored)) thresholds.clear();
  int status = -1;
  if (smart_ioctl(h, SMART_SEND_DRIVE_COMMAND, SMART_CMD, RETURN_SMART_STATUS, registers, sizeof(IDEREGS), ignored) && registers.size() >= sizeof(IDEREGS)) {
    const IDEREGS *r = reinterpret_cast<const IDEREGS *>(registers.data());
    status = detail::ata_status_from_registers(r->bCylLowReg, r->bCylHighReg);
  }
  drive.protocol = "ata";
  drive.source = "ioctl";
  drive.state = smart_state::available;
  const bool failed = status == 1;
  apply_ata_attributes(drive, parse_ata_attributes(values, thresholds), status >= 0 ? &failed : nullptr);
  return true;
}

// --- root\WMI FailurePredict -------------------------------------------------

struct wmi_predict {
  bool have_status = false;
  bool predict_failure = false;
  std::vector<unsigned char> data;
  std::vector<unsigned char> thresholds;
};

std::vector<unsigned char> get_bytes(const wmi_impl::row &r, const wchar_t *column) {
  std::vector<unsigned char> out;
  CComVariant v;
  if (FAILED(r.row_obj->Get(column, 0, &v, nullptr, nullptr))) return out;
  if (v.vt != (VT_ARRAY | VT_UI1) || v.parray == nullptr) return out;
  LONG lower = 0, upper = -1;
  if (FAILED(SafeArrayGetLBound(v.parray, 1, &lower)) || FAILED(SafeArrayGetUBound(v.parray, 1, &upper)) || upper < lower) return out;
  void *data = nullptr;
  if (FAILED(SafeArrayAccessData(v.parray, &data))) return out;
  const unsigned char *bytes = static_cast<const unsigned char *>(data);
  out.assign(bytes, bytes + (upper - lower + 1));
  SafeArrayUnaccessData(v.parray);
  return out;
}

std::string upper(std::string s) {
  for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  return s;
}

// MSStorageDriver_* instance names are the disk's PNP device id with a "_0"
// instance suffix.
std::string instance_to_pnp(const std::string &instance) {
  const std::string::size_type pos = instance.rfind('_');
  if (pos == std::string::npos) return upper(instance);
  return upper(instance.substr(0, pos));
}

// Keyed by PhysicalDrive index. Empty when root\WMI has nothing (no
// FailurePredict provider, not elevated) - the IOCTL path is the primary one.
// Runs one root\WMI / root\cimv2 query and hands each row to `fn`. A query
// that fails (the class has no provider, the namespace refuses an unelevated
// caller) is skipped on its own, so a missing ...Thresholds class does not
// throw away the PredictFailure verdict that ...Status already gave.
template <class Fn> void each_row(const std::string &wql, const std::string &ns, Fn fn) {
  try {
    wmi_impl::query q(wql, ns, "", "");
    wmi_impl::row_enumerator rows = q.execute();
    while (rows.has_next()) fn(rows.get_next());
  } catch (const std::exception &) {
  }
}

// Keyed by PhysicalDrive index. Empty when root\WMI has nothing (no
// FailurePredict provider, not elevated) - the IOCTL path is the primary one.
std::map<int, wmi_predict> read_wmi() {
  std::map<int, wmi_predict> out;
  std::map<std::string, int> pnp_to_index;
  each_row("select Index, PNPDeviceID from Win32_DiskDrive", "root\\cimv2",
           [&](const wmi_impl::row &r) { pnp_to_index[upper(r.get_string_or_empty("PNPDeviceID"))] = static_cast<int>(r.get_int_or("Index", -1)); });
  // Without the disk list no instance can be tied to a PhysicalDrive.
  if (pnp_to_index.empty()) return out;

  std::map<std::string, wmi_predict> by_pnp;
  each_row("select InstanceName, PredictFailure from MSStorageDriver_FailurePredictStatus", "root\\WMI", [&](const wmi_impl::row &r) {
    wmi_predict &p = by_pnp[instance_to_pnp(r.get_string_or_empty("InstanceName"))];
    p.have_status = true;
    p.predict_failure = r.get_int_or("PredictFailure", 0) != 0;
  });
  each_row("select InstanceName, VendorSpecific from MSStorageDriver_FailurePredictData", "root\\WMI",
           [&](const wmi_impl::row &r) { by_pnp[instance_to_pnp(r.get_string_or_empty("InstanceName"))].data = get_bytes(r, L"VendorSpecific"); });
  each_row("select InstanceName, VendorSpecific from MSStorageDriver_FailurePredictThresholds", "root\\WMI",
           [&](const wmi_impl::row &r) { by_pnp[instance_to_pnp(r.get_string_or_empty("InstanceName"))].thresholds = get_bytes(r, L"VendorSpecific"); });

  for (const auto &e : by_pnp) {
    const auto it = pnp_to_index.find(e.first);
    if (it != pnp_to_index.end() && it->second >= 0) out[it->second] = e.second;
  }
  return out;
}

bool apply_wmi(smart_drive &drive, const wmi_predict &p) {
  if (!p.have_status && p.data.empty()) return false;
  std::vector<smart_attribute> attributes;
  try {
    if (!p.data.empty()) attributes = parse_ata_attributes(p.data, p.thresholds);
  } catch (const std::exception &) {
    attributes.clear();
  }
  drive.source = "wmi";
  drive.state = smart_state::available;
  drive.error.clear();
  const bool failed = p.predict_failure;
  apply_ata_attributes(drive, attributes, p.have_status ? &failed : nullptr);
  return true;
}

}  // namespace

std::vector<smart_drive> read_drives() {
  std::vector<smart_drive> drives;
  std::map<int, wmi_predict> wmi;
  bool wmi_read = false;

  // PhysicalDrive numbers can have gaps (a removed disk keeps its number until
  // reboot), so walk a fixed range rather than stop at the first miss.
  for (int index = 0; index < 128; ++index) {
    const std::wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(index);
    HANDLE probe = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (probe == INVALID_HANDLE_VALUE) continue;
    handle_guard probe_guard(probe);

    smart_drive drive;
    drive.drive = utf8::cvt<std::string>(path);
    const int bus = read_descriptor(probe, drive);
    drive.protocol = bus_protocol(bus);

    // SMART commands need a read/write handle, which needs an elevated agent.
    HANDLE rw = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    const DWORD open_error = rw == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    handle_guard rw_guard(rw);

    if (rw == INVALID_HANDLE_VALUE) {
      drive.state = open_error == ERROR_ACCESS_DENIED ? smart_state::denied : smart_state::error;
      drive.error = open_error == ERROR_ACCESS_DENIED ? "access denied opening " + drive.drive + " (the agent must run elevated)"
                                                      : "failed to open " + drive.drive + ": " + last_error_text(open_error);
    } else if (bus == bus_nvme) {
      read_nvme(rw, drive);
    } else if (bus == bus_virtual || bus == bus_file_backed_virtual || bus == bus_spaces) {
      drive.state = smart_state::unsupported;
      drive.error = "virtual disk";
    } else {
      // Every other bus is worth a try - some RAID and USB drivers pass SMART
      // through - but only an ATA/SATA disk is expected to answer, so only
      // there is a failure reported as an error.
      DWORD ata_error = 0;
      if (!read_ata(rw, drive, ata_error)) {
        // ERROR_INVALID_FUNCTION / ERROR_NOT_SUPPORTED / ERROR_INVALID_PARAMETER:
        // the driver refused the command outright.
        const bool refused = ata_error == ERROR_INVALID_FUNCTION || ata_error == ERROR_NOT_SUPPORTED || ata_error == ERROR_INVALID_PARAMETER;
        drive.state = detail::ata_read_failure_state(bus == bus_ata || bus == bus_sata, refused);
        drive.error = drive.state == smart_state::unsupported ? "the storage driver does not pass SMART commands through (" + last_error_text(ata_error) + ")"
                                                              : "SMART read failed: " + last_error_text(ata_error);
      }
    }

    if (drive.state != smart_state::available && bus != bus_nvme) {
      if (!wmi_read) {
        wmi = read_wmi();
        wmi_read = true;
      }
      const auto it = wmi.find(index);
      if (it != wmi.end()) apply_wmi(drive, it->second);
    }
    drives.push_back(drive);
  }
  return drives;
}

}  // namespace smart_check
