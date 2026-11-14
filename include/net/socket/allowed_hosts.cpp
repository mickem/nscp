// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>
#include <mutex>
#include <net/socket/allowed_hosts.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <string>

using namespace boost::asio;
using namespace boost::asio::ip;

template <class addr>
addr calculate_mask(const std::size_t prefix_bits) {
  addr ret{};
  constexpr std::size_t byte_size = 8;
  constexpr std::size_t largest_byte = 0xff;
  const std::size_t index = prefix_bits / byte_size;
  const std::size_t reminder = prefix_bits % byte_size;

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
constexpr std::size_t max_prefix_v4 = 32;
constexpr std::size_t max_prefix_v6 = 128;

// Parse the `/N` suffix of an `allowed hosts` entry. `mask` is empty when the
// entry has none, which means a single host of whatever family the address
// turns out to be; `prefix` is then left disengaged. Anything but a `/`
// followed by decimal digits naming at most 128 bits is refused here; whether
// the length fits the address's own family is prefix_fits()'s job, since a
// host name's family is only known once it resolves.
//
// This used to take the first run of digits anywhere after the `/` and ignore
// the rest, so `/-1` read as `/1` (half the IPv4 space), `/abc24` as `/24`, a
// dotted `/255.255.255.0` as `/255`, `/33` as a `/32` and a bare `/` as a
// single host - a typo silently widened, or narrowed, the list.
bool parse_prefix_length(const std::string &mask, boost::optional<std::size_t> &prefix) {
  prefix = boost::none;
  if (mask.empty()) return true;
  if (mask.size() < 2 || mask[0] != '/') return false;
  std::size_t value = 0;
  for (std::size_t i = 1; i < mask.size(); i++) {
    const char c = mask[i];
    if (c < '0' || c > '9') return false;
    value = value * 10 + static_cast<std::size_t>(c - '0');
    // Checked per digit, so a long run of digits cannot overflow.
    if (value > max_prefix_v6) return false;
  }
  prefix = value;
  return true;
}

bool prefix_fits(const boost::optional<std::size_t> &prefix, const address &a) {
  return !prefix || prefix.value() <= (a.is_v4() ? max_prefix_v4 : max_prefix_v6);
}

std::string invalid_prefix_error(const std::string &record) {
  return "Invalid address: " + record + " (the part after / must be a prefix length: 0-32 for IPv4, 0-128 for IPv6)";
}

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
    // `/24` alone: a prefix of nothing. Reported, since a rejected connection
    // points the operator at the errors logged here.
    if (addr.empty()) {
      errors.push_back("Invalid address: " + record + " (no address before the /)");
      continue;
    }

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
    // Refuse a malformed suffix before resolving anything; whether its length
    // fits the address family is checked once the addresses are known.
    boost::optional<std::size_t> prefix;
    if (!parse_prefix_length(mask, prefix)) {
      errors.push_back(invalid_prefix_error(record));
      continue;
    }
    std::list<address> addresses;
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
      addresses.push_back(a);
    } else {
      try {
        for (const auto &entry : resolver.resolve(addr, "")) addresses.push_back(entry.endpoint().address());
      } catch (const std::exception &e) {
        errors.emplace_back("Failed to parse host " + record + ": " + utf8::utf8_from_native(e.what()));
        continue;
      }
    }
    add_record(record, prefix, addresses, errors);
  }
}

void socket_helpers::allowed_hosts_manager::add_record(const std::string &record, const boost::optional<std::size_t> &prefix,
                                                       const std::list<address> &addresses, std::list<std::string> &errors) {
  // A name with both A and AAAA records and a `/40` is a v4 typo far more
  // often than a deliberate v6 range; keeping the AAAA half would grant 2^88
  // addresses while the log says the entry was dropped.
  for (const address &a : addresses) {
    if (!prefix_fits(prefix, a)) {
      errors.push_back(invalid_prefix_error(record));
      return;
    }
  }
  for (const address &a : addresses) {
    if (a.is_v4()) {
      entries_v4.emplace_back(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(prefix.get_value_or(max_prefix_v4)));
    } else if (a.is_v6()) {
      entries_v6.emplace_back(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(prefix.get_value_or(max_prefix_v6)));
    } else {
      errors.push_back("Invalid address: " + record);
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

std::string socket_helpers::allowed_hosts_manager::rejection_message(const address &remote, const std::list<std::string> &errors) const {
  // A v4 client on a dual-stack listener arrives as ::ffff:a.b.c.d; the operator
  // configured, and will search the list for, a.b.c.d.
  std::string who = remote.to_string();
  if (remote.is_v6() && remote.to_v6().is_v4_mapped()) who = make_address_v4(v4_mapped, remote.to_v6()).to_string();

  // The list as configured rather than as resolved: that is what the operator
  // typed and has to edit. Capped, since a long list would bury the reason.
  constexpr std::size_t max_listed = 10;
  std::string listed;
  bool usable = false;
  std::size_t count = 0;
  std::string where;
  {
    std::lock_guard<std::mutex> lock(entries_mutex_);
    where = settings_path.empty() ? std::string("the module's server section") : settings_path;
    usable = !entries_v4.empty() || !entries_v6.empty();
    count = sources.size();
    std::size_t i = 0;
    for (const std::string &s : sources) {
      if (i++ == max_listed) break;
      str::format::append_list(listed, s, ", ");
    }
  }
  if (count > max_listed) listed += ", ... (" + str::xtos(count - max_listed) + " more)";

  // The prefix is unchanged from the old two-line form; log filters match on it.
  std::string ret = "Rejected connection from: " + who + ", ";
  if (count == 0) {
    ret += "allowed hosts is empty, which rejects every connection";
  } else if (!usable) {
    ret += "no entry in allowed hosts could be used (" + listed + ")";
  } else {
    ret += "not in allowed hosts (" + listed + ")";
  }
  ret += "; set `allowed hosts` in " + where + ", or in /settings/default if that section does not set it";
  if (count == 0) ret += " (`0.0.0.0/0,::/0` allows every address)";
  if (!errors.empty()) {
    ret += "; problems with the list: " + str::utils::joinEx(errors, "; ");
  } else if (count > 0 && !usable) {
    // With `cache allowed hosts` on (the default) the list is parsed once, when
    // the module loads, and that is where its errors were logged.
    ret += " (why each entry failed was logged when the module loaded)";
  }
  return ret;
}
