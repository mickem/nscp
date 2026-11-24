// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Linux SMART reader. Every disk under /sys/block is a candidate; SATA
// drives (and USB bridges that implement SAT) are asked through SG_IO with an
// ATA PASS-THROUGH(16) CDB, NVMe controllers through NVME_IOCTL_ADMIN_CMD.
// Both need root: SG_IO with an ATA pass-through CDB needs CAP_SYS_RAWIO and
// the NVMe admin command CAP_SYS_ADMIN, so an agent running as an ordinary
// user gets smart_state=denied rather than silently reading nothing.

#include <fcntl.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <boost/filesystem.hpp>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>

#include "smart_data.hpp"

#if defined(__has_include)
#if __has_include(<linux/nvme_ioctl.h>)
#include <linux/nvme_ioctl.h>
#define NSCP_HAVE_NVME_IOCTL 1
#endif
#endif

namespace smart_check {

namespace {

namespace fs = boost::filesystem;

std::string read_sysfs(const fs::path &p) {
  std::ifstream in(p.string());
  if (!in) return "";
  std::string line;
  std::getline(in, line);
  return detail::trim(line);
}

// RAII for a file descriptor.
struct fd_guard {
  int fd;
  explicit fd_guard(const int fd) : fd(fd) {}
  ~fd_guard() {
    if (fd >= 0) ::close(fd);
  }
  fd_guard(const fd_guard &) = delete;
  fd_guard &operator=(const fd_guard &) = delete;
};

bool is_permission_error(const int err) { return err == EACCES || err == EPERM; }

std::string permission_message(const std::string &device, const std::string &capability) {
  return "permission denied on " + device + " (run the agent as root or grant it " + capability + ")";
}

// Block devices that are never a physical drive.
bool is_pseudo_device(const std::string &name) {
  static const char *prefixes[] = {"loop", "ram", "zram", "dm-", "md", "sr", "nbd", "fd", "zd", "rbd", "bcache", "drbd", "mmcblk"};
  for (const char *p : prefixes) {
    if (name.compare(0, std::strlen(p), p) == 0) return true;
  }
  return false;
}

// --- ATA over SG_IO ----------------------------------------------------------

enum class sg_result { ok, denied, unsupported, failed };

struct ata_command {
  unsigned char command = 0;
  unsigned char features = 0;
  unsigned char lba_mid = 0;
  unsigned char lba_high = 0;
  bool data_in = false;  // PIO data-in of one sector, else non-data
};

sg_result ata_pass_through(const int fd, const ata_command &cmd, std::vector<unsigned char> &data, std::vector<unsigned char> &sense, std::string &error) {
  unsigned char cdb[16] = {0};
  cdb[0] = 0x85;  // ATA PASS-THROUGH(16)
  if (cmd.data_in) {
    cdb[1] = 4 << 1;  // protocol: PIO data-in
    cdb[2] = 0x0E;    // T_DIR from device, BYT_BLOK, T_LENGTH in the sector count
  } else {
    cdb[1] = 3 << 1;  // protocol: non-data
    cdb[2] = 0x20;    // CK_COND: return the registers in the sense data
  }
  cdb[4] = cmd.features;
  cdb[6] = 1;  // sector count
  cdb[10] = cmd.lba_mid;
  cdb[12] = cmd.lba_high;
  cdb[14] = cmd.command;

  data.assign(cmd.data_in ? 512 : 0, 0);
  sense.assign(64, 0);

  sg_io_hdr_t io;
  std::memset(&io, 0, sizeof(io));
  io.interface_id = 'S';
  io.cmd_len = sizeof(cdb);
  io.cmdp = cdb;
  io.mx_sb_len = static_cast<unsigned char>(sense.size());
  io.sbp = sense.data();
  io.dxfer_direction = cmd.data_in ? SG_DXFER_FROM_DEV : SG_DXFER_NONE;
  io.dxfer_len = static_cast<unsigned int>(data.size());
  io.dxferp = data.empty() ? nullptr : data.data();
  io.timeout = 10000;  // ms; SMART reads are answered from the drive's cache

  if (::ioctl(fd, SG_IO, &io) < 0) {
    const int err = errno;
    if (is_permission_error(err)) return sg_result::denied;
    if (err == EINVAL || err == ENOTTY || err == EOPNOTSUPP) return sg_result::unsupported;
    error = std::string("SG_IO failed: ") + std::strerror(err);
    return sg_result::failed;
  }
  sense.resize(io.sb_len_wr);
  if (io.host_status != 0 || (io.driver_status & 0x0F) > 0x08) {
    error = "SG_IO transport error (host " + std::to_string(io.host_status) + ", driver " + std::to_string(io.driver_status) + ")";
    return sg_result::failed;
  }
  if (io.status == 0) return sg_result::ok;
  // CHECK CONDITION. With CK_COND that is how the registers come back.
  if (!cmd.data_in && detail::ata_status_from_sense(sense.data(), sense.size()) >= 0) return sg_result::ok;
  // ILLEGAL REQUEST: the device (a SAS disk, a virtual disk, a bridge without
  // SAT) does not understand ATA PASS-THROUGH.
  unsigned char key = 0;
  if (!sense.empty()) {
    const unsigned char rc = sense[0] & 0x7F;
    if ((rc == 0x72 || rc == 0x73) && sense.size() > 1) key = sense[1] & 0x0F;
    if ((rc == 0x70 || rc == 0x71) && sense.size() > 2) key = sense[2] & 0x0F;
  }
  if (key == 0x05) return sg_result::unsupported;
  // ABORTED COMMAND from a drive that has SMART disabled, or any other error.
  error = "ATA command " + std::to_string(cmd.command) + " failed (SCSI status " + std::to_string(io.status) + ", sense key " + std::to_string(key) + ")";
  return sg_result::failed;
}

void read_ata(smart_drive &drive, const fs::path &sys) {
  drive.protocol = read_sysfs(sys / "device" / "vendor") == "ATA" ? "ata" : "scsi";
  drive.model = read_sysfs(sys / "device" / "model");
  drive.firmware = read_sysfs(sys / "device" / "rev");

  const int fd = ::open(drive.drive.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    const int err = errno;
    drive.state = is_permission_error(err) ? smart_state::denied : smart_state::error;
    drive.error = is_permission_error(err) ? permission_message(drive.drive, "CAP_SYS_RAWIO") : std::string("open failed: ") + std::strerror(err);
    return;
  }
  fd_guard guard(fd);

  std::vector<unsigned char> identify, sense;
  std::string error;
  ata_command id_cmd;
  id_cmd.command = 0xEC;  // IDENTIFY DEVICE
  id_cmd.data_in = true;
  const sg_result r = ata_pass_through(fd, id_cmd, identify, sense, error);
  if (r == sg_result::denied) {
    drive.state = smart_state::denied;
    drive.error = permission_message(drive.drive, "CAP_SYS_RAWIO");
    return;
  }
  if (r != sg_result::ok) {
    // Not an ATA device behind this path. A SAS disk keeps its health in SCSI
    // log pages, which this check does not read.
    drive.state = r == sg_result::unsupported || drive.protocol == "scsi" ? smart_state::unsupported : smart_state::error;
    drive.error = r == sg_result::unsupported || drive.protocol == "scsi" ? "device does not accept ATA pass-through (SCSI/SAS log pages are not read)" : error;
    return;
  }
  drive.protocol = "ata";
  drive.serial = detail::ata_string(&identify[20], 20);   // words 10-19
  drive.firmware = detail::ata_string(&identify[46], 8);  // words 23-26
  drive.model = detail::ata_string(&identify[54], 40);    // words 27-46
  // Word 82 bit 0: SMART feature set supported; word 85 bit 0: enabled.
  const unsigned w82 = identify[164] | (identify[165] << 8);
  const unsigned w85 = identify[170] | (identify[171] << 8);
  if ((w82 & 1) == 0) {
    drive.state = smart_state::unsupported;
    drive.error = "drive does not implement SMART";
    return;
  }
  if ((w85 & 1) == 0) {
    drive.state = smart_state::error;
    drive.error = "SMART is disabled on the drive (enable it with 'smartctl -s on')";
    return;
  }

  std::vector<unsigned char> values, thresholds;
  ata_command smart_cmd;
  smart_cmd.command = 0xB0;  // SMART
  smart_cmd.lba_mid = 0x4F;
  smart_cmd.lba_high = 0xC2;
  smart_cmd.data_in = true;
  smart_cmd.features = 0xD0;  // READ DATA
  if (ata_pass_through(fd, smart_cmd, values, sense, error) != sg_result::ok) {
    drive.state = smart_state::error;
    drive.error = "SMART READ DATA failed: " + error;
    return;
  }
  smart_cmd.features = 0xD1;  // READ THRESHOLDS (obsolete in ACS-3 but answered by every drive that has them)
  if (ata_pass_through(fd, smart_cmd, thresholds, sense, error) != sg_result::ok) thresholds.clear();

  ata_command status_cmd = smart_cmd;
  status_cmd.features = 0xDA;  // RETURN STATUS
  status_cmd.data_in = false;
  std::vector<unsigned char> none;
  int status = -1;
  if (ata_pass_through(fd, status_cmd, none, sense, error) == sg_result::ok) status = detail::ata_status_from_sense(sense.data(), sense.size());

  drive.source = "sg_io";
  drive.state = smart_state::available;
  const bool failed = status == 1;
  apply_ata_attributes(drive, parse_ata_attributes(values, thresholds), status >= 0 ? &failed : nullptr);
}

// --- NVMe --------------------------------------------------------------------

void read_nvme(smart_drive &drive, const std::string &controller, const std::string &namespace_device) {
  drive.protocol = "nvme";
  const fs::path sys = fs::path("/sys/class/nvme") / controller;
  drive.model = read_sysfs(sys / "model");
  drive.serial = read_sysfs(sys / "serial");
  drive.firmware = read_sysfs(sys / "firmware_rev");
#ifdef NSCP_HAVE_NVME_IOCTL
  // The controller character device takes admin commands; fall back to the
  // namespace block device on systems where the former is missing.
  std::string device = "/dev/" + controller;
  if (!fs::exists(device)) device = namespace_device;
  const int fd = ::open(device.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    const int err = errno;
    drive.state = is_permission_error(err) ? smart_state::denied : smart_state::error;
    drive.error = is_permission_error(err) ? permission_message(device, "CAP_SYS_ADMIN") : std::string("open failed: ") + std::strerror(err);
    return;
  }
  fd_guard guard(fd);

  std::vector<unsigned char> log(512, 0);
  struct nvme_admin_cmd cmd;
  std::memset(&cmd, 0, sizeof(cmd));
  cmd.opcode = 0x02;  // Get Log Page
  cmd.nsid = 0xFFFFFFFF;
  cmd.addr = reinterpret_cast<std::uintptr_t>(log.data());
  cmd.data_len = static_cast<unsigned int>(log.size());
  cmd.cdw10 = ((static_cast<unsigned int>(log.size() / 4) - 1) << 16) | 0x02;  // NUMDL | LID 02h (SMART / Health)
  cmd.timeout_ms = 10000;
  if (::ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd) < 0) {
    const int err = errno;
    drive.state = is_permission_error(err) ? smart_state::denied : smart_state::error;
    drive.error = is_permission_error(err) ? permission_message(device, "CAP_SYS_ADMIN") : std::string("NVMe Get Log Page failed: ") + std::strerror(err);
    return;
  }
  drive.source = "nvme_ioctl";
  drive.state = smart_state::available;
  apply_nvme_health_log(drive, log);
#else
  (void)namespace_device;
  drive.state = smart_state::unsupported;
  drive.error = "built without NVMe ioctl support";
#endif
}

}  // namespace

std::vector<smart_drive> read_drives() {
  std::vector<smart_drive> drives;
  const fs::path block("/sys/block");
  if (!fs::is_directory(block)) return drives;

  std::vector<std::string> names;
  for (fs::directory_iterator it(block), end; it != end; ++it) names.push_back(it->path().filename().string());
  std::sort(names.begin(), names.end());

  static const std::regex nvme_namespace("^(nvme[0-9]+)n[0-9]+$");
  std::set<std::string> nvme_seen;
  for (const std::string &name : names) {
    if (is_pseudo_device(name)) continue;
    const fs::path sys = block / name;
    // A device without a backing "device" link is a software construct.
    if (!fs::exists(sys / "device")) continue;

    smart_drive drive;
    std::smatch m;
    if (std::regex_match(name, m, nvme_namespace)) {
      const std::string controller = m[1];
      // One controller may have several namespaces; its health log is one.
      if (!nvme_seen.insert(controller).second) continue;
      drive.drive = "/dev/" + controller;
      read_nvme(drive, controller, "/dev/" + name);
    } else if (name.compare(0, 2, "vd") == 0 || name.compare(0, 3, "xvd") == 0) {
      drive.drive = "/dev/" + name;
      drive.protocol = "virtual";
      drive.model = read_sysfs(sys / "device" / "model");
      drive.state = smart_state::unsupported;
      drive.error = "virtual disk";
    } else if (name.compare(0, 2, "sd") == 0 || name.compare(0, 2, "hd") == 0) {
      drive.drive = "/dev/" + name;
      read_ata(drive, sys);
    } else if (name.compare(0, 4, "nvme") == 0) {
      // A multipath path device (nvme0c0n1): its controller is listed through
      // the nvmeXnY head.
      continue;
    } else {
      drive.drive = "/dev/" + name;
      drive.protocol = "unknown";
      drive.state = smart_state::unsupported;
      drive.error = "unknown device type";
    }
    drives.push_back(drive);
  }
  return drives;
}

}  // namespace smart_check
