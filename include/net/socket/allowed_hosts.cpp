// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <mutex>
#include <net/socket/allowed_hosts.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;

std::size_t extract_mask(const std::string &mask, std::size_t mask_length) {
  if (!mask.empty()) {
    const std::string::size_type start_pos_number = mask.find_first_of("0123456789");
    if (start_pos_number != std::string::npos) {
      const std::string::size_type end_pos_number = mask.find_first_not_of("0123456789", start_pos_number);
      if (end_pos_number != std::string::npos) {
        mask_length = str::stox<std::size_t>(mask.substr(start_pos_number, end_pos_number - start_pos_number));
      } else {
        mask_length = str::stox<std::size_t>(mask.substr(start_pos_number));
      }
    }
  }
  return static_cast<unsigned int>(mask_length);
}

template <class addr>
addr calculate_mask(const std::string &mask_as_string) {
  addr ret{};
  constexpr std::size_t byte_size = 8;
  constexpr std::size_t largest_byte = 0xff;
  const std::size_t mask = extract_mask(mask_as_string, byte_size * ret.size());
  const std::size_t index = mask / byte_size;
  const std::size_t reminder = mask % byte_size;

  const std::size_t value = largest_byte - (largest_byte >> reminder);

  for (std::size_t i = 0; i < ret.size(); i++) {
    if (i < index)
      ret[i] = largest_byte;
    else if (i == index)
      ret[i] = static_cast<unsigned char>(value);
    else
      ret[i] = 0;
  }
  return ret;
}

namespace {
// Expand the `*` range syntax the `allowed hosts` help text and the docs have
// advertised for years: `192.168.1.*` means `192.168.1.0/24`, `192.168.*`
// means `192.168.0.0/16`, `10.*` means `10.0.0.0/8` and a bare `*` means
// `0.0.0.0/0`.
//
// Nothing implemented it. `192.168.1.*` starts with a digit, so it took the
// numeric branch and make_address threw - outside the try that wraps only the
// DNS branch, so the exception escaped refresh() and, with `cache allowed
// hosts = true`, escaped loadModuleEx: the plugin manager dropped the module
// and the listener never started. Both failure modes are closed rather than
// open, but a documented syntax that silently stops a listener is a footgun.
//
// IPv4 dotted quads only. A `*` in an IPv6 address, or in the middle of an
// IPv4 one (`192.*.1.1`), is not a range this can express and is reported as
// invalid instead of being guessed at.
//
// Returns false when the wildcard form is malformed; `out_addr` / `out_mask`
// are only meaningful on true.
bool expand_wildcard_v4(const std::string &addr, std::string &out_addr, std::string &out_mask) {
  const std::list<std::string> parts = str::utils::split_lst(addr, std::string("."));
  if (parts.empty() || parts.size() > 4) return false;
  std::size_t significant = 0;
  bool seen_star = false;
  for (const std::string &part : parts) {
    if (part == "*") {
      seen_star = true;
      continue;
    }
    // Every part before the first `*` has to be an octet, and nothing may
    // follow it: `192.*.1.1` is not a prefix.
    if (seen_star) return false;
    if (part.empty() || part.size() > 3 || part.find_first_not_of("0123456789") != std::string::npos) return false;
    if (str::stox<unsigned int>(part) > 255) return false;
    significant++;
  }
  if (!seen_star) return false;
  std::string result;
  std::size_t emitted = 0;
  for (const std::string &part : parts) {
    if (emitted >= significant) break;
    if (!result.empty()) result += ".";
    result += part;
    emitted++;
  }
  // Pad the wildcarded octets with zeroes so the result is a real address.
  for (std::size_t i = significant; i < 4; i++) {
    if (!result.empty()) result += ".";
    result += "0";
  }
  out_addr = result;
  out_mask = "/" + str::xtos(significant * 8);
  return true;
}
}  // namespace

void socket_helpers::allowed_hosts_manager::refresh(std::list<std::string> &errors) {
  io_context io_service;
  tcp::resolver resolver(io_service);
  // Rebuilt in place: hold the lock for the whole rebuild so no accepting
  // thread walks a half-built list.
  std::lock_guard<std::mutex> lock(entries_mutex_);
  entries_v4.clear();
  entries_v6.clear();
  for (const std::string &record : sources) {
    std::string::size_type pos = record.find('/');
    std::string addr, mask;
    if (pos == std::string::npos) {
      addr = record;
      mask = "";
    } else {
      addr = record.substr(0, pos);
      mask = record.substr(pos);
    }
    if (addr.empty()) continue;

    // Numeric IPv4 addresses start with a digit; numeric IPv6 addresses
    // contain a `:` (potentially as the very first character, e.g. `::/0`).
    // Anything else is treated as a hostname and resolved via DNS.
    const bool is_wildcard = addr.find('*') != std::string::npos;
    const bool is_numeric = std::isdigit(static_cast<unsigned char>(addr[0])) || addr.find(':') != std::string::npos;
    if (is_wildcard) {
      // A range and an explicit prefix length say the same thing twice, and
      // disagree as often as not. Refuse rather than pick one.
      if (!mask.empty()) {
        errors.push_back("Invalid address: " + record + " (a * range cannot also carry a /mask)");
        continue;
      }
      std::string expanded_addr, expanded_mask;
      if (!expand_wildcard_v4(addr, expanded_addr, expanded_mask)) {
        errors.push_back("Invalid address: " + record + " (a * may only replace whole trailing octets of an IPv4 address, as in 192.168.1.*)");
        continue;
      }
      addr = expanded_addr;
      mask = expanded_mask;
    }
    if (is_numeric || is_wildcard) {
      // make_address throws, and this is not inside the try that wraps the DNS
      // branch: an unparseable numeric address used to take the exception all
      // the way out of refresh(), which stops the module loading when
      // `cache allowed hosts = true` and fires on every accept when it is off.
      // A bad entry is a configuration error to report, not a dead listener.
      boost::system::error_code ec;
      address a = make_address(addr, ec);
      if (ec) {
        errors.push_back("Failed to parse address " + record + ": " + utf8::utf8_from_native(ec.message()));
        continue;
      }
      if (a.is_v4()) {
        entries_v4.emplace_back(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask));
      } else if (a.is_v6()) {
        entries_v6.emplace_back(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask));
      } else {
        errors.push_back("Invalid address: " + record);
      }
    } else {
      try {
        auto endpoints = resolver.resolve(addr, "");
        for (const auto &entry : endpoints) {
          address a = entry.endpoint().address();
          if (a.is_v4()) {
            entries_v4.emplace_back(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask));
          } else if (a.is_v6()) {
            entries_v6.emplace_back(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask));
          } else {
            errors.emplace_back("Invalid address: " + record);
          }
        }
      } catch (const std::exception &e) {
        errors.emplace_back("Failed to parse host " + record + ": " + utf8::utf8_from_native(e.what()));
      }
    }
  }
}

void socket_helpers::allowed_hosts_manager::set_source(const std::string &source) {
  std::lock_guard<std::mutex> lock(entries_mutex_);
  sources.clear();
  for (std::string s : str::utils::split_lst(source, std::string(","))) {
    boost::trim(s);
    if (!s.empty()) sources.push_back(s);
  }
}

std::string socket_helpers::allowed_hosts_manager::to_string() const {
  std::lock_guard<std::mutex> lock(entries_mutex_);
  std::string ret;
  for (const host_record_v4 &r : entries_v4) {
    ip::address_v4 a(r.addr);
    ip::address_v4 m(r.mask);
    std::string s = a.to_string() + "(" + m.to_string() + ")";
    str::format::append_list(ret, s);
  }
  for (const host_record_v6 &r : entries_v6) {
    ip::address_v6 a(r.addr);
    ip::address_v6 m(r.mask);
    std::string s = a.to_string() + "(" + m.to_string() + ")";
    str::format::append_list(ret, s);
  }
  return ret;
}
