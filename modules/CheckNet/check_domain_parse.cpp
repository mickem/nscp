// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "check_domain.hpp"

namespace check_net {
namespace domain {
namespace {
std::string string_field(const boost::json::object &obj, const char *key) {
  const auto *v = obj.if_contains(key);
  return v && v->is_string() ? std::string(v->as_string()) : std::string();
}

void assign_date(boost::posix_time::ptime &target, const boost::posix_time::ptime &date) {
  if (!target.is_not_a_date_time() && target != date) throw std::runtime_error("Conflicting expiration dates");
  target = date;
}

registration choose_date(const boost::posix_time::ptime &registry, const boost::posix_time::ptime &registrar) {
  registration out;
  if (!registrar.is_not_a_date_time()) {
    out.expiration = registrar;
    out.expiration_type = "registrar";
  } else if (!registry.is_not_a_date_time()) {
    out.expiration = registry;
    out.expiration_type = "registry";
  }
  return out;
}
}  // namespace

std::string normalize_domain(std::string name) {
  if (!name.empty() && name.back() == '.') name.pop_back();
  if (name.empty() || name.size() > 253 || name.find('.') == std::string::npos)
    throw std::runtime_error("Specify a registered domain name (ASCII or punycode), not a URL or host path");
  boost::algorithm::to_lower(name);
  std::vector<std::string> labels;
  boost::split(labels, name, boost::is_any_of("."));
  bool last_has_letter = false;
  for (const auto &label : labels) {
    if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') throw std::runtime_error("Invalid domain name");
    last_has_letter = false;
    for (const unsigned char c : label) {
      const bool letter = c >= 'a' && c <= 'z';
      if (!letter && !(c >= '0' && c <= '9') && c != '-') throw std::runtime_error("Invalid domain name; use ASCII or punycode");
      last_has_letter = last_has_letter || letter;
    }
  }
  if (!last_has_letter) throw std::runtime_error("Invalid domain name (IP addresses are not registrations)");
  return name;
}

boost::posix_time::ptime parse_timestamp(const std::string &value) {
  // RDAP eventDate is RFC 3339. Require an explicit, known UTC offset;
  // accepting a date without one would make results depend on the agent TZ.
  static const std::regex pattern(R"(^(\d{4})-(\d{2})-(\d{2})[Tt](\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?([Zz]|[+-]\d{2}:\d{2})$)");
  if (value.size() > 64) throw std::runtime_error("Expiration timestamp is too long");
  std::smatch match;
  if (!std::regex_match(value, match, pattern)) throw std::runtime_error("Invalid expiration timestamp: " + value);
  const int hour = std::stoi(match[4]), minute = std::stoi(match[5]), second = std::stoi(match[6]);
  if (hour > 23 || minute > 59 || second > 59) throw std::runtime_error("Invalid expiration time");
  const auto date = boost::gregorian::date(std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3]));
  std::string fraction = match[7];
  fraction.resize(6, '0');
  auto time = boost::posix_time::ptime(date, boost::posix_time::hours(hour) + boost::posix_time::minutes(minute) + boost::posix_time::seconds(second) +
                                                 boost::posix_time::microseconds(std::stoi(fraction)));
  const std::string offset = match[8];
  if (offset.size() > 1) {
    const int hours = std::stoi(offset.substr(1, 2)), minutes = std::stoi(offset.substr(4, 2));
    if (hours > 23 || minutes > 59 || offset == "-00:00") throw std::runtime_error("Invalid or unknown expiration UTC offset");
    const auto delta = boost::posix_time::hours(hours) + boost::posix_time::minutes(minutes);
    time = offset[0] == '+' ? time - delta : time + delta;
  }
  return time;
}

long long days_remaining(const boost::posix_time::ptime &expiration, const boost::posix_time::ptime &now) {
  const auto ticks = (expiration - now).ticks();
  const auto per_day = boost::posix_time::time_duration::ticks_per_second() * 86400LL;
  // Floor, including negative fractions: a domain that expired a second ago
  // must not appear to have zero days remaining.
  return ticks / per_day - (ticks < 0 && ticks % per_day != 0 ? 1 : 0);
}

registration parse_rdap(const std::string &body, const std::string &name) {
  const auto root = boost::json::parse(body);
  if (!root.is_object()) throw std::runtime_error("RDAP response is not an object");
  const auto &obj = root.as_object();
  if (string_field(obj, "objectClassName") != "domain" || normalize_domain(string_field(obj, "ldhName")) != name)
    throw std::runtime_error("RDAP response does not describe the requested domain");
  boost::posix_time::ptime registry, registrar;
  const auto *events = obj.if_contains("events");
  if (events) {
    if (!events->is_array()) throw std::runtime_error("Invalid RDAP events");
    for (const auto &event : events->as_array()) {
      if (!event.is_object()) throw std::runtime_error("Invalid RDAP event");
      const auto &e = event.as_object();
      const auto action = string_field(e, "eventAction");
      if (action == "expiration") assign_date(registry, parse_timestamp(string_field(e, "eventDate")));
      if (action == "registrar expiration") assign_date(registrar, parse_timestamp(string_field(e, "eventDate")));
    }
  }
  auto out = choose_date(registry, registrar);
  // Domain-level related RDAP links point at the registrar's domain record.
  // Never walk entity events: those dates describe contacts, not the domain.
  const auto *links = obj.if_contains("links");
  if (links && links->is_array()) {
    for (const auto &link : links->as_array()) {
      if (!link.is_object()) continue;
      const auto &l = link.as_object();
      if (string_field(l, "rel") == "related" && string_field(l, "type") == "application/rdap+json") {
        out.registrar_url = string_field(l, "href");
        if (!out.registrar_url.empty()) break;
      }
    }
  }
  return out;
}

registration parse_whois(const std::string &body, const std::string &name) {
  boost::posix_time::ptime registry, registrar, generic;
  std::istringstream input(body);
  std::string line;
  while (std::getline(input, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    auto key = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(line.substr(0, colon)));
    auto value = boost::algorithm::trim_copy(line.substr(colon + 1));
    if (key == "domain name" || key == "domain") {
      if (normalize_domain(value) != name) throw std::runtime_error("WHOIS response describes a different domain");
      continue;
    }
    boost::posix_time::ptime *target = nullptr;
    if (key == "registry expiry date") target = &registry;
    if (key == "registrar registration expiration date") target = &registrar;
    if (key == "expiry date" || key == "expiration date" || key == "paid-till" || key == "expires" || key == "expiration time") target = &generic;
    if (!target) continue;
    // A deliberately limited parser: these documented ISO date forms are
    // unambiguous. Do not guess locale-dependent dates or scrape other fields.
    if (value.size() == 10) value += "T00:00:00Z";
    assign_date(*target, parse_timestamp(value));
  }
  auto out = choose_date(registry, registrar);
  if (out.expiration.is_not_a_date_time() && !generic.is_not_a_date_time()) {
    out.expiration = generic;
    out.expiration_type = "whois";
  }
  if (out.expiration.is_not_a_date_time()) throw std::runtime_error("No supported WHOIS expiration date");
  return out;
}
}  // namespace domain
}  // namespace check_net
