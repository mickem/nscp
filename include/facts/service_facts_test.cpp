// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <facts/service_facts.hpp>
#include <nscapi/nscapi_facts_test_helper.hpp>

using namespace nscapi::facts::testing;

TEST(service_facts, stable_identity_order_and_native_startup_modes) {
  const std::vector<service_facts::service> services = {
      {"z", "Stopped disabled service", "disabled"}, {"sshd", "SSH", "enabled"}, {"Spooler", "Print Spooler", "delayed_trigger"}, {"sshd", "SSH", "enabled"}};
  nscapi::facts::response out;
  service_facts::publish(services, 1700000000, out);
  const std::string json = json_of(out, "services");
  EXPECT_NE(json.find("\"id\":\"Spooler\""), std::string::npos);
  EXPECT_LT(json.find("\"id\":\"Spooler\""), json.find("\"id\":\"sshd\""));
  EXPECT_NE(json.find("\"start_type\":\"disabled\""), std::string::npos);
  auto reversed = services;
  std::reverse(reversed.begin(), reversed.end());
  nscapi::facts::response again;
  service_facts::publish(reversed, 1700000001, again);
  EXPECT_EQ(json, json_of(again, "services"));
  EXPECT_EQ(gathered_of(out, "services"), "2023-11-14T22:13:20Z");
  EXPECT_EQ(error_of(out, "services"), "");
}

TEST(service_facts, unknown_fields_are_omitted_and_empty_inventory_is_explicit) {
  nscapi::facts::response out;
  service_facts::publish({{"demo", "", ""}}, 0, out);
  EXPECT_EQ(json_of(out, "services"), "{\"installed\":[{\"id\":\"demo\",\"name\":\"demo\"}]}");
  nscapi::facts::response empty;
  service_facts::publish({}, 0, empty);
  EXPECT_EQ(json_of(empty, "services"), "{\"installed\":[]}");
}

TEST(service_facts, record_limit_reports_truncation) {
  std::vector<service_facts::service> services;
  for (std::size_t i = 0; i <= service_facts::max_services; ++i) services.push_back({"svc" + std::to_string(i), "", "disabled"});
  nscapi::facts::response out;
  service_facts::publish(services, 1700000000, out);
  const auto message = out.to_message();
  const auto *set = find_set(message, "services");
  ASSERT_NE(set, nullptr);
  EXPECT_EQ(nscapi::facts::tree::get(set->facts(), "installed")->list_value().values_size(), service_facts::max_services);
  EXPECT_NE(error_of(out, "services").find("2501"), std::string::npos);
}
