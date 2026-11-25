// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_hardware.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <cerrno>
#include <cstdio>
#include <set>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>

namespace hardware_check {

using parsers::where::type_size;

const char *const no_dmi_message =
    "No hardware inventory available: this host exposes no DMI/SMBIOS data (no /sys/class/dmi/id and no SMBIOS table; "
    "typical of containers and microVMs)";

namespace {

// Real SMBIOS tables are a few KiB; this only stops a hostile or broken file
// from being read without end.
const std::size_t kMaxTableBytes = 1024 * 1024;
const std::size_t kMaxEntryPointBytes = 256;
const std::size_t kMaxDmiValueBytes = 4096;
const std::size_t kMaxStringLength = 256;
// Bound on /sys/devices/system/memory entries looked at (12 TiB in 128 MiB blocks
// is ~100k; this leaves ample headroom while bounding a pathological tree).
const std::size_t kMaxMemoryBlocks = 1000000;

// Read at most `limit` bytes. Returns false, with errno in *err, when the file
// cannot be opened or read.
bool read_bounded(const std::string &path, const std::size_t limit, std::string &out, int *err) {
  out.clear();
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    if (err != nullptr) *err = errno;
    return false;
  }
  char buf[4096];
  bool ok = true;
  while (out.size() < limit) {
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n < 0 && errno == EINTR) continue;
    if (n < 0) {
      if (err != nullptr) *err = errno;
      ok = false;
      break;
    }
    if (n == 0) break;
    out.append(buf, static_cast<std::size_t>(n));
  }
  ::close(fd);
  if (out.size() > limit) out.resize(limit);
  return ok;
}

// Keep printable ASCII only (anything else becomes '.', as dmidecode does),
// trim, and cap the length. Firmware strings are untrusted bytes, and a
// non-UTF-8 byte would make the whole response unserialisable.
std::string clean_string(const char *data, const std::size_t size) {
  std::string out;
  const std::size_t n = size < kMaxStringLength ? size : kMaxStringLength;
  out.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const unsigned char c = static_cast<unsigned char>(data[i]);
    out += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.';
  }
  const std::string::size_type b = out.find_first_not_of(' ');
  if (b == std::string::npos) return "";
  const std::string::size_type e = out.find_last_not_of(' ');
  return out.substr(b, e - b + 1);
}

std::string clean_string(const std::string &s) {
  // A sysfs attribute ends in a newline; drop it before cleaning.
  std::string::size_type n = s.size();
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) --n;
  return clean_string(s.data(), n);
}

std::string to_upper(std::string s) {
  for (char &c : s) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  }
  return s;
}

void add_error(std::string &errors, const std::string &e) {
  if (!errors.empty()) errors += "; ";
  errors += e;
}

std::string errno_text(const int err) {
  if (err == EACCES || err == EPERM) return "permission denied (root only)";
  if (err == ENOENT) return "not present";
  return "error " + str::xtos(err);
}

// --- little-endian readers over a bounds-checked formatted area -------------

class formatted_area {
 public:
  formatted_area(const unsigned char *data, std::size_t length, const std::vector<std::string> &strings) : data_(data), length_(length), strings_(strings) {}

  bool has(const std::size_t offset, const std::size_t width) const { return offset <= length_ && width <= length_ - offset; }
  std::size_t length() const { return length_; }

  boost::optional<std::uint8_t> u8(const std::size_t off) const {
    if (!has(off, 1)) return boost::none;
    return data_[off];
  }
  boost::optional<std::uint16_t> u16(const std::size_t off) const {
    if (!has(off, 2)) return boost::none;
    return static_cast<std::uint16_t>(data_[off] | (data_[off + 1] << 8));
  }
  boost::optional<std::uint32_t> u32(const std::size_t off) const {
    if (!has(off, 4)) return boost::none;
    return static_cast<std::uint32_t>(data_[off]) | (static_cast<std::uint32_t>(data_[off + 1]) << 8) | (static_cast<std::uint32_t>(data_[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data_[off + 3]) << 24);
  }
  // The string a 1-based index byte at `off` refers to; empty for index 0
  // ("no string"), an index past the string set, or an offset past the area.
  std::string str(const std::size_t off) const {
    const boost::optional<std::uint8_t> idx = u8(off);
    if (!idx || idx.value() == 0 || idx.value() > strings_.size()) return "";
    return strings_[idx.value() - 1];
  }
  const unsigned char *at(const std::size_t off) const { return data_ + off; }

 private:
  const unsigned char *data_;
  std::size_t length_;
  const std::vector<std::string> &strings_;
};

std::uint32_t read_u32(const unsigned char *p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t read_u16(const unsigned char *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

// Type 17 size: 0 = empty socket (none), 0xFFFF = unknown (0), 0x7FFF = see
// the extended size (MiB), else bit 15 selects KiB over MiB.
boost::optional<long long> module_size(const formatted_area &a) {
  const boost::optional<std::uint16_t> size = a.u16(0x0C);
  if (!size || size.value() == 0) return boost::none;
  const std::uint16_t s = size.value();
  if (s == 0xFFFF) return 0LL;
  if (s == 0x7FFF) {
    const boost::optional<std::uint32_t> ext = a.u32(0x1C);
    if (!ext) return 0LL;
    return static_cast<long long>(ext.value() & 0x7FFFFFFFu) * 1024LL * 1024LL;
  }
  const long long units = static_cast<long long>(s & 0x7FFF);
  return (s & 0x8000) ? units * 1024LL : units * 1024LL * 1024LL;
}

// A type 17 speed word, with 0xFFFF meaning "see the extended dword".
long long module_speed(const formatted_area &a, const std::size_t word_off, const std::size_t ext_off) {
  const boost::optional<std::uint16_t> w = a.u16(word_off);
  if (!w) return 0;
  if (w.value() == 0xFFFF) {
    const boost::optional<std::uint32_t> ext = a.u32(ext_off);
    return ext ? static_cast<long long>(ext.value() & 0x7FFFFFFFu) : 0;
  }
  return w.value();
}

}  // namespace

// --- chassis --------------------------------------------------------------------

std::string chassis_type_to_string(const long long type) {
  // SMBIOS System Enclosure types (spec 7.4.1); the Windows check maps the
  // same numbers from Win32_SystemEnclosure.ChassisTypes.
  switch (type) {
    case 1:
      return "Other";
    case 2:
      return "Unknown";
    case 3:
      return "Desktop";
    case 4:
      return "Low Profile Desktop";
    case 5:
      return "Pizza Box";
    case 6:
      return "Mini Tower";
    case 7:
      return "Tower";
    case 8:
      return "Portable";
    case 9:
      return "Laptop";
    case 10:
      return "Notebook";
    case 11:
      return "Hand Held";
    case 12:
      return "Docking Station";
    case 13:
      return "All in One";
    case 14:
      return "Sub Notebook";
    case 15:
      return "Space-Saving";
    case 16:
      return "Lunch Box";
    case 17:
      return "Main System Chassis";
    case 18:
      return "Expansion Chassis";
    case 19:
      return "SubChassis";
    case 20:
      return "Bus Expansion Chassis";
    case 21:
      return "Peripheral Chassis";
    case 22:
      return "Storage Chassis";
    case 23:
      return "Rack Mount Chassis";
    case 24:
      return "Sealed-Case PC";
    case 30:
      return "Tablet";
    case 31:
      return "Convertible";
    case 32:
      return "Detachable";
    case 33:
      return "IoT Gateway";
    case 34:
      return "Embedded PC";
    case 35:
      return "Mini PC";
    case 36:
      return "Stick PC";
    default:
      return "Unknown (" + str::xtos(type) + ")";
  }
}

// --- SMBIOS ---------------------------------------------------------------------

boost::optional<smbios_entry_point> parse_smbios_entry_point(const std::string &bytes) {
  const unsigned char *p = reinterpret_cast<const unsigned char *>(bytes.data());
  smbios_entry_point e;
  if (bytes.size() >= 0x18 && bytes.compare(0, 5, "_SM3_") == 0) {
    // 64-bit entry point (SMBIOS 3.x): the table length is a maximum.
    if (p[0x06] < 0x18 || p[0x06] > bytes.size()) return boost::none;
    e.major = p[0x07];
    e.minor = p[0x08];
    e.table_length = read_u32(p + 0x0C);
    return e;
  }
  if (bytes.size() >= 0x1E && bytes.compare(0, 4, "_SM_") == 0) {
    // 32-bit entry point (SMBIOS 2.1-2.8): exact length and structure count.
    if (p[0x05] < 0x1E || p[0x05] > bytes.size()) return boost::none;
    if (bytes.compare(0x10, 5, "_DMI_") != 0) return boost::none;
    e.major = p[0x06];
    e.minor = p[0x07];
    e.table_length = read_u16(p + 0x16);
    e.structure_count = read_u16(p + 0x1C);
    return e;
  }
  return boost::none;
}

std::string format_smbios_uuid(const unsigned char *b, const bool little_endian) {
  bool all_zero = true, all_ff = true;
  for (int i = 0; i < 16; ++i) {
    if (b[i] != 0x00) all_zero = false;
    if (b[i] != 0xFF) all_ff = false;
  }
  if (all_zero || all_ff) return "";
  static const int le_order[16] = {3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};
  std::string out;
  char hex[3];
  for (int i = 0; i < 16; ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10) out += '-';
    std::snprintf(hex, sizeof(hex), "%02X", b[little_endian ? le_order[i] : i]);
    out += hex;
  }
  return out;
}

smbios_inventory parse_smbios_table(const std::string &table, const boost::optional<smbios_entry_point> &entry) {
  smbios_inventory inv;
  const unsigned char *data = reinterpret_cast<const unsigned char *>(table.data());
  std::size_t limit = table.size();
  std::size_t max_structures = 65535;
  bool uuid_le = true;
  if (entry) {
    const smbios_entry_point &ep = entry.value();
    if (ep.table_length > 0 && ep.table_length < limit) limit = static_cast<std::size_t>(ep.table_length);
    if (ep.structure_count > 0) max_structures = ep.structure_count;
    // Before 2.6 the UUID byte order was unspecified; read it as stored.
    uuid_le = ep.major > 2 || (ep.major == 2 && ep.minor >= 6);
  }
  bool seen_chassis = false;
  bool seen_system = false;
  // Type 17 records are filtered once the walk is done: the array a device
  // belongs to may be described after the device itself.
  struct device_record {
    boost::optional<std::uint16_t> array;   // Physical Memory Array Handle
    boost::optional<memory_module> module;  // set for a populated socket
  };
  std::vector<device_record> devices;
  std::set<std::uint16_t> system_arrays;
  std::set<std::uint16_t> other_arrays;

  std::size_t pos = 0;
  while (inv.structures < static_cast<long long>(max_structures)) {
    if (pos >= limit) break;
    if (limit - pos < 4) {
      inv.truncated = true;
      break;
    }
    const std::uint8_t type = data[pos];
    const std::uint8_t length = data[pos + 1];
    if (length < 4 || length > limit - pos) {
      inv.truncated = true;
      break;
    }
    // The string set: NUL-terminated strings ending in an extra NUL. With no
    // strings it is just the two NULs.
    const std::size_t strings_start = pos + length;
    std::size_t k = strings_start;
    bool terminated = false;
    while (k + 1 < limit) {
      if (data[k] == 0 && data[k + 1] == 0) {
        terminated = true;
        break;
      }
      ++k;
    }
    if (!terminated) {
      inv.truncated = true;
      break;
    }
    std::vector<std::string> strings;
    {
      std::size_t s = strings_start;
      while (s < k) {
        std::size_t e = s;
        while (e < k && data[e] != 0) ++e;
        strings.push_back(clean_string(table.data() + s, e - s));
        s = e + 1;
      }
    }
    const formatted_area a(data + pos, length, strings);
    inv.structures++;

    if (type == 1 && !seen_system) {
      seen_system = true;
      inv.vendor = a.str(0x04);
      inv.model = a.str(0x05);
      inv.serial = a.str(0x07);
      if (a.has(0x08, 16)) inv.uuid = format_smbios_uuid(a.at(0x08), uuid_le);
    } else if (type == 3 && !seen_chassis) {
      seen_chassis = true;
      const boost::optional<std::uint8_t> ct = a.u8(0x05);
      if (ct) inv.chassis_type = ct.value() & 0x7F;  // bit 7 is the chassis lock
      inv.chassis_serial = a.str(0x07);
      inv.asset_tag = a.str(0x08);
    } else if (type == 16) {
      // Only arrays used for system memory (not video or flash).
      const boost::optional<std::uint8_t> use = a.u8(0x05);
      const boost::optional<std::uint16_t> count = a.u16(0x0D);
      const boost::optional<std::uint16_t> handle = a.u16(0x02);
      if (use && use.value() == 0x03) {
        if (handle) system_arrays.insert(handle.value());
        if (count) {
          inv.have_arrays = true;
          inv.slots += count.value();
        }
      } else if (use && handle) {
        other_arrays.insert(handle.value());
      }
    } else if (type == 17) {
      device_record rec;
      rec.array = a.u16(0x04);
      const boost::optional<long long> size = module_size(a);
      if (size) {
        memory_module m;
        m.capacity = size.value();
        m.locator = a.str(0x10);
        if (m.locator.empty()) m.locator = a.str(0x11);
        const long long configured = module_speed(a, 0x20, 0x58);
        m.speed = configured > 0 ? configured : module_speed(a, 0x15, 0x54);
        m.manufacturer = a.str(0x17);
        m.serial = a.str(0x18);
        m.part_number = a.str(0x1A);
        rec.module = m;
      }
      devices.push_back(rec);
    } else if (type == 127) {
      break;  // end of table
    }
    pos = k + 2;
  }
  for (const device_record &rec : devices) {
    // A device of an array that is not system memory (flash, video, cache)
    // is not a DIMM. A handle claimed by both kinds is kept.
    if (rec.array && other_arrays.count(rec.array.value()) > 0 && system_arrays.count(rec.array.value()) == 0) continue;
    inv.device_records++;
    if (rec.module) inv.modules.push_back(rec.module.value());
  }
  return inv;
}

// --- hardware_info ---------------------------------------------------------------

void hardware_info::recompute() {
  modules = static_cast<long long>(module_details.size());
  long long total = 0;
  long long slowest = 0;
  bool sized = !module_details.empty();
  module_list.clear();
  for (const memory_module &m : module_details) {
    total += m.capacity;
    if (m.capacity <= 0) sized = false;
    if (m.speed > 0 && (slowest == 0 || m.speed < slowest)) slowest = m.speed;
    if (!module_list.empty()) module_list += "; ";
    module_list += m.locator.empty() ? "?" : m.locator;
    module_list += ": " + str::format::format_byte_units(m.capacity);
    if (m.speed > 0) module_list += "@" + str::xtos(m.speed) + "MHz";
  }
  // A module of unknown size makes the sum a partial figure, not a total.
  if (sized) {
    memory = total;
  } else {
    memory = boost::none;
  }
  if (slowest > 0) {
    memory_speed = slowest;
  } else {
    memory_speed = boost::none;
  }
}

std::string hardware_info::get_memory_human(parsers::where::evaluation_context context) const {
  if (!memory) return "unknown";
  return str::format::format_byte_units(memory.value(), context->get_number_format());
}

filter_obj_handler::filter_obj_handler() {
  // clang-format off
  registry_.add_string_var("vendor", &hardware_info::get_vendor, "System vendor/manufacturer (sys_vendor)")
      .add_string_var("model", &hardware_info::get_model, "System model / product name (product_name)")
      .add_string_var("uuid", &hardware_info::get_uuid, "SMBIOS system UUID, upper case (product_uuid; empty unless the agent runs as root)")
      .add_string_var("serial", &hardware_info::get_serial,
                      "System serial number (product_serial; empty unless the agent runs as root, and often blank or placeholder on VMs and OEM boards)")
      .add_string_var("chassis", &hardware_info::get_chassis, "Chassis type name (Desktop, Laptop, Rack Mount Chassis, ...); empty when unknown")
      .add_string_var("chassis_serial", &hardware_info::get_chassis_serial, "Enclosure serial number (chassis_serial; empty unless the agent runs as root)")
      .add_string_var("asset_tag", &hardware_info::get_asset_tag, "SMBIOS asset tag (chassis_asset_tag)")
      .add_string_var("module_list", &hardware_info::get_module_list,
                      "Semicolon-separated per-DIMM inventory (slot: size@speed, e.g. 'DIMM_A1: 32GB@4800MHz'); empty unless the SMBIOS table could be read")
      .add_string_var("memory_source", &hardware_info::get_memory_source,
                      "Where memory came from: 'smbios' (sum of the memory modules), 'sysfs' (memory the kernel manages, in memory-block "
                      "granularity: the SMBIOS table needs root) or 'none'");
  registry_.add_int_var("chassis_type", &hardware_info::get_chassis_type, "Raw SMBIOS chassis type number (0 when unknown)");
  registry_.add_optional_int_var("slots", [](auto obj) { return obj->get_slots(); }, "unknown",
                                 "Total memory sockets on the board; 'unknown' unless the SMBIOS table could be read (root)")
      .add_optional_int_var("modules", [](auto obj) { return obj->get_modules(); }, "unknown",
                            "Number of populated memory modules; 'unknown' unless the SMBIOS table could be read (root), so a threshold such as "
                            "modules < 8 never fires on an unprivileged agent")
      .add_optional_int_var("memory", type_size, [](auto obj) { return obj->get_memory(); }, "unknown",
                            "Total installed memory (supports size units, e.g. 'memory < 64G'); renders human-readable. See memory_source for where it came from")
      .add_optional_int_var("memory_speed", [](auto obj) { return obj->get_memory_speed(); }, "unknown",
                            "Slowest populated module's configured clock in MHz; 'unknown' unless the SMBIOS table could be read and reports a speed");
  // Render ${memory} human-readable (64GB) while expressions keep comparing bytes.
  registry_.add_human_string_context("memory", &hardware_info::get_memory_human, "Total installed memory as a human-readable size");
  // clang-format on
}

// --- gathering --------------------------------------------------------------------

hardware_info gather_hardware_from(const std::string &root) {
  hardware_info out;

  const std::string dmi = root + "/sys/class/dmi/id";
  struct stat st;
  if (::stat(dmi.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    out.dmi_ok = true;
    std::vector<std::string> denied;
    auto read_attr = [&](const char *name) {
      std::string raw;
      int err = 0;
      if (!read_bounded(dmi + "/" + name, kMaxDmiValueBytes, raw, &err)) {
        if (err == EACCES || err == EPERM) denied.push_back(name);
        return std::string();
      }
      return clean_string(raw);
    };
    out.vendor = read_attr("sys_vendor");
    out.model = read_attr("product_name");
    out.uuid = to_upper(read_attr("product_uuid"));
    out.serial = read_attr("product_serial");
    out.chassis_serial = read_attr("chassis_serial");
    out.asset_tag = read_attr("chassis_asset_tag");
    const std::string ct = read_attr("chassis_type");
    if (!ct.empty() && ct.find_first_not_of("0123456789") == std::string::npos && ct.size() < 6) out.chassis_type = std::stoll(ct);
    if (!denied.empty()) {
      std::string list;
      for (const std::string &d : denied) list += (list.empty() ? "" : ", ") + d;
      add_error(out.gather_errors, "DMI: " + list + ": permission denied (root only)");
    }
  } else {
    add_error(out.gather_errors, "DMI: /sys/class/dmi/id not present");
  }

  const std::string tables = root + "/sys/firmware/dmi/tables";
  std::string table;
  int err = 0;
  if (read_bounded(tables + "/DMI", kMaxTableBytes, table, &err) && !table.empty()) {
    std::string ep_bytes;
    boost::optional<smbios_entry_point> ep;
    if (read_bounded(tables + "/smbios_entry_point", kMaxEntryPointBytes, ep_bytes, nullptr)) ep = parse_smbios_entry_point(ep_bytes);
    const smbios_inventory inv = parse_smbios_table(table, ep);
    if (inv.truncated) add_error(out.gather_errors, "SMBIOS table: malformed or truncated after " + str::xtos(inv.structures) + " structure(s)");
    if (inv.structures > 0) {
      out.smbios_ok = true;
      // The sysfs copies win where both exist; the table fills the gaps.
      if (out.vendor.empty()) out.vendor = inv.vendor;
      if (out.model.empty()) out.model = inv.model;
      if (out.uuid.empty()) out.uuid = inv.uuid;
      if (out.serial.empty()) out.serial = inv.serial;
      if (out.chassis_type == 0) out.chassis_type = inv.chassis_type;
      if (out.chassis_serial.empty()) out.chassis_serial = inv.chassis_serial;
      if (out.asset_tag.empty()) out.asset_tag = inv.asset_tag;
      // The memory inventory is all or nothing: a walk that stopped early
      // or found no populated module (a machine cannot run without one, so the
      // table does not describe it) leaves the counts unknown rather than
      // publishing a partial figure a threshold would then act on.
      if (!inv.truncated && !inv.modules.empty()) {
        out.module_details = inv.modules;
        out.recompute();
        out.slots = inv.have_arrays && inv.slots > 0 ? inv.slots : inv.device_records;
        if (out.memory) out.memory_source = "smbios";
      } else if (!inv.truncated && (inv.device_records > 0 || inv.have_arrays)) {
        add_error(out.gather_errors, "SMBIOS table: no populated memory module reported");
      }
    }
  } else {
    add_error(out.gather_errors, "SMBIOS table: " + (table.empty() && err == 0 ? std::string("empty") : errno_text(err)));
  }

  if (out.chassis_type > 0) out.chassis = chassis_type_to_string(out.chassis_type);

  if (!out.memory) {
    // Without the SMBIOS table, the memory the kernel manages in hotplug
    // blocks is the closest unprivileged answer to "installed memory".
    const std::string mem = root + "/sys/devices/system/memory";
    std::string block_raw;
    if (read_bounded(mem + "/block_size_bytes", 64, block_raw, nullptr)) {
      unsigned long long block = 0;
      try {
        block = std::stoull(clean_string(block_raw), nullptr, 16);
      } catch (const std::exception &) {
        block = 0;
      }
      if (block > 0) {
        long long online = 0;
        boost::system::error_code ec;
        boost::filesystem::directory_iterator it(mem, ec), end;
        std::size_t seen = 0;
        for (; !ec && it != end && seen < kMaxMemoryBlocks; it.increment(ec), ++seen) {
          const std::string name = it->path().filename().string();
          if (name.compare(0, 6, "memory") != 0 || name.size() == 6 || name.find_first_not_of("0123456789", 6) != std::string::npos) continue;
          std::string state;
          if (!read_bounded(mem + "/" + name + "/online", 8, state, nullptr)) continue;
          if (!state.empty() && state[0] == '1') online++;
        }
        if (online > 0) {
          out.memory = online * static_cast<long long>(block);
          out.memory_source = "sysfs";
        }
      }
    }
  }
  return out;
}

void check_from(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const hardware_info &info) {
  if (!info.dmi_ok && !info.smbios_ok) {
    // No identity source at all: an inventory row of empty strings would be a
    // misleading OK.
    return nscapi::protobuf::functions::set_response_bad(*response, no_dmi_message);
  }
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  // No default thresholds, as on Windows: a bare call is an inventory line.
  // Policy is pinned expectations, e.g. crit=serial != 'ABC123',
  // warn=modules < 8, crit=chassis like 'Laptop', warn=memory < 64G.
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${vendor} ${model} (${chassis}), serial=${serial}, ${modules} memory module(s), ${memory}", "hardware", "",
                           "");
  filter_helper.set_default_perf_config("extra(memory;modules)");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<hardware_info> record(new hardware_info(info));
  filter.match(record);

  filter_helper.post_process(filter);
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  try {
    check_from(request, response, gather_hardware());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read hardware inventory: " + std::string(e.what()));
  }
}

}  // namespace hardware_check
