// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "kdc_probe.hpp"

#include <str/xtos.hpp>

namespace kdc_probe {

namespace {

// --- minimal DER writer -----------------------------------------------------

bytes cat(std::initializer_list<bytes> parts) {
  bytes out;
  for (const bytes &p : parts) out.insert(out.end(), p.begin(), p.end());
  return out;
}

bytes der(unsigned char tag, const bytes &content) {
  bytes out;
  out.push_back(tag);
  const std::size_t len = content.size();
  if (len < 0x80) {
    out.push_back(static_cast<unsigned char>(len));
  } else if (len <= 0xff) {
    out.push_back(0x81);
    out.push_back(static_cast<unsigned char>(len));
  } else {
    out.push_back(0x82);
    out.push_back(static_cast<unsigned char>(len >> 8));
    out.push_back(static_cast<unsigned char>(len & 0xff));
  }
  out.insert(out.end(), content.begin(), content.end());
  return out;
}

bytes seq(const bytes &content) { return der(0x30, content); }
// Context-specific constructed tag [n] (Kerberos uses explicit tagging throughout).
bytes ctx(int n, const bytes &content) { return der(static_cast<unsigned char>(0xa0 | n), content); }

bytes der_int(unsigned long v) {
  bytes content;
  do {
    content.insert(content.begin(), static_cast<unsigned char>(v & 0xff));
    v >>= 8;
  } while (v != 0);
  if ((content[0] & 0x80) != 0) content.insert(content.begin(), 0x00);  // keep it non-negative
  return der(0x02, content);
}

bytes general_string(const std::string &s) { return der(0x1b, bytes(s.begin(), s.end())); }

// PrincipalName ::= SEQUENCE { name-type [0] Int32, name-string [1] SEQUENCE OF KerberosString }
bytes principal_name(unsigned long name_type, std::initializer_list<std::string> names) {
  bytes name_strings;
  for (const std::string &n : names) name_strings = cat({name_strings, general_string(n)});
  return seq(cat({ctx(0, der_int(name_type)), ctx(1, seq(name_strings))}));
}

// --- minimal DER reader (for KRB-ERROR) -------------------------------------

bool read_len(const bytes &d, std::size_t &pos, std::size_t &out) {
  if (pos >= d.size()) return false;
  const unsigned char first = d[pos++];
  if ((first & 0x80) == 0) {
    out = first;
  } else {
    const std::size_t count = first & 0x7f;
    if (count == 0 || count > 4 || pos + count > d.size()) return false;
    out = 0;
    for (std::size_t i = 0; i < count; ++i) out = (out << 8) | d[pos++];
  }
  return pos + out <= d.size();
}

}  // namespace

bytes build_as_req(const std::string &realm, const std::string &client_name, unsigned long nonce) {
  // KDCOptions ::= BIT STRING (32 bits), none set.
  const bytes kdc_options = der(0x03, {0x00, 0x00, 0x00, 0x00, 0x00});
  // KerberosTime "till": the classic krb5 end-of-time value; the probe never
  // uses the ticket, it only needs a syntactically valid request.
  static const char kTill[] = "20370913024805Z";
  const bytes till = der(0x18, bytes(kTill, kTill + sizeof(kTill) - 1));
  // aes256-cts-hmac-sha1-96 (18), aes128-cts-hmac-sha1-96 (17), rc4-hmac (23).
  const bytes etypes = seq(cat({der_int(18), der_int(17), der_int(23)}));

  const bytes req_body = seq(cat({
      ctx(0, kdc_options),
      ctx(1, principal_name(1, {client_name})),      // cname, NT-PRINCIPAL
      ctx(2, general_string(realm)),                 // realm
      ctx(3, principal_name(2, {"krbtgt", realm})),  // sname, NT-SRV-INST
      ctx(5, till),                                  // till
      ctx(7, der_int(nonce)),                        // nonce
      ctx(8, etypes),                                // etype preference list
  }));

  // AS-REQ ::= [APPLICATION 10] KDC-REQ; KDC-REQ ::= SEQUENCE { pvno [1] 5, msg-type [2] 10, req-body [4] }
  return der(0x6a, seq(cat({ctx(1, der_int(5)), ctx(2, der_int(10)), ctx(4, req_body)})));
}

classification classify_response(const bytes &data) {
  classification result;
  if (data.size() < 2) return result;

  if (data[0] == 0x6b) {  // [APPLICATION 11] AS-REP
    result.kind = response_kind::as_rep;
    return result;
  }
  if (data[0] != 0x7e) return result;  // not [APPLICATION 30] KRB-ERROR

  result.kind = response_kind::krb_error;
  // Walk KRB-ERROR ::= [APPLICATION 30] SEQUENCE { ... error-code [6] Int32 ... }
  // for the error code; a parse failure still counts as a live KDC, we just
  // cannot name the error.
  std::size_t pos = 1, len = 0;
  if (!read_len(data, pos, len)) return result;
  if (pos >= data.size() || data[pos] != 0x30) return result;
  ++pos;
  std::size_t seq_len = 0;
  if (!read_len(data, pos, seq_len)) return result;
  const std::size_t seq_end = pos + seq_len;
  while (pos < seq_end && pos < data.size()) {
    const unsigned char tag = data[pos++];
    std::size_t field_len = 0;
    if (!read_len(data, pos, field_len)) return result;
    if (tag == 0xa6) {  // error-code [6]: INTEGER
      std::size_t ipos = pos;
      if (ipos < data.size() && data[ipos] == 0x02) {
        ++ipos;
        std::size_t ilen = 0;
        if (read_len(data, ipos, ilen) && ilen >= 1 && ilen <= 8) {
          long long code = (data[ipos] & 0x80) != 0 ? -1 : 0;  // sign-extend
          for (std::size_t i = 0; i < ilen; ++i) code = (code << 8) | data[ipos + i];
          result.error_code = code;
        }
      }
      return result;
    }
    pos += field_len;
  }
  return result;
}

std::string krb_error_name(long long code) {
  switch (code) {
    case 6:
      return "KDC_ERR_C_PRINCIPAL_UNKNOWN";
    case 7:
      return "KDC_ERR_S_PRINCIPAL_UNKNOWN";
    case 10:
      return "KDC_ERR_CANNOT_POSTDATE";
    case 14:
      return "KDC_ERR_ETYPE_NOSUPP";
    case 23:
      return "KDC_ERR_KEY_EXPIRED";
    case 24:
      return "KDC_ERR_PREAUTH_FAILED";
    case 25:
      return "KDC_ERR_PREAUTH_REQUIRED";
    case 37:
      return "KRB_AP_ERR_SKEW";
    case 52:
      return "KRB_ERR_RESPONSE_TOO_BIG";
    case 68:
      return "KRB_ERR_WRONG_REALM";
    default:
      return "error " + str::xtos(code);
  }
}

std::string classification::describe() const {
  switch (kind) {
    case response_kind::as_rep:
      return "AS-REP (ticket issued)";
    case response_kind::krb_error:
      return error_code < 0 ? "KRB-ERROR" : "KRB-ERROR " + krb_error_name(error_code);
    default:
      return "invalid response";
  }
}

}  // namespace kdc_probe
