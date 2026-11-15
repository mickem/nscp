// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <client/diagnostics_renderer.hpp>
#include <string>

namespace {
PB::Diagnostics::DiagnosticsResponseMessage::Response *envelope(PB::Diagnostics::DiagnosticsResponseMessage &message, const bool enabled = true,
                                                                const unsigned long long id = 7) {
  PB::Diagnostics::DiagnosticsResponseMessage::Response *payload = message.add_payload();
  payload->set_enabled(enabled);
  payload->set_snapshot_id(id);
  payload->set_captured_at(1763203327);
  payload->set_max_rows(10);
  return payload;
}

void processes(PB::Diagnostics::DiagnosticsMessage *snapshot) {
  PB::Diagnostics::Section *s = snapshot->add_sections();
  s->set_name("processes.top_cpu");
  s->set_source("CheckSystem");
  s->set_captured_at(1763203326);
  s->set_description("Processes using the most CPU");
  PB::Diagnostics::Column *c = s->add_columns();
  c->set_name("name");
  c = s->add_columns();
  c->set_name("pid");
  c->set_type(PB::Diagnostics::Column::INT);
  c = s->add_columns();
  c->set_name("cpu");
  c->set_type(PB::Diagnostics::Column::FLOAT);
  c->set_unit("percent");
  PB::Diagnostics::Row *r = s->add_rows();
  r->add_cells()->set_s("acme-indexer.exe");
  r->add_cells()->set_i(4242);
  r->add_cells()->set_f(87.5);
  r = s->add_rows();
  r->add_cells()->set_s("svchost.exe");
  r->add_cells()->set_i(17);
  r->add_cells()->set_f(3.0);
}
}  // namespace

TEST(diagnostics_renderer, says_why_there_is_nothing) {
  EXPECT_EQ(client::render_diagnostics("", ""), "Diagnostics are not available from this core.");
  PB::Diagnostics::DiagnosticsResponseMessage off;
  envelope(off, false, 0);
  EXPECT_NE(client::render_diagnostics(off.SerializeAsString(), "").find("disabled"), std::string::npos);
  PB::Diagnostics::DiagnosticsResponseMessage early;
  envelope(early, true, 0);
  EXPECT_NE(client::render_diagnostics(early.SerializeAsString(), "").find("No diagnostics collected yet"), std::string::npos);
}

TEST(diagnostics_renderer, lists_the_sections_with_their_rows_and_status) {
  PB::Diagnostics::DiagnosticsResponseMessage message;
  PB::Diagnostics::DiagnosticsResponseMessage::Response *payload = envelope(message);
  processes(payload->mutable_snapshot());
  PB::Diagnostics::Section *failed = payload->mutable_snapshot()->add_sections();
  failed->set_name("eventlog.errors");
  failed->set_source("CheckEventLog");
  failed->set_error("subscription failed");
  payload->add_excluded("services.problems");
  const std::string out = client::render_diagnostics(message.SerializeAsString(), "");
  EXPECT_NE(out.find("Snapshot 7, collected 2025-11-15T10:42:07Z"), std::string::npos) << out;
  EXPECT_NE(out.find("NAME               SOURCE"), std::string::npos) << out;
  EXPECT_NE(out.find("processes.top_cpu  CheckSystem    2"), std::string::npos) << out;
  EXPECT_NE(out.find("error: subscription failed"), std::string::npos) << out;
  EXPECT_NE(out.find("Left out by prune pattern: services.problems"), std::string::npos) << out;
}

TEST(diagnostics_renderer, prints_one_section_as_a_table) {
  PB::Diagnostics::DiagnosticsResponseMessage message;
  processes(envelope(message)->mutable_snapshot());
  const std::string out = client::render_diagnostics(message.SerializeAsString(), "processes.top_cpu");
  EXPECT_NE(out.find("processes.top_cpu (CheckSystem, captured 2025-11-15T10:42:06Z)"), std::string::npos) << out;
  EXPECT_NE(out.find("  NAME              PID   CPU (percent)"), std::string::npos) << out;
  EXPECT_NE(out.find("  acme-indexer.exe  4242  87.5"), std::string::npos) << out;
  // 3.0 is printed as 3, and nothing trails the last column.
  const std::string last = "  svchost.exe       17    3";
  ASSERT_GE(out.size(), last.size());
  EXPECT_EQ(out.substr(out.size() - last.size()), last) << out;
  EXPECT_EQ(out.find("truncated"), std::string::npos);
}

TEST(diagnostics_renderer, says_when_a_section_is_missing_empty_or_failed) {
  PB::Diagnostics::DiagnosticsResponseMessage missing;
  envelope(missing);
  EXPECT_EQ(client::render_diagnostics(missing.SerializeAsString(), "nope"), "Diagnostics section not found: nope");

  PB::Diagnostics::DiagnosticsResponseMessage failed;
  PB::Diagnostics::Section *s = envelope(failed)->mutable_snapshot()->add_sections();
  s->set_name("eventlog.errors");
  s->set_source("CheckEventLog");
  s->set_error("subscription failed");
  const std::string out = client::render_diagnostics(failed.SerializeAsString(), "eventlog.errors");
  EXPECT_NE(out.find("Error: subscription failed"), std::string::npos) << out;
  EXPECT_EQ(out.find("(no rows)"), std::string::npos) << out;
}
