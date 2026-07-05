// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/protobuf/command.hpp>
#include <string>
#include <vector>

// Tests for the request-building helpers that nscapi::core_helper uses
// to stamp caller identity into outgoing QueryRequestMessages. The
// helpers are the trust source for the policy decision in plugin_manager
// (see extract_subject_from_header) - if these get the metadata layout
// wrong, every policy decision in the agent reads the wrong subject.

namespace {

struct ParsedMeta {
  std::string caller_plugin_id;  // empty if absent
  bool caller_plugin_id_present = false;
  std::string principal;
  bool principal_present = false;
};

ParsedMeta parse_meta(const std::string& buffer) {
  ParsedMeta m;
  PB::Commands::QueryRequestMessage msg;
  EXPECT_TRUE(msg.ParseFromString(buffer)) << "request should be a valid QueryRequestMessage";
  for (const auto& kv : msg.header().metadata()) {
    if (kv.key() == "nscp.caller_plugin_id") {
      m.caller_plugin_id = kv.value();
      m.caller_plugin_id_present = true;
    } else if (kv.key() == "nscp.principal") {
      m.principal = kv.value();
      m.principal_present = true;
    }
  }
  return m;
}

PB::Commands::QueryRequestMessage parse_request(const std::string& buffer) {
  PB::Commands::QueryRequestMessage msg;
  EXPECT_TRUE(msg.ParseFromString(buffer));
  return msg;
}

}  // namespace

// ===== build_simple_query_request =========================================

TEST(core_helper_request, simple_query_stamps_caller_plugin_id_unconditionally) {
  // simple_query (no _as) calls build_simple_query_request with an empty
  // principal. The caller_plugin_id MUST still be stamped - it's the key
  // input to the policy. Forgetting this is how legacy callers used to
  // disappear from the audit log.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, "", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_TRUE(m.caller_plugin_id_present);
  EXPECT_EQ("42", m.caller_plugin_id);
  EXPECT_FALSE(m.principal_present);  // empty principal MUST NOT be stamped
}

TEST(core_helper_request, simple_query_stamps_principal_when_supplied) {
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, "operator", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("42", m.caller_plugin_id);
  EXPECT_TRUE(m.principal_present);
  EXPECT_EQ("operator", m.principal);
}

TEST(core_helper_request, simple_query_preserves_command_and_arguments_list) {
  std::list<std::string> args = {"warn=load>80", "crit=load>90"};
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, "", "check_cpu", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  EXPECT_EQ("check_cpu", msg.payload(0).command());
  ASSERT_EQ(2, msg.payload(0).arguments_size());
  EXPECT_EQ("warn=load>80", msg.payload(0).arguments(0));
  EXPECT_EQ("crit=load>90", msg.payload(0).arguments(1));
}

TEST(core_helper_request, simple_query_preserves_command_and_arguments_vector) {
  std::vector<std::string> args = {"a", "b", "c"};
  std::string buf;
  nscapi::request_builder::build_simple_query_request(7, "", "check_x", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  EXPECT_EQ("check_x", msg.payload(0).command());
  ASSERT_EQ(3, msg.payload(0).arguments_size());
  EXPECT_EQ("a", msg.payload(0).arguments(0));
  EXPECT_EQ("c", msg.payload(0).arguments(2));
}

TEST(core_helper_request, simple_query_does_not_stamp_principal_for_empty_string) {
  // Explicitly: an empty principal must NOT add a metadata entry. The
  // policy treats absent and empty differently downstream (a `:` in the
  // subject changes glob semantics) so the difference is observable.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "x", std::list<std::string>{}, buf);
  auto msg = parse_request(buf);
  for (const auto& kv : msg.header().metadata()) {
    EXPECT_NE("nscp.principal", kv.key()) << "empty principal must not produce a metadata entry";
  }
}

TEST(core_helper_request, simple_query_negative_plugin_id_is_stringified_verbatim) {
  // str::xtos passes signed ints through - negative is unusual but should
  // not crash; the policy will just fail to resolve and fall back to "*".
  std::string buf;
  nscapi::request_builder::build_simple_query_request(-1, "", "x", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("-1", m.caller_plugin_id);
}

// ===== build_query_request_on_behalf_of ===================================

TEST(core_helper_request, on_behalf_of_uses_verbatim_plugin_id_string) {
  // The proxy path (CheckHelpers) forwards the upstream's stringified id
  // without parsing it. Even non-numeric "ids" should pass through
  // verbatim - they will simply fail to resolve server-side, but that's
  // plugin_manager's problem, not the helper's.
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("7", "operator", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("7", m.caller_plugin_id);
  EXPECT_EQ("operator", m.principal);
}

TEST(core_helper_request, on_behalf_of_empty_plugin_id_omits_key) {
  // An empty caller_plugin_id_str must NOT result in
  // `nscp.caller_plugin_id = ""` - that would resolve to "" in the
  // policy, masking the "no identity present" signal. The key has to be
  // absent entirely so plugin_manager treats it as "no identity".
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("", "operator", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_FALSE(m.caller_plugin_id_present);
  EXPECT_EQ("operator", m.principal);
}

TEST(core_helper_request, on_behalf_of_empty_principal_omits_key) {
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("7", "", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("7", m.caller_plugin_id);
  EXPECT_FALSE(m.principal_present);
}

TEST(core_helper_request, on_behalf_of_both_empty_yields_no_identity_metadata) {
  // Degenerate case: caller passed neither. The simple_query_on_behalf_of
  // wrapper falls back to simple_query in this case, but the builder
  // itself just emits no identity keys.
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("", "", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_FALSE(m.caller_plugin_id_present);
  EXPECT_FALSE(m.principal_present);
}

TEST(core_helper_request, on_behalf_of_preserves_command_and_arguments) {
  std::list<std::string> args = {"warn=load>80"};
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("7", "u", "check_cpu", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  EXPECT_EQ("check_cpu", msg.payload(0).command());
  ASSERT_EQ(1, msg.payload(0).arguments_size());
  EXPECT_EQ("warn=load>80", msg.payload(0).arguments(0));
}

TEST(core_helper_request, on_behalf_of_preserves_non_numeric_caller_id_verbatim) {
  // Defensive: if a buggy upstream stamps a non-numeric id, the proxy
  // forwards it as-is. plugin_manager's extract_subject_from_header
  // catches the parse failure and falls back to "*".
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("not-a-number", "u", "x", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("not-a-number", m.caller_plugin_id);
}

// ===== Metadata invariants ================================================

// Count the number of metadata entries with a given key. The builders
// stamp at most one of each identity key; emitting two of the same key
// would let a malicious header inject a second value the policy might
// pick up (extract_subject_from_header reads the LAST one, since the
// loop overwrites).
namespace {
int count_meta(const std::string& buffer, const std::string& key) {
  PB::Commands::QueryRequestMessage msg;
  EXPECT_TRUE(msg.ParseFromString(buffer));
  int n = 0;
  for (const auto& kv : msg.header().metadata()) {
    if (kv.key() == key) ++n;
  }
  return n;
}
}  // namespace

TEST(core_helper_request, simple_query_emits_exactly_one_caller_plugin_id) {
  // No identity-key duplication. The builder must stamp the
  // caller_plugin_id exactly once, even if some future refactor adds a
  // second code path that also appends metadata.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, "operator", "check_cpu", std::list<std::string>{}, buf);
  EXPECT_EQ(1, count_meta(buf, "nscp.caller_plugin_id"));
  EXPECT_EQ(1, count_meta(buf, "nscp.principal"));
}

TEST(core_helper_request, on_behalf_of_emits_at_most_one_of_each_identity_key) {
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("7", "u", "check_cpu", std::list<std::string>{}, buf);
  EXPECT_EQ(1, count_meta(buf, "nscp.caller_plugin_id"));
  EXPECT_EQ(1, count_meta(buf, "nscp.principal"));
}

// ===== Principal shape ====================================================

// NRPEServer can now stamp a Subject DN (RFC 2253) as the principal.
// Those strings are user-influenced (cert CN can contain commas, equals
// signs, quotes, escaped backslashes) and are the most exotic principal
// the builder sees in practice. The builder must pass them through
// verbatim - protobuf's string field is binary-safe, so this is more an
// assertion about "we don't accidentally sanitise it" than about
// protobuf's correctness.
TEST(core_helper_request, principal_handles_dn_with_commas_and_equals) {
  const std::string dn = "CN=icinga-master,O=Acme,C=US";
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, dn, "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ(dn, m.principal);
}

TEST(core_helper_request, principal_handles_dn_with_escaped_backslashes) {
  // RFC 2253 escapes specials with `\`; the literal escape sequence must
  // round-trip without further transformation.
  const std::string dn = "CN=name\\, with comma,O=Acme";
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, dn, "x", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ(dn, m.principal);
}

TEST(core_helper_request, principal_handles_utf8) {
  // OpenSSL emits UTF-8 directly under XN_FLAG_RFC2253; the builder
  // must not re-encode it. Use a fixed UTF-8 byte sequence rather than
  // a string literal so the test does not depend on the source file
  // encoding.
  const std::string utf8_dn = std::string("CN=\xc3\xa9quipe,O=Acme");  // "équipe"
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, utf8_dn, "x", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ(utf8_dn, m.principal);
}

// ===== Plugin id boundaries ===============================================

TEST(core_helper_request, simple_query_plugin_id_zero_is_stamped) {
  // Zero is a valid plugin id in practice (some boot paths use it as
  // the "system" id). Must be stamped, not omitted - empty plugin_id
  // metadata is semantically "no caller" and would mask the call.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(0, "", "check_cpu", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_TRUE(m.caller_plugin_id_present);
  EXPECT_EQ("0", m.caller_plugin_id);
}

TEST(core_helper_request, simple_query_large_plugin_id_round_trips) {
  // INT_MAX is well above any realistic plugin count, but the stringifier
  // is signed-int based so it's worth sanity-checking the upper bound.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(2147483647, "", "x", std::list<std::string>{}, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("2147483647", m.caller_plugin_id);
}

// ===== Argument handling ==================================================

TEST(core_helper_request, simple_query_no_arguments_yields_empty_argument_list) {
  // A command with zero args must still produce a valid payload with an
  // empty arguments repeated field, not a missing payload.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "check_uptime", std::list<std::string>{}, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  EXPECT_EQ("check_uptime", msg.payload(0).command());
  EXPECT_EQ(0, msg.payload(0).arguments_size());
}

TEST(core_helper_request, simple_query_preserves_empty_argument_strings) {
  // Empty strings in the argument list are distinct from "no arg" and
  // must round-trip. Some checks rely on positional empty args (e.g.
  // `cmd!!warn=80` to skip the first arg).
  std::list<std::string> args = {"", "warn=80", ""};
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "x", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(3, msg.payload(0).arguments_size());
  EXPECT_EQ("", msg.payload(0).arguments(0));
  EXPECT_EQ("warn=80", msg.payload(0).arguments(1));
  EXPECT_EQ("", msg.payload(0).arguments(2));
}

TEST(core_helper_request, simple_query_preserves_argument_order_under_many_args) {
  // Repeated-field order under protobuf is guaranteed but worth pinning -
  // a refactor that switches to an unordered container would break
  // commands whose meaning is positional (range thresholds, ordered flags).
  std::list<std::string> args;
  for (int i = 0; i < 64; ++i) args.push_back("a" + std::to_string(i));
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "x", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(64, msg.payload(0).arguments_size());
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ("a" + std::to_string(i), msg.payload(0).arguments(i)) << "drift at index " << i;
  }
}

TEST(core_helper_request, simple_query_argument_with_embedded_nul_round_trips) {
  // protobuf string fields are binary-safe. Some upstream check_nrpe
  // builds in the wild forward args with embedded NULs by accident and
  // we shouldn't silently truncate them.
  std::string arg("ab", 2);
  arg.push_back('\0');
  arg.append("cd", 2);
  std::list<std::string> args = {arg};
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "x", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload(0).arguments_size());
  EXPECT_EQ(arg, msg.payload(0).arguments(0));
  EXPECT_EQ(5u, msg.payload(0).arguments(0).size());
}

TEST(core_helper_request, on_behalf_of_preserves_multiple_arguments_in_order) {
  // The existing on_behalf_of test only covered one argument. Multiple
  // ordered args is the more realistic shape for a proxy-wrapped check.
  std::list<std::string> args = {"warn=load>80", "crit=load>90", "show-all"};
  std::string buf;
  nscapi::request_builder::build_query_request_on_behalf_of("7", "u", "check_load", args, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  ASSERT_EQ(3, msg.payload(0).arguments_size());
  EXPECT_EQ("warn=load>80", msg.payload(0).arguments(0));
  EXPECT_EQ("crit=load>90", msg.payload(0).arguments(1));
  EXPECT_EQ("show-all", msg.payload(0).arguments(2));
}

// ===== Command field ======================================================

TEST(core_helper_request, simple_query_empty_command_is_passed_through) {
  // Builder doesn't validate command emptiness - that's the policy /
  // command registry's job. An empty command should still produce a
  // valid message that downstream can reject cleanly.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "", std::list<std::string>{}, buf);
  auto msg = parse_request(buf);
  ASSERT_EQ(1, msg.payload_size());
  EXPECT_EQ("", msg.payload(0).command());
}

TEST(core_helper_request, simple_query_command_with_dot_preserved_verbatim) {
  // Object form `module.command` is the canonical "qualified" command
  // shape. The builder must not split or rewrite it.
  std::string buf;
  nscapi::request_builder::build_simple_query_request(1, "", "CheckSystem.check_cpu", std::list<std::string>{}, buf);
  auto msg = parse_request(buf);
  EXPECT_EQ("CheckSystem.check_cpu", msg.payload(0).command());
}

// ===== Vector overload parity =============================================

// The list-of-string and vector-of-string overloads share an
// implementation today (build_simple_query_request_impl), but they are
// separate exported entry points and someone could refactor one without
// the other. Pin parity so the two overloads stay observationally
// equivalent for typical inputs.
TEST(core_helper_request, simple_query_list_and_vector_overloads_produce_equivalent_output) {
  std::list<std::string> list_args = {"a", "b", "c"};
  std::vector<std::string> vec_args = {"a", "b", "c"};
  std::string list_buf, vec_buf;
  nscapi::request_builder::build_simple_query_request(42, "op", "check_cpu", list_args, list_buf);
  nscapi::request_builder::build_simple_query_request(42, "op", "check_cpu", vec_args, vec_buf);
  // Byte-for-byte equality would be too strict (protobuf does not
  // promise stable serialisation across calls in principle), so compare
  // the parsed messages instead.
  auto list_msg = parse_request(list_buf);
  auto vec_msg = parse_request(vec_buf);
  EXPECT_EQ(list_msg.SerializeAsString().size(), vec_msg.SerializeAsString().size());
  ASSERT_EQ(list_msg.payload_size(), vec_msg.payload_size());
  ASSERT_EQ(list_msg.payload(0).arguments_size(), vec_msg.payload(0).arguments_size());
  for (int i = 0; i < list_msg.payload(0).arguments_size(); ++i) {
    EXPECT_EQ(list_msg.payload(0).arguments(i), vec_msg.payload(0).arguments(i));
  }
  EXPECT_EQ(list_msg.payload(0).command(), vec_msg.payload(0).command());
}

TEST(core_helper_request, simple_query_vector_overload_stamps_identity_like_list) {
  // The vector overload calls the same template as list, but it's
  // exported as a separate symbol. Make sure both code paths actually
  // stamp the identity metadata.
  std::vector<std::string> args = {"warn=80"};
  std::string buf;
  nscapi::request_builder::build_simple_query_request(42, "operator", "check_cpu", args, buf);
  ParsedMeta m = parse_meta(buf);
  EXPECT_EQ("42", m.caller_plugin_id);
  EXPECT_EQ("operator", m.principal);
}

// ===== Idempotency of the buffer ==========================================

TEST(core_helper_request, builder_overwrites_output_buffer) {
  // The builders take `std::string &out_buffer` by reference. Make sure
  // they reset it rather than appending - otherwise calling the builder
  // twice with the same buffer would yield a corrupted (concatenated)
  // wire format.
  std::string buf = "previous garbage that must not survive";
  nscapi::request_builder::build_simple_query_request(1, "", "check_cpu", std::list<std::string>{}, buf);
  auto msg = parse_request(buf);
  EXPECT_EQ("check_cpu", msg.payload(0).command());
  EXPECT_EQ(1, msg.payload_size());
}
