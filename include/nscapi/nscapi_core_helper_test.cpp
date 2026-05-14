/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 */

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
