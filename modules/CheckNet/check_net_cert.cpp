// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_net_cert.hpp"

#include <boost/algorithm/string/join.hpp>
#include <net/socket/socket_helpers.hpp>

namespace check_net {
namespace cert {

// Fill `out` from a peer certificate read off a live session, and apply the
// `sans=` requirement. Returns false when a required name is missing, which is
// the caller's cue to fail the check's `result`.
bool populate(cert_fields &out, const socket_helpers::peer_certificate &info, const std::vector<std::string> &required_sans) {
  out.has_certificate = true;
  if (info.expiry_days) out.expiry_days = static_cast<long long>(info.expiry_days.value());
  out.subject = info.subject;
  out.issuer = info.issuer;
  out.subject_cn = info.subject_cn;
  out.issuer_cn = info.issuer_cn;
  out.self_signed = info.self_signed;
  out.sans = boost::algorithm::join(info.sans, ",");

  const std::vector<std::string> missing = find_missing_sans(info.sans, required_sans);
  out.missing_sans = boost::algorithm::join(missing, ",");
  return missing.empty();
}

}  // namespace cert
}  // namespace check_net
