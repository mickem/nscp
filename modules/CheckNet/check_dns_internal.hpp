// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// Minimal DNS-over-UDP wire helpers so check_dns can query arbitrary record
// types (MX/TXT/CNAME/NS/SOA/PTR/A/AAAA) against a chosen server

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace check_net {
namespace check_dns_internal {

enum dns_type { DNS_A = 1, DNS_NS = 2, DNS_CNAME = 5, DNS_SOA = 6, DNS_PTR = 12, DNS_MX = 15, DNS_TXT = 16, DNS_AAAA = 28 };

// Map a record-type name (case-insensitive) to its numeric type, or -1.
inline int type_from_string(std::string s) {
  for (char &c : s) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
  if (s == "A") return DNS_A;
  if (s == "AAAA") return DNS_AAAA;
  if (s == "NS") return DNS_NS;
  if (s == "CNAME") return DNS_CNAME;
  if (s == "SOA") return DNS_SOA;
  if (s == "PTR") return DNS_PTR;
  if (s == "MX") return DNS_MX;
  if (s == "TXT") return DNS_TXT;
  return -1;
}

inline std::string type_to_string(int t) {
  switch (t) {
    case DNS_A:
      return "A";
    case DNS_AAAA:
      return "AAAA";
    case DNS_NS:
      return "NS";
    case DNS_CNAME:
      return "CNAME";
    case DNS_SOA:
      return "SOA";
    case DNS_PTR:
      return "PTR";
    case DNS_MX:
      return "MX";
    case DNS_TXT:
      return "TXT";
    default:
      return "?";
  }
}

// Encode a dotted name into DNS label form (len-prefixed labels + terminating 0).
inline std::string encode_qname(const std::string &name) {
  std::string out;
  std::size_t start = 0;
  while (start < name.size()) {
    std::size_t dot = name.find('.', start);
    if (dot == std::string::npos) dot = name.size();
    const std::size_t len = dot - start;
    if (len > 0 && len <= 63) {
      out.push_back(static_cast<char>(len));
      out.append(name, start, len);
    }
    start = dot + 1;
  }
  out.push_back(0);
  return out;
}

// Build a DNS query packet.
inline std::string build_query(std::uint16_t id, const std::string &qname, int qtype, bool recursion_desired) {
  std::string p;
  const std::uint16_t flags = recursion_desired ? 0x0100 : 0x0000;
  auto put16 = [&](std::uint16_t v) {
    p.push_back(static_cast<char>((v >> 8) & 0xff));
    p.push_back(static_cast<char>(v & 0xff));
  };
  put16(id);
  put16(flags);
  put16(1);  // qdcount
  put16(0);  // ancount
  put16(0);  // nscount
  put16(1);  // arcount (one EDNS0 OPT record, below)
  p += encode_qname(qname);
  put16(static_cast<std::uint16_t>(qtype));
  put16(1);  // qclass = IN
  // EDNS0 OPT pseudo-record advertising a 4096-byte UDP payload so servers can
  // return larger answers (e.g. multi-record TXT) without truncation.
  p.push_back(0);   // root name
  put16(41);        // type = OPT
  put16(4096);      // class = requestor's UDP payload size
  put16(0);         // extended-rcode + version (high 16 bits of TTL)
  put16(0);         // flags (low 16 bits of TTL)
  put16(0);         // rdlength
  return p;
}

struct dns_result {
  bool ok;                           // parse succeeded (well-formed packet)
  int rcode;                         // 0 = NOERROR, 3 = NXDOMAIN, ...
  std::vector<std::string> records;  // formatted answer records of the queried type
  dns_result() : ok(false), rcode(-1) {}
};

namespace detail {
inline std::uint16_t rd16(const std::string &p, std::size_t off) {
  return static_cast<std::uint16_t>((static_cast<unsigned char>(p[off]) << 8) | static_cast<unsigned char>(p[off + 1]));
}

// Read a (possibly compressed) domain name starting at `off`. `next` receives
// the offset just past the name *in the record stream* (i.e. past the first
// pointer, not through it). Returns false on malformed input.
inline bool read_name(const std::string &p, std::size_t off, std::string &out, std::size_t &next) {
  out.clear();
  bool jumped = false;
  next = off;
  int safety = 0;
  while (off < p.size()) {
    if (++safety > 128) return false;  // loop guard
    const unsigned char len = static_cast<unsigned char>(p[off]);
    if ((len & 0xc0) == 0xc0) {  // compression pointer
      if (off + 1 >= p.size()) return false;
      const std::size_t ptr = ((len & 0x3f) << 8) | static_cast<unsigned char>(p[off + 1]);
      if (!jumped) next = off + 2;
      jumped = true;
      if (ptr >= p.size()) return false;
      off = ptr;
      continue;
    }
    if (len == 0) {
      if (!jumped) next = off + 1;
      return true;
    }
    if (off + 1 + len > p.size()) return false;
    if (!out.empty()) out.push_back('.');
    out.append(p, off + 1, len);
    off += 1 + len;
  }
  return false;
}

inline std::string ipv4(const std::string &p, std::size_t off) {
  std::ostringstream os;
  os << static_cast<int>(static_cast<unsigned char>(p[off])) << '.' << static_cast<int>(static_cast<unsigned char>(p[off + 1])) << '.'
     << static_cast<int>(static_cast<unsigned char>(p[off + 2])) << '.' << static_cast<int>(static_cast<unsigned char>(p[off + 3]));
  return os.str();
}

inline std::string ipv6(const std::string &p, std::size_t off) {
  std::ostringstream os;
  os << std::hex;
  for (int i = 0; i < 8; ++i) {
    if (i) os << ':';
    os << ((static_cast<unsigned char>(p[off + i * 2]) << 8) | static_cast<unsigned char>(p[off + i * 2 + 1]));
  }
  return os.str();
}
}  // namespace detail

// Parse a DNS response packet, extracting the answer records matching `qtype`.
inline dns_result parse_response(const std::string &p, int qtype) {
  using namespace detail;
  dns_result res;
  if (p.size() < 12) return res;
  res.rcode = static_cast<unsigned char>(p[3]) & 0x0f;
  const std::uint16_t qdcount = rd16(p, 4);
  const std::uint16_t ancount = rd16(p, 6);

  std::size_t off = 12;
  // Skip the question section.
  for (std::uint16_t i = 0; i < qdcount; ++i) {
    std::string name;
    std::size_t next = 0;
    if (!read_name(p, off, name, next)) return res;
    off = next + 4;  // qtype(2) + qclass(2)
    if (off > p.size()) return res;
  }

  for (std::uint16_t i = 0; i < ancount; ++i) {
    std::string name;
    std::size_t next = 0;
    if (!read_name(p, off, name, next)) return res;
    off = next;
    if (off + 10 > p.size()) return res;
    const std::uint16_t rtype = rd16(p, off);
    const std::uint16_t rdlen = rd16(p, off + 8);
    const std::size_t rdata = off + 10;
    if (rdata + rdlen > p.size()) return res;

    if (rtype == qtype) {
      switch (qtype) {
        case DNS_A:
          if (rdlen == 4) res.records.push_back(ipv4(p, rdata));
          break;
        case DNS_AAAA:
          if (rdlen == 16) res.records.push_back(ipv6(p, rdata));
          break;
        case DNS_CNAME:
        case DNS_NS:
        case DNS_PTR: {
          std::string target;
          std::size_t dummy = 0;
          if (read_name(p, rdata, target, dummy)) res.records.push_back(target);
          break;
        }
        case DNS_MX: {
          if (rdlen >= 3) {
            const std::uint16_t pref = rd16(p, rdata);
            std::string exch;
            std::size_t dummy = 0;
            if (read_name(p, rdata + 2, exch, dummy)) res.records.push_back(std::to_string(pref) + " " + exch);
          }
          break;
        }
        case DNS_SOA: {
          std::string mname;
          std::size_t dummy = 0;
          if (read_name(p, rdata, mname, dummy)) res.records.push_back(mname);
          break;
        }
        case DNS_TXT: {
          std::string txt;
          std::size_t q = rdata;
          const std::size_t end = rdata + rdlen;
          while (q < end) {
            const unsigned char slen = static_cast<unsigned char>(p[q]);
            if (q + 1 + slen > end) break;
            txt.append(p, q + 1, slen);
            q += 1 + slen;
          }
          res.records.push_back(txt);
          break;
        }
        default:
          break;
      }
    }
    off = rdata + rdlen;
  }
  res.ok = true;
  return res;
}

}  // namespace check_dns_internal
}  // namespace check_net
