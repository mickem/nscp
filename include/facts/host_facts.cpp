// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <cctype>
#include <facts/host_facts.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

namespace host_facts {

const char *const tag_os_name = "os_name";
const char *const tag_os_version = "os_version";
const char *const tag_os_family = "os_family";
const char *const tag_arch = "arch";
const char *const tag_virtualization = "virtualization";

const char *const set_os = "os";
const char *const set_hardware = "hardware";

namespace {
std::string to_lower(const std::string &s) {
  std::string result = s;
  for (std::string::size_type i = 0; i < result.size(); ++i) {
    result[i] = static_cast<char>(::tolower(static_cast<unsigned char>(result[i])));
  }
  return result;
}

bool contains(const std::string &haystack_lower, const char *needle_lower) { return haystack_lower.find(needle_lower) != std::string::npos; }

bool starts_with(const std::string &value, const char *prefix) { return value.compare(0, std::string(prefix).size(), prefix) == 0; }
}  // namespace

std::string virtualization_from_hypervisor_id(const std::string &id) {
  // The id is 12 bytes and vendors pad it with NULs or spaces ("KVMKVMKVM\0\0\0",
  // "prl hyperv "), so cut at the first NUL, trim, and match on a prefix of
  // the lower-cased remainder.
  std::string value = to_lower(id);
  const std::string::size_type nul = value.find('\0');
  if (nul != std::string::npos) value = value.substr(0, nul);
  const std::string::size_type end = value.find_last_not_of(" \t\r\n");
  value = end == std::string::npos ? "" : value.substr(0, end + 1);
  if (value.empty()) return "";

  if (starts_with(value, "vmwarevmware")) return "vmware";
  if (starts_with(value, "microsoft h")) return "hyperv";  // "Microsoft Hv"
  if (starts_with(value, "kvmkvm") || starts_with(value, "linuxkvm")) return "kvm";
  if (starts_with(value, "xenvmm")) return "xen";
  if (starts_with(value, "vboxvb")) return "virtualbox";
  if (starts_with(value, "tcgtcg")) return "qemu";
  if (starts_with(value, "prl hyperv")) return "parallels";
  if (starts_with(value, "bhyve")) return "bhyve";
  if (starts_with(value, "acrnacrn")) return "acrn";
  return "";
}

std::string virtualization_from_dmi(const std::string &sys_vendor, const std::string &product_name) {
  const std::string vendor = to_lower(sys_vendor);
  const std::string product = to_lower(product_name);
  if (vendor.empty() && product.empty()) return "";

  if (contains(vendor, "vmware") || contains(product, "vmware")) return "vmware";
  if (contains(vendor, "innotek") || contains(product, "virtualbox")) return "virtualbox";
  if (contains(vendor, "parallels") || contains(product, "parallels")) return "parallels";
  if (contains(vendor, "xen") || contains(product, "hvm domu")) return "xen";
  // Hyper-V presents as Microsoft Corporation / Virtual Machine. Plain
  // "Microsoft Corporation" is also what a Surface reports, so the product
  // has to say it too.
  if (contains(vendor, "microsoft") && contains(product, "virtual machine")) return "hyperv";
  // QEMU with KVM acceleration names KVM in the product; bare QEMU (TCG) does
  // not. Both spell the vendor "QEMU".
  if (contains(product, "kvm")) return "kvm";
  if (contains(vendor, "qemu") || contains(product, "qemu") || contains(product, "standard pc")) return "qemu";
  if (contains(vendor, "bochs") || contains(product, "bochs")) return "qemu";
  if (contains(vendor, "amazon ec2")) return "kvm";  // Nitro is KVM-based.
  if (contains(vendor, "google") && contains(product, "google compute engine")) return "kvm";
  if (contains(vendor, "digitalocean") || contains(vendor, "openstack") || contains(product, "openstack")) return "kvm";
  if (contains(vendor, "alibaba") || contains(vendor, "tencent")) return "kvm";
  if (contains(vendor, "nutanix") || contains(product, "ahv")) return "kvm";
  if (contains(vendor, "oracle") && contains(product, "virtualbox")) return "virtualbox";
  if (contains(product, "bhyve")) return "bhyve";
  // A guest of Apple's Virtualization framework (macOS VMs, the arm64 macOS
  // CI runners) reports the model VirtualMac<n>,<m>; the framework has no name
  // of its own in the vocabulary, so it is an unnamed "virtual".
  if (contains(vendor, "apple") && contains(product, "virtualmac")) return "virtual";
  return "";
}

bool dmi_names_physical_hardware(const std::string &sys_vendor, const std::string &product_name) {
  // Both fields have to say something: a guest that leaves one blank is not
  // evidence of hardware, and the gather has already stripped the SMBIOS
  // placeholders ("To Be Filled By O.E.M.") that would otherwise qualify.
  if (sys_vendor.empty() || product_name.empty()) return false;
  return virtualization_from_dmi(sys_vendor, product_name).empty();
}

long long memory_gb_from_bytes(unsigned long long bytes) {
  if (bytes == 0) return 0;
  const unsigned long long gb = 1024ULL * 1024ULL * 1024ULL;
  // Round to nearest, then floor at 1: firmware reserves a slice of physical
  // memory, so a 64 GB machine reports a little under 64 GiB and truncation
  // would publish 63.
  const unsigned long long rounded = (bytes + gb / 2) / gb;
  return rounded == 0 ? 1 : static_cast<long long>(rounded);
}

std::string domain_from_fqdn(const std::string &fqdn) {
  std::string value = fqdn;
  // A canonical name may carry the root dot; it is not part of the domain.
  while (!value.empty() && value[value.size() - 1] == '.') value.erase(value.size() - 1);
  const std::string::size_type dot = value.find('.');
  if (dot == std::string::npos) return "";
  return value.substr(dot + 1);
}

void publish_tags(const nscapi::core_wrapper *core, const facts &f) {
  if (core == nullptr) return;
  // An empty value removes the tag (see tag_repository::set), so passing the
  // undetermined facts through unchanged is what sheds a stale tag.
  core->set_tag(tag_os_name, f.os_name);
  core->set_tag(tag_os_version, f.os_version);
  core->set_tag(tag_os_family, f.os_family);
  core->set_tag(tag_arch, f.arch);
  core->set_tag(tag_virtualization, f.virtualization);
}

bool should_regather(const std::string &reason, const bool have_snapshot) {
  if (!have_snapshot) return true;
  // An unknown reason reads as scheduled: a core that grows a new one should
  // not silently turn a cached producer into a collecting one.
  return reason == "startup" || reason == "manual";
}

snapshot snapshot_cache::get(const std::string &reason, const std::function<facts()> &gather) {
  bool have_snapshot = false;
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    have_snapshot = held_.valid();
    if (!should_regather(reason, have_snapshot)) return held_;
  }

  // Outside the lock: a future producer's gather may be slow, and a reader
  // waiting on it would be waiting for data it is about to be handed anyway.
  snapshot fresh;
  fresh.values = gather();
  fresh.taken_at = std::time(nullptr);

  boost::unique_lock<boost::mutex> lock(mutex_);
  held_ = fresh;
  return held_;
}

void publish_facts(const snapshot &snap, const bool want_os, const bool want_hardware, nscapi::facts::response &out) {
  const facts &f = snap.values;
  if (want_os) {
    // `family`, `name` and `version` rather than the tags' `os_` prefix: the
    // set they sit in already says os, and a key that repeats its section
    // reads badly in a document (`os.os_name`).
    nscapi::facts::section os = out.set(set_os);
    os.value("family", f.os_family).value("name", f.os_name).value("version", f.os_version);
    os.value("arch", f.arch).value("virtualization", f.virtualization);
    // The DNS domain is inventory rather than a group: an operator selects on
    // os_family, but asks *which* domain a given host ended up in.
    os.value("domain", f.domain);
    out.gathered(set_os, snap.taken_at);
  }
  if (want_hardware) {
    nscapi::facts::section hardware = out.set(set_hardware);
    hardware.value("manufacturer", f.manufacturer).value("model", f.model);
    // Zero means "not determined" for both, and the builder writes a number
    // as it is given - so the guard has to be here, unlike for the strings.
    if (f.cpu_cores > 0) hardware.value("cpu_cores", f.cpu_cores);
    if (f.memory_gb > 0) hardware.value("memory_gb", f.memory_gb);
    out.gathered(set_hardware, snap.taken_at);
  }
}

}  // namespace host_facts
