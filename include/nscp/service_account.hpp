// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <simpleini/simpleini.h>

#include <cstddef>
#include <cstdint>
#include <str/utf8.hpp>
#include <string>
#include <vector>

#include "path_defaults.hpp"

// Which account the Windows service runs as, and what follows from it.
//
// The default is LocalSystem, as it always was. The opt-in alternative is the
// service's own *virtual account*, `NT SERVICE\<service name>`: Windows
// creates it, it has no password, and no other service shares it. Running as
// that account is what makes a compromised agent something less than SYSTEM.
//
// The choice is recorded in boot.ini (`[service] account = virtual` plus
// `[service] name`), next to the layout, and for the same reason: every
// process that touches the shared folder has to agree about who is supposed to
// have access to it. boot.ini sits in the install folder, which only
// administrators can write, so it is a statement an attacker cannot make. An
// elevated `nscp` that did not know about the service account would see its
// grant on %ProgramData%\NSClient++ as an intruder and remove it; the service
// would then be unable to read its own configuration.
//
// Everything here is platform-neutral and dependency-free on purpose: the
// rules (what is refused, which SID is expected, what order things happen in)
// are what a unit test can cover on any machine, while the calls that carry
// them out need a Windows host with administrator rights.
namespace nscp {
namespace service_account {

enum class kind {
  // LocalSystem. The default, and what an absent key means.
  system,
  // NT SERVICE\<service name>.
  virtual_account,
};

// True when `value` names an account kind we understand. Empty is "not set".
inline bool is_known(const std::string &value) { return value.empty() || value == "system" || value == "virtual"; }

// Unknown text is `system`: a mistyped key must not make an elevated process
// start granting access to an account nobody chose.
inline kind parse(const std::string &value) { return value == "virtual" ? kind::virtual_account : kind::system; }

inline const char *name(const kind value) { return value == kind::virtual_account ? "virtual" : "system"; }

// Who the service is: the account kind and the name it is registered under.
// The name matters because the virtual account, and its SID, are derived from
// it - two services installed under different names are different accounts.
struct identity {
  kind account = kind::system;
  std::string service_name;
};

// The service names this feature accepts: printable ASCII without the path
// separators the service control manager itself refuses. Windows allows more,
// but the SID is a hash of the *upper-cased* name, and upper-casing anything
// outside ASCII means reproducing the kernel's case table. Refusing is better
// than computing a SID that is subtly not the service's.
inline bool is_supported_service_name(const std::string &service_name) {
  if (service_name.empty() || service_name.size() > 256) return false;
  for (const char c : service_name) {
    if (c <= 0x20 || c >= 0x7f || c == '/' || c == '\\') return false;
  }
  return true;
}

namespace detail {

inline std::uint32_t rotate_left(const std::uint32_t value, const unsigned bits) { return (value << bits) | (value >> (32 - bits)); }

// SHA-1, only because that is how Windows defines a service SID. Not used for
// anything that needs a collision-resistant hash.
inline void sha1(const std::vector<unsigned char> &input, unsigned char digest[20]) {
  std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

  std::vector<unsigned char> message(input);
  const std::uint64_t bit_length = static_cast<std::uint64_t>(input.size()) * 8;
  message.push_back(0x80);
  while (message.size() % 64 != 56) message.push_back(0);
  for (int shift = 56; shift >= 0; shift -= 8) message.push_back(static_cast<unsigned char>((bit_length >> shift) & 0xff));

  for (std::size_t offset = 0; offset < message.size(); offset += 64) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      const unsigned char *p = &message[offset + static_cast<std::size_t>(i) * 4];
      w[i] = (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) | (static_cast<std::uint32_t>(p[2]) << 8) |
             static_cast<std::uint32_t>(p[3]);
    }
    for (int i = 16; i < 80; i++) w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      std::uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const std::uint32_t next = rotate_left(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rotate_left(b, 30);
      b = a;
      a = next;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  for (int i = 0; i < 5; i++) {
    digest[i * 4] = static_cast<unsigned char>((h[i] >> 24) & 0xff);
    digest[i * 4 + 1] = static_cast<unsigned char>((h[i] >> 16) & 0xff);
    digest[i * 4 + 2] = static_cast<unsigned char>((h[i] >> 8) & 0xff);
    digest[i * 4 + 3] = static_cast<unsigned char>(h[i] & 0xff);
  }
}

}  // namespace detail

// The SID of `NT SERVICE\<service_name>`, in its `S-1-5-80-...` string form,
// or an empty string for a name is_supported_service_name() refuses.
//
// Computed rather than looked up: the five sub-authorities are the SHA-1 of
// the upper-cased service name in UTF-16LE, read as little-endian 32-bit
// words. That is the same answer `sc showsid` gives, it needs no access to
// anything, and it works before the service is installed - which is when the
// installer has to put the SID in an ACL.
inline std::string service_sid(const std::string &service_name) {
  if (!is_supported_service_name(service_name)) return std::string();
  std::vector<unsigned char> utf16;
  utf16.reserve(service_name.size() * 2);
  for (char c : service_name) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    utf16.push_back(static_cast<unsigned char>(c));
    utf16.push_back(0);
  }
  unsigned char digest[20];
  detail::sha1(utf16, digest);
  std::string sid = "S-1-5-80";
  for (int i = 0; i < 5; i++) {
    const std::uint32_t word = static_cast<std::uint32_t>(digest[i * 4]) | (static_cast<std::uint32_t>(digest[i * 4 + 1]) << 8) |
                               (static_cast<std::uint32_t>(digest[i * 4 + 2]) << 16) | (static_cast<std::uint32_t>(digest[i * 4 + 3]) << 24);
    sid += "-" + std::to_string(word);
  }
  return sid;
}

// The account name the service control manager is given.
inline std::string account_name(const identity &who) {
  if (who.account == kind::virtual_account) return "NT SERVICE\\" + who.service_name;
  return "LocalSystem";
}

// The SIDs, beyond SYSTEM and Administrators, that are supposed to have access
// to the shared folder. Empty for LocalSystem, and empty for a virtual account
// whose service name we cannot turn into a SID - the caller then behaves as it
// always did, which is the safe direction.
inline std::vector<std::string> acl_principals(const identity &who) {
  std::vector<std::string> sids;
  if (who.account != kind::virtual_account) return sids;
  const std::string sid = service_sid(who.service_name);
  if (!sid.empty()) sids.push_back(sid);
  return sids;
}

// Why `wanted` cannot be used on this installation, or an empty string when it
// can. The message is the whole answer the operator gets, so it says what to
// do next.
//
// The virtual account needs the modern layout. On the legacy one the writable
// state lives in the install folder: giving the service account write access
// there means it can replace nscp.exe, a module or a script, and wait for an
// administrator to run it. Nothing is switched implicitly - moving the
// configuration is a decision of its own.
inline std::string refusal(const kind wanted, const std::string &service_name, const paths::layout layout) {
  if (wanted != kind::virtual_account) return std::string();
  if (!is_supported_service_name(service_name)) {
    return "The virtual service account is not available for a service named '" + service_name +
           "': the name must be plain ASCII without spaces or slashes. Reinstall the service under such a name, or keep it running as LocalSystem.";
  }
  if (layout != paths::layout::modern) {
    return "The virtual service account needs the modern layout, and this installation uses the legacy one. Switch the layout first with "
           "`nscp settings --migrate-layout modern` from an elevated prompt (or install the MSI with LAYOUT=modern), then run this again.";
  }
  return std::string();
}

// Why the layout cannot be changed to `target` while the service runs as
// `who`, or an empty string when it can. The other half of refusal(): leaving
// the modern layout would strand a virtual account on the legacy one.
inline std::string layout_change_refusal(const identity &who, const paths::layout target) {
  if (who.account != kind::virtual_account || target == paths::layout::modern) return std::string();
  return "The service runs as the virtual account " + account_name(who) +
         ", which needs the modern layout. Switch it back first with `nscp service --account system`, then run this again.";
}

// The privileges the service declares it needs when it runs as the virtual
// account. Everything not listed is removed from its token when it starts -
// most importantly SeImpersonatePrivilege, which every service account is
// given by default and which is the well-known route from a service account
// back to SYSTEM.
//
// SeChangeNotifyPrivilege is the one every process needs (it is what lets a
// path be traversed). The list is deliberately the minimum and is an
// assumption until it has been measured against the checks on a real host:
// see the service account plan, phase 0.
inline std::vector<std::string> required_privileges() { return {"SeChangeNotifyPrivilege"}; }

// The local groups the virtual account is added to, by SID so the answer is
// the same on every locale: Performance Monitor Users, Performance Log Users
// and Event Log Readers. They give back the performance counters and event
// logs that an account outside Administrators cannot otherwise read.
inline std::vector<std::string> group_sids() { return {"S-1-5-32-558", "S-1-5-32-559", "S-1-5-32-573"}; }

// Read the service identity from an already-loaded boot.ini.
inline identity from_boot_ini(CSimpleIni &boot_conf) {
  identity who;
  who.account = parse(utf8::cvt<std::string>(boot_conf.GetValue(L"service", L"account", L"")));
  who.service_name = utf8::cvt<std::string>(boot_conf.GetValue(L"service", L"name", L""));
  return who;
}

// Read the service identity from boot.ini at `path`. A missing or unreadable
// file is LocalSystem, which is what every installation that predates the
// setting gets.
inline identity from_boot_ini_file(const std::string &path) {
  CSimpleIni boot_conf;
  if (boot_conf.LoadFile(path.c_str()) < 0) return identity();
  return from_boot_ini(boot_conf);
}

}  // namespace service_account
}  // namespace nscp
