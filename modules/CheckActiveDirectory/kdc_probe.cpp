// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "kdc_probe.hpp"

#include <boost/optional.hpp>
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
  } else {
    // Long form with however many length octets the size needs: a probe
    // request is tiny, but the realm is caller-supplied and a truncated
    // length octet string would silently corrupt the whole request.
    bytes len_octets;
    for (std::size_t v = len; v != 0; v >>= 8) len_octets.insert(len_octets.begin(), static_cast<unsigned char>(v & 0xff));
    out.push_back(static_cast<unsigned char>(0x80 | len_octets.size()));
    out.insert(out.end(), len_octets.begin(), len_octets.end());
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

// A bounds-checked read cursor over a slice of a DER buffer. Everything the
// classifier reads goes through here, so no caller can index the buffer or
// advance an offset by hand.
//
// Every length is validated against the bytes *remaining* (`remaining() < n`),
// never by comparing `pos + n` to the size: a response is attacker-supplied
// (the probe talks to whatever host:port the check was pointed at) and on a
// 32-bit build `pos + n` wraps, so a crafted 4-octet length such as
// 0xfffffff2 passed the old check and then drove the walk offset *backwards* -
// an unterminated parse loop that spun a worker thread forever.
class cursor {
 public:
  explicit cursor(const bytes &data) : data_(&data), pos_(0), end_(data.size()) {}

  bool done() const { return pos_ >= end_; }
  std::size_t remaining() const { return done() ? 0 : end_ - pos_; }

  boost::optional<unsigned char> take_byte() {
    if (done()) return boost::none;
    return (*data_)[pos_++];
  }

  // One TLV: returns a cursor over its contents and reports the tag, or an
  // empty optional when the buffer is truncated or the length is malformed.
  boost::optional<cursor> take_tlv(unsigned char &tag) {
    const boost::optional<unsigned char> t = take_byte();
    if (!t) return boost::none;
    const boost::optional<std::size_t> len = take_length();
    if (!len) return boost::none;
    // Safe: take_length() guarantees *len <= remaining(), so pos_ + *len <= end_.
    const cursor content(*data_, pos_, pos_ + *len);
    pos_ += *len;
    tag = *t;
    return content;
  }

  // The whole remaining slice as a two's-complement big-endian DER INTEGER.
  // Accumulated unsigned: shifting a negative value left is undefined before
  // C++20, and the sign seed makes short encodings extend correctly.
  boost::optional<long long> take_integer() {
    const std::size_t len = remaining();
    if (len < 1 || len > sizeof(long long)) return boost::none;
    unsigned long long acc = ((*data_)[pos_] & 0x80) != 0 ? ~0ULL : 0ULL;
    for (std::size_t i = 0; i < len; ++i) acc = (acc << 8) | *take_byte();
    return static_cast<long long>(acc);
  }

 private:
  cursor(const bytes &data, std::size_t from, std::size_t to) : data_(&data), pos_(from), end_(to) {}

  // DER length octets: short form, or long form with up to 4 length octets
  // (a Kerberos message is capped well below 4GB by the caller anyway).
  boost::optional<std::size_t> take_length() {
    const boost::optional<unsigned char> first = take_byte();
    if (!first) return boost::none;
    std::size_t len = *first & 0x7f;
    if ((*first & 0x80) != 0) {
      const std::size_t count = len;
      if (count == 0 || count > 4 || remaining() < count) return boost::none;
      len = 0;
      for (std::size_t i = 0; i < count; ++i) len = (len << 8) | *take_byte();
    }
    if (remaining() < len) return boost::none;
    return len;
  }

  const bytes *data_;  // pointer, not reference, so the cursor stays copyable
  std::size_t pos_;
  std::size_t end_;
};

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
  cursor outer(data);
  unsigned char tag = 0;
  boost::optional<cursor> application = outer.take_tlv(tag);
  if (!application) return result;
  boost::optional<cursor> sequence = application->take_tlv(tag);
  if (!sequence || tag != 0x30) return result;
  while (!sequence->done()) {
    boost::optional<cursor> field = sequence->take_tlv(tag);
    if (!field) return result;
    if (tag != 0xa6) continue;  // error-code [6]
    boost::optional<cursor> integer = field->take_tlv(tag);
    if (integer && tag == 0x02) {
      if (const boost::optional<long long> code = integer->take_integer()) result.error_code = *code;
    }
    return result;
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
