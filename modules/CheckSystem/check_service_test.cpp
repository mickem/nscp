/*
 * Copyright (C) 2004-2026 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "check_service.h"

#include <gtest/gtest.h>

#include <nsclient/nsclient_exception.hpp>
#include <win/winsvc.hpp>

// ============================================================================
// check_state_is_perfect tests
// ============================================================================

TEST(CheckServiceStateIsPerfect, BootStartRunningIsPerfect) {
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_BOOT_START, false));
}

TEST(CheckServiceStateIsPerfect, BootStartStoppedIsNotPerfect) {
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_BOOT_START, false));
}

TEST(CheckServiceStateIsPerfect, SystemStartRunningIsPerfect) {
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_SYSTEM_START, false));
}

TEST(CheckServiceStateIsPerfect, SystemStartStoppedIsNotPerfect) {
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_SYSTEM_START, false));
}

TEST(CheckServiceStateIsPerfect, AutoStartRunningIsPerfect) {
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_AUTO_START, false));
}

TEST(CheckServiceStateIsPerfect, AutoStartStoppedIsNotPerfect) {
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_AUTO_START, false));
}

TEST(CheckServiceStateIsPerfect, AutoStartWithTriggerAlwaysPerfect) {
  // Trigger services are always considered perfect for auto-start
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_AUTO_START, true));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_AUTO_START, true));
}

TEST(CheckServiceStateIsPerfect, DemandStartAlwaysPerfect) {
  // Demand-start services are always perfect regardless of state
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_DEMAND_START, false));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_DEMAND_START, false));
}

TEST(CheckServiceStateIsPerfect, DisabledStoppedIsPerfect) {
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_STOPPED, SERVICE_DISABLED, false));
}

TEST(CheckServiceStateIsPerfect, DisabledRunningIsNotPerfect) {
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, SERVICE_DISABLED, false));
}

TEST(CheckServiceStateIsPerfect, UnknownStartTypeIsNotPerfect) {
  // Unknown start type should return false
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_perfect(SERVICE_RUNNING, 999, false));
}

// ============================================================================
// check_state_is_ok tests
// ============================================================================

TEST(CheckServiceStateIsOk, PerfectStateIsAlsoOk) {
  // If state is perfect, it should also be ok
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_RUNNING, SERVICE_BOOT_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_RUNNING, SERVICE_SYSTEM_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_RUNNING, SERVICE_AUTO_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_DEMAND_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_DISABLED, false, false, 0));
}

TEST(CheckServiceStateIsOk, StartPendingIsOkForAutoServices) {
  // Services that are starting are considered ok for boot/system/auto start types
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_START_PENDING, SERVICE_BOOT_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_START_PENDING, SERVICE_SYSTEM_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_START_PENDING, SERVICE_AUTO_START, false, false, 0));
}

TEST(CheckServiceStateIsOk, StartPendingNotOkForDemandStart) {
  // Start pending for demand-start should still be ok (demand start is always perfect)
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_START_PENDING, SERVICE_DEMAND_START, false, false, 0));
}

TEST(CheckServiceStateIsOk, DelayedServiceIsOk) {
  // Delayed services are ok even if stopped for auto-start types
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_AUTO_START, true, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_BOOT_START, true, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_SYSTEM_START, true, false, 0));
}

TEST(CheckServiceStateIsOk, TriggerServiceIsOk) {
  // Trigger services are ok even if stopped for auto-start types
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_AUTO_START, false, true, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_BOOT_START, false, true, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_SYSTEM_START, false, true, 0));
}

TEST(CheckServiceStateIsOk, StoppedWithZeroExitCodeIsOk) {
  // Stopped services with exit code 0 are ok for auto-start types
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_AUTO_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_BOOT_START, false, false, 0));
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_SYSTEM_START, false, false, 0));
}

TEST(CheckServiceStateIsOk, StoppedWithNonZeroExitCodeIsNotOk) {
  // Stopped services with non-zero exit code are not ok (unless delayed/trigger)
  // Note: check_state_is_ok first checks delayed/trigger, then exit_code=0, then falls through to perfect
  // For auto-start, stopped is not perfect unless trigger=true
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_AUTO_START, false, false, 1));
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_BOOT_START, false, false, 1));
}

TEST(CheckServiceStateIsOk, DelayedNotOkForDemandStart) {
  // Delayed flag doesn't help demand-start services (but demand start is always perfect anyway)
  EXPECT_TRUE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOPPED, SERVICE_DEMAND_START, true, false, 0));
}

TEST(CheckServiceStateIsOk, DisabledRunningIsNotOk) {
  // Disabled service that is running is not ok
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_RUNNING, SERVICE_DISABLED, false, false, 0));
}

TEST(CheckServiceStateIsOk, PausedStateIsNotOk) {
  // Paused services are not ok for auto-start
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_PAUSED, SERVICE_AUTO_START, false, false, 0));
}

TEST(CheckServiceStateIsOk, StopPendingIsNotOk) {
  // Stop pending services are not ok for auto-start
  EXPECT_FALSE(service_checks::check_svc_filter::check_state_is_ok(SERVICE_STOP_PENDING, SERVICE_AUTO_START, false, false, 0));
}

// ============================================================================
// service_info struct tests (via win_list_services)
// ============================================================================

TEST(ServiceInfo, Construction) {
  win_list_services::service_info info("TestService", "Test Service Display Name");
  EXPECT_EQ(info.get_name(), "TestService");
  EXPECT_EQ(info.get_desc(), "Test Service Display Name");
  EXPECT_EQ(info.pid, 0);
  EXPECT_EQ(info.state, 0);
  EXPECT_EQ(info.start_type, 0);
  EXPECT_FALSE(info.delayed);
  EXPECT_EQ(info.triggers, 0);
  EXPECT_EQ(info.exit_code, 0);
}

TEST(ServiceInfo, CopyConstruction) {
  win_list_services::service_info info("Svc", "Service");
  info.pid = 1234;
  info.state = SERVICE_RUNNING;
  info.start_type = SERVICE_AUTO_START;
  info.delayed = true;
  info.triggers = 2;
  info.exit_code = 0;

  win_list_services::service_info copy(info);
  EXPECT_EQ(copy.get_name(), "Svc");
  EXPECT_EQ(copy.pid, 1234);
  EXPECT_EQ(copy.state, SERVICE_RUNNING);
  EXPECT_EQ(copy.start_type, SERVICE_AUTO_START);
  EXPECT_TRUE(copy.delayed);
  EXPECT_EQ(copy.triggers, 2);
}

TEST(ServiceInfo, GetStateString) {
  win_list_services::service_info info("Svc", "Service");

  info.state = SERVICE_RUNNING;
  EXPECT_EQ(info.get_state_s(), "running");

  info.state = SERVICE_STOPPED;
  EXPECT_EQ(info.get_state_s(), "stopped");

  info.state = SERVICE_START_PENDING;
  EXPECT_EQ(info.get_state_s(), "starting");

  info.state = SERVICE_STOP_PENDING;
  EXPECT_EQ(info.get_state_s(), "stopping");

  info.state = SERVICE_PAUSED;
  EXPECT_EQ(info.get_state_s(), "paused");

  info.state = SERVICE_PAUSE_PENDING;
  EXPECT_EQ(info.get_state_s(), "pausing");

  info.state = SERVICE_CONTINUE_PENDING;
  EXPECT_EQ(info.get_state_s(), "continuing");

  info.state = 999;
  EXPECT_EQ(info.get_state_s(), "unknown");
}

TEST(ServiceInfo, GetLegacyStateString) {
  win_list_services::service_info info("Svc", "Service");

  info.state = SERVICE_RUNNING;
  EXPECT_EQ(info.get_legacy_state_s(), "Started");

  info.state = SERVICE_STOPPED;
  EXPECT_EQ(info.get_legacy_state_s(), "Stopped");
}

TEST(ServiceInfo, GetStartTypeString) {
  win_list_services::service_info info("Svc", "Service");

  info.start_type = SERVICE_AUTO_START;
  info.delayed = false;
  info.triggers = 0;
  EXPECT_EQ(info.get_start_type_s(), "auto");

  info.triggers = 1;
  EXPECT_EQ(info.get_start_type_s(), "auto_trigger");

  info.delayed = true;
  info.triggers = 0;
  EXPECT_EQ(info.get_start_type_s(), "delayed");

  info.triggers = 1;
  EXPECT_EQ(info.get_start_type_s(), "delayed_trigger");

  info.delayed = false;
  info.triggers = 0;
  info.start_type = SERVICE_DEMAND_START;
  EXPECT_EQ(info.get_start_type_s(), "demand");

  info.start_type = SERVICE_DISABLED;
  EXPECT_EQ(info.get_start_type_s(), "disabled");

  info.start_type = SERVICE_BOOT_START;
  EXPECT_EQ(info.get_start_type_s(), "boot");

  info.start_type = SERVICE_SYSTEM_START;
  EXPECT_EQ(info.get_start_type_s(), "system");
}

TEST(ServiceInfo, ParseState) {
  EXPECT_EQ(win_list_services::service_info::parse_state("running"), SERVICE_RUNNING);
  EXPECT_EQ(win_list_services::service_info::parse_state("started"), SERVICE_RUNNING);
  EXPECT_EQ(win_list_services::service_info::parse_state("stopped"), SERVICE_STOPPED);
  EXPECT_EQ(win_list_services::service_info::parse_state("starting"), SERVICE_START_PENDING);
  EXPECT_EQ(win_list_services::service_info::parse_state("stopping"), SERVICE_STOP_PENDING);
  EXPECT_EQ(win_list_services::service_info::parse_state("paused"), SERVICE_PAUSED);
  EXPECT_EQ(win_list_services::service_info::parse_state("pausing"), SERVICE_PAUSE_PENDING);
  EXPECT_EQ(win_list_services::service_info::parse_state("continuing"), SERVICE_CONTINUE_PENDING);
}

TEST(ServiceInfo, ParseStateInvalidThrows) { EXPECT_THROW(win_list_services::service_info::parse_state("invalid"), nsclient::nsclient_exception); }

TEST(ServiceInfo, ParseStartType) {
  EXPECT_EQ(win_list_services::service_info::parse_start_type("auto"), SERVICE_AUTO_START);
  EXPECT_EQ(win_list_services::service_info::parse_start_type("boot"), SERVICE_BOOT_START);
  EXPECT_EQ(win_list_services::service_info::parse_start_type("demand"), SERVICE_DEMAND_START);
  EXPECT_EQ(win_list_services::service_info::parse_start_type("disabled"), SERVICE_DISABLED);
  EXPECT_EQ(win_list_services::service_info::parse_start_type("system"), SERVICE_SYSTEM_START);
}

TEST(ServiceInfo, ParseStartTypeInvalidThrows) { EXPECT_THROW(win_list_services::service_info::parse_start_type("invalid"), nsclient::nsclient_exception); }

TEST(ServiceInfo, GetDelayed) {
  win_list_services::service_info info("Svc", "Service");
  info.delayed = false;
  EXPECT_EQ(info.get_delayed(), 0);
  info.delayed = true;
  EXPECT_EQ(info.get_delayed(), 1);
}

TEST(ServiceInfo, GetIsTrigger) {
  win_list_services::service_info info("Svc", "Service");
  info.triggers = 0;
  EXPECT_EQ(info.get_is_trigger(), 0);
  info.triggers = 1;
  EXPECT_EQ(info.get_is_trigger(), 1);
  info.triggers = 5;
  EXPECT_EQ(info.get_is_trigger(), 1);
}

TEST(ServiceInfo, ShowFormat) {
  win_list_services::service_info info("TestSvc", "Test Service");
  info.state = SERVICE_RUNNING;
  info.start_type = SERVICE_AUTO_START;
  info.delayed = false;
  info.triggers = 0;
  info.exit_code = 0;

  std::string s = info.show();
  EXPECT_NE(s.find("TestSvc"), std::string::npos);
  EXPECT_NE(s.find("Test Service"), std::string::npos);
  EXPECT_NE(s.find("running"), std::string::npos);
  EXPECT_NE(s.find("auto"), std::string::npos);
}

// ============================================================================
// parse_service_type and parse_service_state tests
// ============================================================================

TEST(ParseServiceType, SingleTypes) {
  EXPECT_EQ(win_list_services::parse_service_type("service"), SERVICE_WIN32);
  EXPECT_EQ(win_list_services::parse_service_type("svc"), SERVICE_WIN32);
  EXPECT_EQ(win_list_services::parse_service_type("driver"), SERVICE_DRIVER);
  EXPECT_EQ(win_list_services::parse_service_type("drv"), SERVICE_DRIVER);
}

TEST(ParseServiceType, CombinedTypes) {
  DWORD result = win_list_services::parse_service_type("service,driver");
  EXPECT_TRUE(result & SERVICE_WIN32);
  EXPECT_TRUE(result & SERVICE_DRIVER);
}

TEST(ParseServiceType, InvalidTypeThrows) { EXPECT_THROW(win_list_services::parse_service_type("invalid"), nsclient::nsclient_exception); }

TEST(ParseServiceState, AllStates) {
  EXPECT_EQ(win_list_services::parse_service_state("all"), SERVICE_STATE_ALL);
  EXPECT_EQ(win_list_services::parse_service_state("active"), SERVICE_ACTIVE);
  EXPECT_EQ(win_list_services::parse_service_state("inactive"), SERVICE_INACTIVE);
}

TEST(ParseServiceState, InvalidStateThrows) { EXPECT_THROW(win_list_services::parse_service_state("invalid"), nsclient::nsclient_exception); }

// ============================================================================
// get_service_classification tests
// ============================================================================

TEST(ServiceClassification, EssentialServices) {
  win_list_services::init();
  EXPECT_EQ(win_list_services::get_service_classification("EventLog"), "essential");
  EXPECT_EQ(win_list_services::get_service_classification("RpcSs"), "essential");
  EXPECT_EQ(win_list_services::get_service_classification("Winmgmt"), "essential");
}

TEST(ServiceClassification, SystemServices) {
  win_list_services::init();
  EXPECT_EQ(win_list_services::get_service_classification("DcomLaunch"), "system");
  EXPECT_EQ(win_list_services::get_service_classification("Power"), "system");
}

TEST(ServiceClassification, IgnoredServices) {
  win_list_services::init();
  EXPECT_EQ(win_list_services::get_service_classification("VSS"), "ignored");
  EXPECT_EQ(win_list_services::get_service_classification("msiserver"), "ignored");
}

TEST(ServiceClassification, UnknownServiceIsCustom) {
  win_list_services::init();
  EXPECT_EQ(win_list_services::get_service_classification("MyCustomService"), "custom");
}
