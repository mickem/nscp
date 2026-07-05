// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "script_identity.hpp"

#include <gtest/gtest.h>

#include <nscapi/protobuf/command.hpp>
#include <string>

// Tests for the parse-strip-restamp helper used by command_wrapper::query.
// Boundary-of-trust: anything a Python script wrote into the metadata
// gets thrown away for the caller_plugin_id key; the principal key is
// untouched (Python scripts don't have an end-user principal). The
// helper has zero Python deps so we can test it directly.

namespace {

struct ParsedMeta {
  std::string caller_plugin_id;
  int caller_plugin_id_count = 0;
  std::string principal;
  bool principal_present = false;
};

ParsedMeta parse_meta(const std::string& buffer) {
  ParsedMeta m;
  PB::Commands::QueryRequestMessage msg;
  EXPECT_TRUE(msg.ParseFromString(buffer));
  for (const auto& kv : msg.header().metadata()) {
    if (kv.key() == "nscp.caller_plugin_id") {
      m.caller_plugin_id = kv.value();
      ++m.caller_plugin_id_count;
    } else if (kv.key() == "nscp.principal") {
      m.principal = kv.value();
      m.principal_present = true;
    }
  }
  return m;
}

std::string make_request(const std::string& command, const std::string& caller_plugin_id_meta, const std::string& principal_meta) {
  PB::Commands::QueryRequestMessage msg;
  auto* payload = msg.add_payload();
  payload->set_command(command);
  if (!caller_plugin_id_meta.empty()) {
    auto* m = msg.mutable_header()->add_metadata();
    m->set_key("nscp.caller_plugin_id");
    m->set_value(caller_plugin_id_meta);
  }
  if (!principal_meta.empty()) {
    auto* m = msg.mutable_header()->add_metadata();
    m->set_key("nscp.principal");
    m->set_value(principal_meta);
  }
  std::string out;
  msg.SerializeToString(&out);
  return out;
}

}  // namespace

TEST(script_identity, non_protobuf_input_returns_verbatim) {
  // Garbage in -> garbage out: scripts that supply a non-standard payload
  // (e.g. a custom binary format their own handler decodes) must not be
  // broken by re-stamping. The fallthrough is observable as "the bytes
  // are unchanged".
  const std::string junk = "this is not a protobuf message at all";
  EXPECT_EQ(junk, script_wrapper::restamp_caller_plugin_id(junk, 42));
}

TEST(script_identity, empty_input_returns_verbatim) {
  // ParseFromString("") succeeds (empty protobuf is valid) - so for the
  // empty case the helper produces a valid serialized message with the
  // stamped metadata, not an empty string. Document that here.
  std::string out = script_wrapper::restamp_caller_plugin_id("", 42);
  // Not equal to input (we added metadata), and parses successfully:
  PB::Commands::QueryRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(out));
  EXPECT_EQ(1, parsed.header().metadata_size());
  EXPECT_EQ("nscp.caller_plugin_id", parsed.header().metadata(0).key());
  EXPECT_EQ("42", parsed.header().metadata(0).value());
}

TEST(script_identity, valid_request_without_meta_gets_stamped) {
  const std::string req = make_request("check_cpu", "", "");
  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  ParsedMeta m = parse_meta(out);
  EXPECT_EQ(1, m.caller_plugin_id_count);
  EXPECT_EQ("42", m.caller_plugin_id);
  EXPECT_FALSE(m.principal_present);
}

TEST(script_identity, stale_caller_id_is_replaced_not_appended) {
  // The script wrote "1" (perhaps copied from an incoming request).
  // After restamping, exactly one entry must exist, and it must be the
  // trusted plugin_id. Appending would let the policy see two values -
  // protobuf's repeated semantics make that undefined-but-broken.
  const std::string req = make_request("check_cpu", "1", "");
  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  ParsedMeta m = parse_meta(out);
  EXPECT_EQ(1, m.caller_plugin_id_count);
  EXPECT_EQ("42", m.caller_plugin_id);
}

TEST(script_identity, multiple_stale_caller_ids_are_all_removed) {
  // Defensive: even if a script somehow stamped the key twice, we strip
  // them all before adding the trusted one. Otherwise the policy would
  // see "the trusted id AND the stale ones" depending on iteration order.
  PB::Commands::QueryRequestMessage msg;
  msg.add_payload()->set_command("x");
  for (int i = 0; i < 3; ++i) {
    auto* m = msg.mutable_header()->add_metadata();
    m->set_key("nscp.caller_plugin_id");
    m->set_value("stale-" + std::to_string(i));
  }
  std::string req;
  msg.SerializeToString(&req);
  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  ParsedMeta m = parse_meta(out);
  EXPECT_EQ(1, m.caller_plugin_id_count);
  EXPECT_EQ("42", m.caller_plugin_id);
}

TEST(script_identity, principal_is_preserved_not_touched) {
  // The principal key is intentionally untouched - Python scripts can
  // still forward a principal if they want to. Restamping is only the
  // caller_plugin_id, not the principal.
  const std::string req = make_request("check_cpu", "1", "operator");
  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  ParsedMeta m = parse_meta(out);
  EXPECT_EQ("42", m.caller_plugin_id);
  EXPECT_TRUE(m.principal_present);
  EXPECT_EQ("operator", m.principal);
}

TEST(script_identity, command_and_arguments_are_preserved) {
  PB::Commands::QueryRequestMessage msg;
  auto* p = msg.add_payload();
  p->set_command("check_cpu");
  p->add_arguments("warn=load>80");
  p->add_arguments("crit=load>90");
  std::string req;
  msg.SerializeToString(&req);

  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  PB::Commands::QueryRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(out));
  ASSERT_EQ(1, parsed.payload_size());
  EXPECT_EQ("check_cpu", parsed.payload(0).command());
  ASSERT_EQ(2, parsed.payload(0).arguments_size());
  EXPECT_EQ("warn=load>80", parsed.payload(0).arguments(0));
  EXPECT_EQ("crit=load>90", parsed.payload(0).arguments(1));
}

TEST(script_identity, unrelated_metadata_is_preserved) {
  // Non-nscp metadata (e.g. a script's own key/value used by its handler)
  // must survive the restamping. We only touch nscp.caller_plugin_id.
  PB::Commands::QueryRequestMessage msg;
  msg.add_payload()->set_command("x");
  auto* user = msg.mutable_header()->add_metadata();
  user->set_key("my.script.context");
  user->set_value("preserve me");
  auto* stale = msg.mutable_header()->add_metadata();
  stale->set_key("nscp.caller_plugin_id");
  stale->set_value("stale");
  std::string req;
  msg.SerializeToString(&req);

  const std::string out = script_wrapper::restamp_caller_plugin_id(req, 42);
  PB::Commands::QueryRequestMessage parsed;
  ASSERT_TRUE(parsed.ParseFromString(out));
  bool found_user = false;
  bool found_trusted = false;
  for (const auto& kv : parsed.header().metadata()) {
    if (kv.key() == "my.script.context") {
      EXPECT_EQ("preserve me", kv.value());
      found_user = true;
    } else if (kv.key() == "nscp.caller_plugin_id") {
      EXPECT_EQ("42", kv.value());
      found_trusted = true;
    }
  }
  EXPECT_TRUE(found_user);
  EXPECT_TRUE(found_trusted);
}
