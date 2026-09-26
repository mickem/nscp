// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <cctype>
#include <facts/network_facts.hpp>
#include <nscapi/nscapi_facts_helper.hpp>

namespace network_facts {

const char *const set_network = "network";
const char *const key_interfaces = "interfaces";
const char *const id_interfaces = "network.interfaces";

void publish(const std::vector<interface_record> &interfaces, const std::time_t taken_at, nscapi::facts::response &out) {
  // Written even when empty: "enabled, collected, nothing there" is an
  // answer, and an absent list would read as "not collected".
  nscapi::facts::record_list list = out.set(set_network).list(key_interfaces);
  for (const interface_record &nic : interfaces) {
    nscapi::facts::section record = list.record(nic.id);
    record.value("display_name", nic.display_name).value("mac", nic.mac).value("status", nic.status);
    // Zero is "not known" (a virtual adapter, a link that is down), and the
    // builder writes a number as it is given, so the guard is here.
    if (nic.speed_bps > 0) record.value("speed_bps", nic.speed_bps);
    if (!nic.addresses.empty()) record.strings("addresses", nic.addresses);
  }
  out.gathered(set_network, taken_at);
}

namespace {
const char *const hex_digits = "0123456789abcdef";
}  // namespace

std::string mac_from_bytes(const unsigned char *bytes, const std::size_t length) {
  if (bytes == nullptr || length != 6) return "";
  bool all_zero = true;
  std::string out;
  for (std::size_t i = 0; i < length; ++i) {
    if (bytes[i] != 0) all_zero = false;
    if (i > 0) out += ':';
    out += hex_digits[(bytes[i] >> 4) & 0xf];
    out += hex_digits[bytes[i] & 0xf];
  }
  return all_zero ? "" : out;
}

std::string normalize_mac(const std::string &raw) {
  unsigned char bytes[6];
  std::size_t count = 0;
  std::string::size_type i = 0;
  while (i < raw.size()) {
    if (count == 6) return "";
    if (i + 1 >= raw.size() || !std::isxdigit(static_cast<unsigned char>(raw[i])) || !std::isxdigit(static_cast<unsigned char>(raw[i + 1]))) return "";
    bytes[count++] = static_cast<unsigned char>(std::stoi(raw.substr(i, 2), nullptr, 16));
    i += 2;
    if (i == raw.size()) break;
    if (raw[i] != ':' && raw[i] != '-') return "";
    ++i;
    // A trailing separator is not an octet.
    if (i == raw.size()) return "";
  }
  if (count != 6) return "";
  return mac_from_bytes(bytes, count);
}

std::string normalize_address(const std::string &raw) {
  const std::string::size_type zone = raw.find('%');
  return zone == std::string::npos ? raw : raw.substr(0, zone);
}

std::string status_from_oper_status(const int oper_status) {
  // IF_OPER_STATUS (ifdef.h) numbers ifOperStatus exactly as RFC 2863 does.
  switch (oper_status) {
    case 1:
      return "up";
    case 2:
      return "down";
    case 3:
      return "testing";
    case 5:
      return "dormant";
    case 6:
      return "notpresent";
    case 7:
      return "lowerlayerdown";
    default:
      return "unknown";
  }
}

}  // namespace network_facts
