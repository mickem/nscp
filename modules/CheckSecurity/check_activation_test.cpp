// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The licensing decode of check_activation: LicenseStatus / SL_GENUINE_STATE
// wording and the derived fields the thresholds are written against.

#include <gtest/gtest.h>

#include <nscapi/nscapi_helper_singleton.hpp>

#include "check_activation.hpp"

// Normally provided by NSC_WRAP_DLL() in the auto-generated module.cpp; in the
// test binary there is no generated module, so define the singleton here.
nscapi::helper_singleton *nscapi::plugin_singleton = new nscapi::helper_singleton();

// The data source is the platform-specific half of the check (WMI + slc.dll) and
// is exercised by the integration suite instead; the command object still needs
// it to link, so stand it in here.
namespace activation_source {
void gather(bool, bool, std::vector<activation_filter::filter_obj_ptr> &, std::string &error) { error = "not available in the unit test"; }
}  // namespace activation_source

namespace {

activation_filter::filter_obj windows_product(const long long status, const long long grace_minutes) {
  activation_filter::filter_obj obj;
  obj.name = "Windows(R), Professional edition";
  obj.app_id = activation_filter::windows_application_id;
  obj.key = "3V66T";
  obj.license_status = status;
  obj.grace_minutes = grace_minutes;
  return obj;
}

}  // namespace

TEST(check_activation, license_status_words) {
  EXPECT_EQ("unlicensed", activation_filter::license_status_name(0));
  EXPECT_EQ("licensed", activation_filter::license_status_name(1));
  EXPECT_EQ("initial_grace", activation_filter::license_status_name(2));
  EXPECT_EQ("additional_grace", activation_filter::license_status_name(3));
  EXPECT_EQ("non_genuine_grace", activation_filter::license_status_name(4));
  EXPECT_EQ("notification", activation_filter::license_status_name(5));
  EXPECT_EQ("extended_grace", activation_filter::license_status_name(6));
  EXPECT_EQ("unknown", activation_filter::license_status_name(42));
}

TEST(check_activation, genuine_state_words) {
  EXPECT_EQ("genuine", activation_filter::genuine_state_name(0));
  EXPECT_EQ("invalid_license", activation_filter::genuine_state_name(1));
  EXPECT_EQ("tampered", activation_filter::genuine_state_name(2));
  EXPECT_EQ("offline", activation_filter::genuine_state_name(3));
  EXPECT_EQ("unknown", activation_filter::genuine_state_name(activation_filter::genuine_unknown));
}

TEST(check_activation, grace_minutes_render_as_whole_days) {
  EXPECT_EQ(0, activation_filter::grace_minutes_to_days(0));
  EXPECT_EQ(0, activation_filter::grace_minutes_to_days(-1));
  EXPECT_EQ(0, activation_filter::grace_minutes_to_days(1439));  // just under a day
  EXPECT_EQ(1, activation_filter::grace_minutes_to_days(1440));
  EXPECT_EQ(180, activation_filter::grace_minutes_to_days(180 * 24 * 60));
}

TEST(check_activation, activated_windows_is_licensed_without_a_countdown) {
  activation_filter::filter_obj obj = windows_product(activation_filter::status_licensed, 0);
  obj.genuine_status = activation_filter::genuine_is_genuine;
  activation_filter::finalize(obj);

  EXPECT_EQ(1, obj.get_licensed());
  EXPECT_EQ(1, obj.get_genuine());
  EXPECT_EQ(1, obj.get_is_windows());
  EXPECT_EQ(0, obj.get_grace_days());
  EXPECT_EQ("licensed", obj.get_status());
  EXPECT_EQ("genuine", obj.get_genuine_state());
}

TEST(check_activation, grace_period_is_not_licensed) {
  // A machine inside the out-of-box grace period: still usable, but counting down.
  activation_filter::filter_obj obj = windows_product(activation_filter::status_oob_grace, 12 * 24 * 60);
  activation_filter::finalize(obj);

  EXPECT_EQ(0, obj.get_licensed());
  EXPECT_EQ(12, obj.get_grace_days());
  EXPECT_EQ("initial_grace", obj.get_status());
}

TEST(check_activation, unknown_genuine_state_is_not_reported_as_genuine) {
  activation_filter::filter_obj obj = windows_product(activation_filter::status_licensed, 0);
  obj.genuine_status = activation_filter::genuine_unknown;
  activation_filter::finalize(obj);

  EXPECT_EQ(0, obj.get_genuine());
  EXPECT_EQ("unknown", obj.get_genuine_state());
}

TEST(check_activation, non_windows_products_are_flagged_as_such) {
  activation_filter::filter_obj obj = windows_product(activation_filter::status_licensed, 0);
  obj.app_id = "0ff1ce15-a989-479d-af46-f275c6370663";  // Office
  activation_filter::finalize(obj);

  EXPECT_EQ(0, obj.get_is_windows());
}
