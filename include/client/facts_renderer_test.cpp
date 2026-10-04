// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <client/facts_renderer.hpp>

namespace {
PB::Facts::FactsResponseMessage::Response *start(PB::Facts::FactsResponseMessage &message) {
  PB::Facts::FactsResponseMessage::Response *payload = message.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_OK);
  return payload;
}

PB::Facts::Field *add_string(PB::Facts::Object *object, const std::string &key, const std::string &value) {
  PB::Facts::Field *field = object->add_fields();
  field->set_key(key);
  field->mutable_value()->set_string_value(value);
  return field;
}

// The document the whole-document form of the response carries.
PB::Facts::Object *document_of(PB::Facts::FactsResponseMessage::Response *payload) { return payload->mutable_facts()->mutable_object_value(); }
}  // namespace

TEST(FactsRenderer, AFreshInstallSaysWhatToDoAboutIt) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  document_of(payload);

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("Revision: 0"), std::string::npos) << out;
  EXPECT_NE(out.find("Enabled: (none"), std::string::npos) << out;
  EXPECT_NE(out.find("(no facts collected)"), std::string::npos) << out;
}

TEST(FactsRenderer, TheHeaderCarriesTheRoundsMetadata) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_revision(7);
  payload->set_collected("2026-09-22T10:00:00Z");
  payload->add_enabled("os");
  payload->add_enabled("storage");
  PB::Common::KeyValue *error = payload->add_errors();
  error->set_key("hardware");
  error->set_value("access denied");
  add_string(document_of(payload), "family", "windows");

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  // "Checked", not "Collected": the header time is when the core last asked,
  // which for a producer that caches its snapshot is not when the values were
  // read. That is what the per-set Gathered lines say.
  EXPECT_EQ(out.substr(0, out.find('\n')), "Revision: 7  Checked: 2026-09-22T10:00:00Z");
  EXPECT_NE(out.find("\nEnabled: os, storage"), std::string::npos) << out;
  EXPECT_NE(out.find("\nErrors:\n  hardware: access denied"), std::string::npos) << "a set that failed to collect is reported, not silently absent";
}

TEST(FactsRenderer, TheHeaderSeparatesWhenItAskedFromWhenTheValuesWereRead) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_revision(7);
  payload->set_collected("2026-09-22T10:00:00Z");
  payload->add_enabled("os");
  // The producer read the machine at boot and has handed back that snapshot
  // since; the round that carried it says nothing about how old it is.
  PB::Common::KeyValue *gathered = payload->add_gathered();
  gathered->set_key("os");
  gathered->set_value("2026-09-22T06:12:41Z");
  add_string(document_of(payload), "family", "windows");

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("Checked: 2026-09-22T10:00:00Z"), std::string::npos) << out;
  EXPECT_NE(out.find("\nGathered:\n  os: 2026-09-22T06:12:41Z"), std::string::npos) << out;
}

TEST(FactsRenderer, ASetThatDidNotSayWhenItReadCarriesNoGatheredLine) {
  // A producer that collects on every round need not say: the round time is
  // the same moment, and an empty Gathered block would be noise.
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_collected("2026-09-22T10:00:00Z");
  payload->add_enabled("os");
  add_string(document_of(payload), "family", "windows");

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_EQ(out.find("Gathered:"), std::string::npos) << out;
}

TEST(FactsRenderer, ScalarsAreOneLineEach) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  PB::Facts::Object *os = document_of(payload);
  add_string(os, "family", "windows");
  os->add_fields()->set_key("cores");
  os->mutable_fields(1)->mutable_value()->set_int_value(64);
  os->add_fields()->set_key("memory_bytes");
  os->mutable_fields(2)->mutable_value()->set_uint_value(18446744073709551615ull);
  os->add_fields()->set_key("load");
  os->mutable_fields(3)->mutable_value()->set_double_value(1.5);
  os->add_fields()->set_key("virtual");
  os->mutable_fields(4)->mutable_value()->set_bool_value(false);

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("\n  family: windows"), std::string::npos) << out;
  EXPECT_NE(out.find("\n  cores: 64"), std::string::npos) << out;
  EXPECT_NE(out.find("\n  memory_bytes: 18446744073709551615"), std::string::npos) << out;
  // Whatever the host's locale calls a decimal point, this is a dot.
  EXPECT_NE(out.find("\n  load: 1.5"), std::string::npos) << out;
  EXPECT_NE(out.find("\n  virtual: false"), std::string::npos) << out;
}

TEST(FactsRenderer, ARecordListLeadsWithTheId) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  PB::Facts::Field *storage = document_of(payload)->add_fields();
  storage->set_key("storage");
  PB::Facts::Field *volumes = storage->mutable_value()->mutable_object_value()->add_fields();
  volumes->set_key("volumes");
  PB::Facts::List *list = volumes->mutable_value()->mutable_list_value();
  PB::Facts::Object *c = list->add_values()->mutable_object_value();
  add_string(c, "id", "C:");
  add_string(c, "fs", "NTFS");
  PB::Facts::Object *d = list->add_values()->mutable_object_value();
  add_string(d, "id", "D:");

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("\n  storage:\n    volumes:\n      - C:\n          fs: NTFS\n      - D:"), std::string::npos) << out;
  EXPECT_EQ(out.find("id: C:"), std::string::npos) << "the id names the entry, so it must not also be listed as a field";
}

TEST(FactsRenderer, ARecordWithoutAnIdStillRenders) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  PB::Facts::Field *volumes = document_of(payload)->add_fields();
  volumes->set_key("volumes");
  add_string(volumes->mutable_value()->mutable_list_value()->add_values()->mutable_object_value(), "fs", "NTFS");

  // The core rejects a record without an id, so this is only reachable from a
  // subtree of something hand-built - but a renderer that drops the entry
  // would hide it rather than show the problem.
  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("- (no id)"), std::string::npos) << out;
}

TEST(FactsRenderer, StringListsAndEmptyListsRead) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  PB::Facts::Object *agent = document_of(payload);
  PB::Facts::Field *modules = agent->add_fields();
  modules->set_key("modules");
  modules->mutable_value()->mutable_list_value()->add_values()->set_string_value("CheckSystem");
  modules->mutable_value()->mutable_list_value()->add_values()->set_string_value("CheckDisk");
  PB::Facts::Field *none = agent->add_fields();
  none->set_key("hotfixes");
  none->mutable_value()->mutable_list_value();

  const std::string out = client::render_facts(message.SerializeAsString(), "");
  EXPECT_NE(out.find("\n  modules:\n    - CheckSystem\n    - CheckDisk"), std::string::npos) << out;
  EXPECT_NE(out.find("\n  hotfixes: (none)"), std::string::npos) << "an empty list says so rather than leaving a dangling heading";
}

TEST(FactsRenderer, ASubtreeIsLabelledWithItsPath) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_path("os");
  payload->set_found(true);
  add_string(payload->mutable_facts()->mutable_object_value(), "family", "windows");

  const std::string out = client::render_facts(message.SerializeAsString(), "os");
  EXPECT_NE(out.find("\nos:\n  family: windows"), std::string::npos) << out;
}

TEST(FactsRenderer, AScalarSubtreeRendersOnItsOwn) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_path("os.family");
  payload->set_found(true);
  payload->mutable_facts()->set_string_value("windows");

  // A path may address any node, so the response carries a Value and not just
  // an object - and a scalar belongs on the path's own line.
  const std::string out = client::render_facts(message.SerializeAsString(), "os.family");
  EXPECT_NE(out.find("\nos.family: windows"), std::string::npos) << out;
}

TEST(FactsRenderer, APathNothingProducedSaysSo) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->set_path("os.family");
  payload->set_found(false);

  EXPECT_EQ(client::render_facts(message.SerializeAsString(), "os.family"), "No facts at: os.family");
}

TEST(FactsRenderer, AnAnswerItCannotReadSaysSoRatherThanPrintingNothing) {
  // A message with no payload has no fields set at all, so it serialises to
  // nothing and is the same case as a core that does not know the call.
  EXPECT_EQ(PB::Facts::FactsResponseMessage().SerializeAsString(), "");
  EXPECT_EQ(client::render_facts("", ""), "This core does not serve facts.");
  EXPECT_EQ(client::render_facts(std::string("\xff\xff\xff\xff junk", 10), ""), "Could not read the facts the core returned.");
}

TEST(FactsRenderer, AnErrorFromTheCoreIsReported) {
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = message.add_payload();
  payload->mutable_result()->set_code(PB::Common::Result_StatusCodeType_STATUS_ERROR);
  payload->mutable_result()->set_message("the repository is not configured");

  EXPECT_EQ(client::render_facts(message.SerializeAsString(), ""), "Could not read the facts: the repository is not configured");
}

namespace {
client::fact_set_switch switch_for(const std::string &id, const std::string &producer, const bool configured) {
  client::fact_set_switch set;
  set.id = id;
  set.producer = producer;
  set.section = producer.empty() ? "/settings/facts" : "/settings/" + producer + "/facts";
  set.configured = configured;
  return set;
}

// The STATE cell of the row for `id`, read back out of the rendered table.
std::string state_of(const std::string &out, const std::string &id) {
  const std::string::size_type row = out.find("\n" + id + " ");
  if (row == std::string::npos) return "(no row)";
  const std::string::size_type end = out.find('\n', row + 1);
  std::string line = out.substr(row + 1, end == std::string::npos ? std::string::npos : end - row - 1);
  line = line.substr(id.size());
  const std::string::size_type start = line.find_first_not_of(' ');
  const std::string::size_type stop = line.find("  ", start);
  return line.substr(start, stop - start);
}
}  // namespace

TEST(FactSetList, NoProducerLoadedSaysWhatToLoad) {
  const std::string out = client::render_fact_sets({}, "");
  EXPECT_NE(out.find("No module that produces facts is loaded"), std::string::npos) << out;
}

TEST(FactSetList, ADefaultConfigListsEverySetAsDisabled) {
  PB::Facts::FactsResponseMessage message;
  document_of(start(message));
  const std::string out = client::render_fact_sets({switch_for("os", "CheckSystem", false), switch_for("agent", "", false)}, message.SerializeAsString());
  EXPECT_EQ(out.substr(0, out.find('\n')).find("SET"), 0u) << out;
  EXPECT_EQ(state_of(out, "os"), "disabled") << out;
  EXPECT_EQ(state_of(out, "agent"), "disabled") << out;
  // The core registers `agent` itself and has no module name to report.
  EXPECT_NE(out.find("core"), std::string::npos) << out;
}

TEST(FactSetList, SaysWhenTheConfigurationAndTheLastRoundDisagree) {
  // What the core reports is the top-level set each producer wrote, not the
  // switch: `storage.volumes` lands in `storage`.
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->add_enabled("os");
  payload->add_enabled("hardware");
  payload->add_enabled("storage");
  PB::Common::KeyValue *error = payload->add_errors();
  error->set_key("storage");
  error->set_value("access denied");
  document_of(payload);

  const std::string out = client::render_fact_sets(
      {
          switch_for("os", "CheckSystem", true),
          switch_for("hardware", "CheckSystem", false),
          switch_for("network.interfaces", "CheckSystem", true),
          switch_for("storage.volumes", "CheckDisk", true),
      },
      message.SerializeAsString());
  EXPECT_EQ(state_of(out, "os"), "enabled") << out;
  EXPECT_EQ(state_of(out, "hardware"), "disabled (reload to stop)") << out;
  EXPECT_EQ(state_of(out, "network.interfaces"), "enabled (reload to start)") << out;
  EXPECT_EQ(state_of(out, "storage.volumes"), "enabled, failing: access denied") << out;
  // Grouped by producer: CheckDisk's row comes before CheckSystem's.
  EXPECT_LT(out.find("storage.volumes"), out.find("\nhardware")) << out;
}

TEST(FactSetList, ASiblingSwitchKeepsTheSharedSetWithoutAReload) {
  // `docker` and `docker.containers` both land in the `docker` set. With
  // only the containers switch on, the core holding `docker` is not a sign
  // that the summary switch waits on a reload.
  PB::Facts::FactsResponseMessage message;
  PB::Facts::FactsResponseMessage::Response *payload = start(message);
  payload->add_enabled("docker");
  document_of(payload);

  const std::string out =
      client::render_fact_sets({switch_for("docker", "CheckDocker", false), switch_for("docker.containers", "CheckDocker", true)}, message.SerializeAsString());
  EXPECT_EQ(state_of(out, "docker"), "disabled") << out;
  EXPECT_EQ(state_of(out, "docker.containers"), "enabled") << out;
}

TEST(FactSetList, WithoutARoundTheConfigurationIsAllThereIsToSay) {
  const std::string out = client::render_fact_sets({switch_for("os", "CheckSystem", true)}, "");
  EXPECT_EQ(state_of(out, "os"), "enabled") << out;
}
