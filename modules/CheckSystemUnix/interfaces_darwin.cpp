// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "interfaces_darwin.h"

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace darwin_interfaces {

namespace {

std::string format_mac(const unsigned char *bytes, const std::size_t length) {
  if (length == 0) return "";
  std::string out;
  char part[4];
  for (std::size_t i = 0; i < length; ++i) {
    std::snprintf(part, sizeof(part), i == 0 ? "%02x" : ":%02x", bytes[i]);
    out += part;
  }
  return out;
}

// The link state. An interface that is administratively down is "down"; one
// that is up asks its media for the carrier (what `ifconfig` prints as
// "status: active"). Interfaces without media - the loopback, tunnels,
// bridges - answer ENOTTY/EINVAL there and are up when the kernel says they
// are running.
std::string link_status(const int sock, const std::string &name, const int flags) {
  if ((flags & IFF_UP) == 0) return "down";
  if (sock >= 0) {
    struct ifmediareq media;
    std::memset(&media, 0, sizeof(media));
    std::strncpy(media.ifm_name, name.c_str(), sizeof(media.ifm_name) - 1);
    if (ioctl(sock, SIOCGIFMEDIA, &media) == 0 && (media.ifm_status & IFM_AVALID) != 0) {
      return (media.ifm_status & IFM_ACTIVE) != 0 ? "up" : "down";
    }
  }
  return (flags & IFF_RUNNING) != 0 ? "up" : "down";
}

}  // namespace

std::vector<interface_info> read() {
  int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
  std::vector<char> buffer;
  // The list can grow between the size query and the read (an interface
  // coming up); retry with the new size a few times rather than fail.
  for (int attempt = 0; attempt < 4; ++attempt) {
    std::size_t length = 0;
    if (sysctl(mib, 6, nullptr, &length, nullptr, 0) != 0) {
      throw std::runtime_error(std::string("sysctl NET_RT_IFLIST2 failed: ") + std::strerror(errno));
    }
    buffer.resize(length + 1024);
    length = buffer.size();
    if (sysctl(mib, 6, buffer.data(), &length, nullptr, 0) == 0) {
      buffer.resize(length);
      break;
    }
    if (errno != ENOMEM || attempt == 3) {
      throw std::runtime_error(std::string("sysctl NET_RT_IFLIST2 failed: ") + std::strerror(errno));
    }
  }

  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  std::vector<interface_info> result;
  const char *const end = buffer.data() + buffer.size();
  for (const char *p = buffer.data(); p + sizeof(struct if_msghdr) <= end;) {
    const struct if_msghdr *header = reinterpret_cast<const struct if_msghdr *>(p);
    if (header->ifm_msglen == 0) break;
    const char *const next = p + header->ifm_msglen;
    if (next > end) break;
    if (header->ifm_type == RTM_IFINFO2 && header->ifm_msglen >= sizeof(struct if_msghdr2) + offsetof(struct sockaddr_dl, sdl_data)) {
      const struct if_msghdr2 *info = reinterpret_cast<const struct if_msghdr2 *>(p);
      const struct sockaddr_dl *link = reinterpret_cast<const struct sockaddr_dl *>(info + 1);
      if (link->sdl_family == AF_LINK && link->sdl_nlen > 0 && link->sdl_data + link->sdl_nlen <= next) {
        interface_info nic;
        nic.name.assign(link->sdl_data, link->sdl_nlen);
        const bool has_mac = link->sdl_alen > 0 && reinterpret_cast<const char *>(LLADDR(link)) + link->sdl_alen <= next;
        if (has_mac) nic.mac = format_mac(reinterpret_cast<const unsigned char *>(LLADDR(link)), link->sdl_alen);
        nic.loopback = (info->ifm_flags & IFF_LOOPBACK) != 0;
        nic.status = link_status(sock, nic.name, info->ifm_flags);
        nic.speed_bps = static_cast<long long>(info->ifm_data.ifi_baudrate);
        nic.rx_bytes = info->ifm_data.ifi_ibytes;
        nic.rx_packets = info->ifm_data.ifi_ipackets;
        nic.rx_errors = info->ifm_data.ifi_ierrors;
        nic.tx_bytes = info->ifm_data.ifi_obytes;
        nic.tx_packets = info->ifm_data.ifi_opackets;
        nic.tx_errors = info->ifm_data.ifi_oerrors;
        result.push_back(nic);
      }
    }
    p = next;
  }
  if (sock >= 0) close(sock);
  return result;
}

}  // namespace darwin_interfaces
