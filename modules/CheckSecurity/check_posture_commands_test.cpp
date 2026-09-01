// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The command bodies of the eight posture checks whose data acquisition is
// Windows-only: check_nla, check_antivirus, check_bitlocker, check_secureboot,
// check_defender, check_local_accounts, check_group_members and check_users.
//
// Only the gathering is platform-specific. Everything the command does around
// it - option parsing, the keyword registry, the default warning/critical
// expressions, the empty-state contract, the rendering and the perf data - is
// the same code the win32 build runs, and none of it was covered anywhere: the
// existing unit tests stop at the filter_obj accessors, and the integration
// suite can only reach the "not supported on this platform" branch, which
// returns before a single row is ever matched.
//
// So stand in for the platform source and hand the commands rows. Each
// *_source::gather below reads a per-check fixture the test sets first, which
// is what lets the *matched* half of these checks - the half that decides
// whether a host is reported healthy or critical - run here.
//
// nscapi::plugin_singleton is defined once for this target in
// check_activation_test.cpp.

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "check_antivirus.hpp"
#include "check_bitlocker.hpp"
#include "check_defender.hpp"
#include "check_group_members.hpp"
#include "check_local_accounts.hpp"
#include "check_nla.hpp"
#include "check_secureboot.hpp"
#include "check_users.hpp"

namespace {

// One fixture per source: the rows the next gather() hands back, and an error
// to report instead when it is set.
template <typename T>
struct source_fixture {
  std::vector<std::shared_ptr<T>> rows;
  std::string error;

  void reset() {
    rows.clear();
    error.clear();
  }
  std::shared_ptr<T> add() {
    auto row = std::make_shared<T>();
    rows.push_back(row);
    return row;
  }
};

source_fixture<nla_filter::filter_obj> nla_rows;
source_fixture<antivirus_filter::filter_obj> antivirus_rows;
source_fixture<bitlocker_filter::filter_obj> bitlocker_rows;
source_fixture<secureboot_filter::filter_obj> secureboot_rows;
source_fixture<defender_filter::filter_obj> defender_rows;
source_fixture<local_accounts_filter::filter_obj> local_account_rows;
source_fixture<group_members_filter::filter_obj> group_member_rows;
source_fixture<users_filter::filter_obj> user_rows;

// The group check_group_members was asked about, so the option plumbing can be
// asserted on rather than assumed.
std::string requested_group;

std::string join_lines(const PB::Commands::QueryResponseMessage::Response &r) {
  std::string out;
  for (int i = 0; i < r.lines_size(); ++i) {
    if (!out.empty()) out += "\n";
    out += r.lines(i).message();
  }
  return out;
}

std::vector<std::string> perf_labels(const PB::Commands::QueryResponseMessage::Response &r) {
  std::vector<std::string> out;
  for (int i = 0; i < r.lines_size(); ++i) {
    for (const auto &p : r.lines(i).perf()) out.push_back(p.alias());
  }
  return out;
}

PB::Commands::QueryRequestMessage::Request request_for(const std::string &command, const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command(command);
  for (const std::string &a : args) request.add_arguments(a);
  return request;
}

}  // namespace

// The stand-in data sources. Non-const references, matching the real ones.
namespace nla_source {
void gather(std::vector<nla_filter::filter_obj_ptr> &out, std::string &error) {
  error = nla_rows.error;
  out = nla_rows.rows;
}
}  // namespace nla_source

namespace antivirus_source {
void gather(std::vector<antivirus_filter::filter_obj_ptr> &out, std::string &error) {
  error = antivirus_rows.error;
  out = antivirus_rows.rows;
}
}  // namespace antivirus_source

namespace bitlocker_source {
void gather(std::vector<bitlocker_filter::filter_obj_ptr> &out, std::string &error) {
  error = bitlocker_rows.error;
  out = bitlocker_rows.rows;
}
}  // namespace bitlocker_source

namespace secureboot_source {
void gather(std::vector<secureboot_filter::filter_obj_ptr> &out, std::string &error) {
  error = secureboot_rows.error;
  out = secureboot_rows.rows;
}
}  // namespace secureboot_source

namespace defender_source {
void gather(std::vector<defender_filter::filter_obj_ptr> &out, std::string &error) {
  error = defender_rows.error;
  out = defender_rows.rows;
}
}  // namespace defender_source

namespace local_accounts_source {
void gather(std::vector<local_accounts_filter::filter_obj_ptr> &out, std::string &error) {
  error = local_account_rows.error;
  out = local_account_rows.rows;
}
}  // namespace local_accounts_source

namespace group_members_source {
void gather(const std::string &group, std::vector<group_members_filter::filter_obj_ptr> &out, std::string &error) {
  requested_group = group;
  error = group_member_rows.error;
  out = group_member_rows.rows;
}
}  // namespace group_members_source

namespace users_source {
void gather(std::vector<users_filter::filter_obj_ptr> &out, std::string &error) {
  error = user_rows.error;
  out = user_rows.rows;
}
}  // namespace users_source

namespace {

// Every test starts from empty fixtures; a leftover row from a previous case
// would otherwise decide the next one's status.
class posture_command : public ::testing::Test {
 protected:
  void SetUp() override {
    nla_rows.reset();
    antivirus_rows.reset();
    bitlocker_rows.reset();
    secureboot_rows.reset();
    defender_rows.reset();
    local_account_rows.reset();
    group_member_rows.reset();
    user_rows.reset();
    requested_group.clear();
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// check_nla - no default alert; the operator asserts the expected posture.
// ---------------------------------------------------------------------------

TEST_F(posture_command, nla_reports_the_source_error_verbatim) {
  // This is the branch the integration suite reaches on Unix: the platform has
  // no Network Location Awareness, and the check must say so rather than claim
  // a healthy posture.
  nla_rows.error = "check_nla is not supported on this platform";
  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("not supported on this platform"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, nla_without_networks_is_ok_and_says_so) {
  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No networks found"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, nla_without_a_threshold_never_alerts) {
  // A public network is not by itself a fault - plenty of hosts are meant to be
  // on one - so the check ships with no default expression at all.
  auto network = nla_rows.add();
  network->network = "Guest Wifi";
  network->category = "public";
  network->connected = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, nla_alerts_on_the_posture_the_operator_asserts) {
  auto corp = nla_rows.add();
  corp->network = "Corp";
  corp->category = "domain";
  corp->connected = 1;
  auto guest = nla_rows.add();
  guest->network = "Guest Wifi";
  guest->category = "public";
  guest->connected = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {"crit=connected = 1 and category = 'public'"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  // The shipped top-syntax is ${list}, which is every network the filter kept -
  // both of them - each rendered through the detail syntax.
  EXPECT_NE(join_lines(response).find("Corp=domain"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Guest Wifi=public"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, nla_the_problem_list_holds_only_the_offending_network) {
  // ${list} is everything that matched the filter; ${problem_list} is the
  // subset that tripped a threshold. An operator who only wants to see what is
  // wrong asks for the latter.
  auto corp = nla_rows.add();
  corp->network = "Corp";
  corp->category = "domain";
  corp->connected = 1;
  auto guest = nla_rows.add();
  guest->network = "Guest Wifi";
  guest->category = "public";
  guest->connected = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(
      request_for("check_nla", {"crit=connected = 1 and category = 'public'", "top-syntax=${status}: ${problem_list}"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Guest Wifi=public"), std::string::npos) << join_lines(response);
  EXPECT_EQ(join_lines(response).find("Corp=domain"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, nla_filter_narrows_what_is_considered) {
  auto corp = nla_rows.add();
  corp->network = "Corp";
  corp->category = "domain";
  corp->connected = 1;
  auto stale = nla_rows.add();
  stale->network = "Old Guest Wifi";
  stale->category = "public";
  stale->connected = 0;

  // Disconnected networks are filtered out before the expression is evaluated,
  // so the remembered public network does not trip it.
  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {"filter=connected = 1", "crit=category = 'public'"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, nla_rejects_a_bad_filter_rather_than_ignoring_it) {
  PB::Commands::QueryResponseMessage::Response response;
  check_nla_command::check(request_for("check_nla", {"filter=no such keyword > 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_antivirus - critical when a registered product is off or stale.
// ---------------------------------------------------------------------------

TEST_F(posture_command, antivirus_without_a_product_is_unknown_not_ok) {
  // "No antivirus registered" is not the same as "antivirus is healthy", and
  // reporting OK there would hide an unprotected host.
  PB::Commands::QueryResponseMessage::Response response;
  check_antivirus_command::check(request_for("check_antivirus", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No antivirus product registered"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, antivirus_healthy_product_is_ok) {
  auto product = antivirus_rows.add();
  product->name = "Contoso AV";
  product->enabled = 1;
  product->up_to_date = 1;
  product->product_state = 0x061100;

  PB::Commands::QueryResponseMessage::Response response;
  check_antivirus_command::check(request_for("check_antivirus", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  // The shipped ok-syntax never renders: cli_helper drops it whenever the
  // top-syntax already contains a list, which this one does. The OK line is the
  // top-syntax with every matched product in it.
  EXPECT_NE(join_lines(response).find("OK: Contoso AV (enabled=1 up_to_date=1)"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, antivirus_disabled_product_is_critical_by_default) {
  auto product = antivirus_rows.add();
  product->name = "Contoso AV";
  product->enabled = 0;
  product->up_to_date = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_antivirus_command::check(request_for("check_antivirus", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Contoso AV (enabled=0 up_to_date=1)"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, antivirus_stale_definitions_are_critical_by_default) {
  auto product = antivirus_rows.add();
  product->name = "Contoso AV";
  product->enabled = 1;
  product->up_to_date = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_antivirus_command::check(request_for("check_antivirus", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST_F(posture_command, antivirus_one_bad_product_among_several_still_alerts) {
  auto good = antivirus_rows.add();
  good->name = "Contoso AV";
  good->enabled = 1;
  good->up_to_date = 1;
  auto bad = antivirus_rows.add();
  bad->name = "Fabrikam Shield";
  bad->enabled = 0;
  bad->up_to_date = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_antivirus_command::check(request_for("check_antivirus", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Fabrikam Shield"), std::string::npos) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_bitlocker - critical when a volume is not protected.
// ---------------------------------------------------------------------------

TEST_F(posture_command, bitlocker_without_volumes_is_unknown) {
  PB::Commands::QueryResponseMessage::Response response;
  check_bitlocker_command::check(request_for("check_bitlocker", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No encryptable volumes found"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, bitlocker_protected_volume_is_ok) {
  auto volume = bitlocker_rows.add();
  volume->drive = "C:";
  volume->protection_status = 1;
  volume->conversion_status = 1;
  volume->is_protected = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_bitlocker_command::check(request_for("check_bitlocker", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("OK: C: protected=1"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, bitlocker_unprotected_volume_is_critical_by_default) {
  auto encrypted = bitlocker_rows.add();
  encrypted->drive = "C:";
  encrypted->protection_status = 1;
  encrypted->is_protected = 1;
  auto plain = bitlocker_rows.add();
  plain->drive = "D:";
  plain->protection_status = 0;
  plain->is_protected = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_bitlocker_command::check(request_for("check_bitlocker", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("D: protected=0"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, bitlocker_conversion_status_is_available_to_expressions) {
  // A volume mid-encryption reports protection_status 1 but is not yet fully
  // converted; an operator that cares can say so through the raw keyword.
  auto volume = bitlocker_rows.add();
  volume->drive = "C:";
  volume->protection_status = 1;
  volume->conversion_status = 2;  // encrypting
  volume->is_protected = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_bitlocker_command::check(request_for("check_bitlocker", {"warn=conversion_status != 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_secureboot - critical when Secure Boot is off.
// ---------------------------------------------------------------------------

TEST_F(posture_command, secureboot_without_a_state_is_unknown) {
  PB::Commands::QueryResponseMessage::Response response;
  check_secureboot_command::check(request_for("check_secureboot", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No Secure Boot state"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, secureboot_enabled_is_ok) {
  auto state = secureboot_rows.add();
  state->enabled = 1;
  state->supported = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_secureboot_command::check(request_for("check_secureboot", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("secure boot enabled=1 supported=1"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, secureboot_disabled_is_critical_by_default) {
  auto state = secureboot_rows.add();
  state->enabled = 0;
  state->supported = 1;

  PB::Commands::QueryResponseMessage::Response response;
  check_secureboot_command::check(request_for("check_secureboot", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("secure boot enabled=0 supported=1"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, secureboot_a_legacy_bios_boot_can_be_excused) {
  // A BIOS (non-UEFI) machine cannot have Secure Boot at all. The default
  // expression still trips, which is why `supported` is a keyword: a fleet with
  // old hardware in it can exempt exactly those hosts.
  auto state = secureboot_rows.add();
  state->enabled = 0;
  state->supported = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_secureboot_command::check(request_for("check_secureboot", {"crit=supported = 1 and enabled = 0"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_defender - warning on ageing definitions, critical when protection is
// off or the definitions are badly stale.
// ---------------------------------------------------------------------------

TEST_F(posture_command, defender_without_rows_is_unknown_rather_than_an_error) {
  // Documented contract: when Defender is not the active antivirus the WMI
  // query returns no rows, and that is UNKNOWN via the empty state - not a
  // hard failure.
  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Microsoft Defender status unavailable"), std::string::npos) << join_lines(response);
}

namespace {

// A healthy Defender, which each case below then spoils in one specific way.
std::shared_ptr<defender_filter::filter_obj> healthy_defender() {
  auto row = defender_rows.add();
  row->enabled = 1;
  row->realtime_enabled = 1;
  row->tamper_protection = 1;
  row->signature_age = 1;
  row->quick_scan_age = 2;
  row->full_scan_age = 6;
  row->engine_version = "1.1.24010.10";
  row->signature_version = "1.403.1234.0";
  row->product_version = "4.18.24010.7";
  return row;
}

}  // namespace

TEST_F(posture_command, defender_healthy_is_ok_and_reports_the_signature_age) {
  healthy_defender();

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("sig_age=1d"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("sig=1.403.1234.0"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("engine=1.1.24010.10"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, defender_ageing_definitions_are_a_warning) {
  healthy_defender()->signature_age = 5;  // past warn (>3), short of crit (>7)

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("sig_age=5d"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, defender_badly_stale_definitions_are_critical) {
  healthy_defender()->signature_age = 30;

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST_F(posture_command, defender_realtime_protection_off_is_critical) {
  healthy_defender()->realtime_enabled = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("realtime=0"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, defender_unknown_ages_never_trip_the_defaults) {
  // -1 means "we could not find out". Treating that as 'very old' would alert
  // every host whose WMI schema is missing the field, so the defaults are
  // written as `> N` and a negative age has to stay below them.
  auto row = healthy_defender();
  row->signature_age = -1;
  row->quick_scan_age = -1;
  row->full_scan_age = -1;

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, defender_tamper_protection_is_available_to_expressions) {
  // Not part of the defaults (plenty of managed estates turn it off
  // deliberately), but a hardened fleet can require it.
  healthy_defender()->tamper_protection = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {"crit=tamper_protection = 0"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST_F(posture_command, defender_graphs_only_the_ages_its_thresholds_mention) {
  // Perf data follows the thresholds, not the keyword registry: only keywords
  // the warning/critical expressions actually reference are emitted. The
  // shipped defaults name enabled, realtime_enabled and signature_age - and the
  // first two are declared no_perf() because a boolean graphs to nothing
  // useful - so a default run graphs the signature age alone.
  healthy_defender();

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {}), &response);

  ASSERT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_EQ(perf_labels(response), std::vector<std::string>{"defender_signature_age"}) << join_lines(response);
}

TEST_F(posture_command, defender_graphs_the_scan_ages_once_a_threshold_uses_them) {
  // The other two ages carry perf configuration of their own; they simply need
  // an expression to mention them.
  healthy_defender();

  PB::Commands::QueryResponseMessage::Response response;
  check_defender_command::check(request_for("check_defender", {"warn=quick_scan_age > 7 or full_scan_age > 30"}), &response);

  ASSERT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  // Three now: the two the new warning names, plus the signature age the
  // untouched default critical still names. Overriding `warn` replaces only the
  // warning expression. The series are named after the check's perf alias
  // ("defender") plus the per-keyword suffix; their order is not part of the
  // contract, so compare them as a set.
  std::vector<std::string> labels = perf_labels(response);
  std::sort(labels.begin(), labels.end());
  EXPECT_EQ(labels, (std::vector<std::string>{"defender_full_scan_age", "defender_quick_scan_age", "defender_signature_age"})) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_local_accounts - built-in Guest enabled is a warning, an enabled
// account with no password required is critical.
// ---------------------------------------------------------------------------

TEST(check_local_accounts_rid, the_relative_id_is_the_last_sid_component) {
  EXPECT_EQ(500, local_accounts_filter::rid_from_sid("S-1-5-21-1004336348-1177238915-682003330-500"));
  EXPECT_EQ(501, local_accounts_filter::rid_from_sid("S-1-5-21-1004336348-1177238915-682003330-501"));
  EXPECT_EQ(1001, local_accounts_filter::rid_from_sid("S-1-5-21-1-2-3-1001"));
}

TEST(check_local_accounts_rid, an_unparsable_sid_is_minus_one_rather_than_a_throw) {
  // The SID arrives from WMI as a string and is not guaranteed to be one: an
  // exception escaping here would abort the whole check.
  EXPECT_EQ(-1, local_accounts_filter::rid_from_sid(""));
  EXPECT_EQ(-1, local_accounts_filter::rid_from_sid("S-1-5-21-1-2-3-"));  // trailing dash, no rid
  EXPECT_EQ(-1, local_accounts_filter::rid_from_sid("no-dashes-here"));   // std::stoll throws
  EXPECT_EQ(-1, local_accounts_filter::rid_from_sid("Administrator"));    // no dash at all
}

TEST(check_local_accounts_rid, well_known_rids_drive_the_builtin_flags) {
  local_accounts_filter::filter_obj account;
  account.rid = local_accounts_filter::rid_from_sid("S-1-5-21-1-2-3-500");
  EXPECT_EQ(1, account.get_is_builtin_admin());
  EXPECT_EQ(0, account.get_is_builtin_guest());

  account.rid = local_accounts_filter::rid_from_sid("S-1-5-21-1-2-3-501");
  EXPECT_EQ(0, account.get_is_builtin_admin());
  EXPECT_EQ(1, account.get_is_builtin_guest());

  // A renamed built-in keeps its rid, and an ordinary account is neither.
  account.rid = 1001;
  EXPECT_EQ(0, account.get_is_builtin_admin());
  EXPECT_EQ(0, account.get_is_builtin_guest());
}

TEST(check_local_accounts_rid, enabled_is_the_inverse_of_disabled) {
  local_accounts_filter::filter_obj account;
  account.disabled = 0;
  EXPECT_EQ(1, account.get_enabled());
  account.disabled = 1;
  EXPECT_EQ(0, account.get_enabled());
}

namespace {

std::shared_ptr<local_accounts_filter::filter_obj> account(const std::string &name, const long long rid, const long long disabled) {
  auto row = local_account_rows.add();
  row->name = name;
  row->sid = "S-1-5-21-1-2-3-" + std::to_string(rid);
  row->rid = rid;
  row->disabled = disabled;
  row->password_required = 1;
  row->password_expires = 1;
  return row;
}

}  // namespace

TEST_F(posture_command, local_accounts_without_accounts_is_ok_and_says_so) {
  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No local accounts found"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, local_accounts_a_hardened_host_is_ok) {
  account("Administrator", 500, /*disabled=*/1);
  account("Guest", 501, /*disabled=*/1);
  account("svc-backup", 1001, /*disabled=*/0);

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Administrator (enabled=0"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("svc-backup (enabled=1"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, local_accounts_an_enabled_guest_is_a_warning) {
  account("Guest", 501, /*disabled=*/0);

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::WARNING) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Guest (enabled=1"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, local_accounts_a_disabled_guest_is_not_a_warning) {
  account("Guest", 501, /*disabled=*/1);

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, local_accounts_an_enabled_passwordless_account_is_critical) {
  account("kiosk", 1002, /*disabled=*/0)->password_required = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("pw_req=0"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, local_accounts_a_disabled_passwordless_account_is_not_critical) {
  // Disabled accounts cannot be logged into, so the password policy on them is
  // not a live exposure - which is why both defaults are qualified with
  // `enabled = 1`.
  account("kiosk", 1002, /*disabled=*/1)->password_required = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, local_accounts_a_stricter_policy_can_be_expressed) {
  // The shipped defaults are deliberately low-false-positive; the remaining
  // keywords are what a stricter site writes its own policy against.
  account("Administrator", 500, /*disabled=*/0)->password_expires = 0;

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {"crit=is_builtin_admin = 1 and enabled = 1 and password_expires = 0"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST_F(posture_command, local_accounts_emits_the_account_count_as_perfdata) {
  account("Administrator", 500, /*disabled=*/1);
  account("svc-backup", 1001, /*disabled=*/0);

  PB::Commands::QueryResponseMessage::Response response;
  check_local_accounts_command::check(request_for("check_local_accounts", {}), &response);

  ASSERT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  // set_default_perf_config("extra(count)") adds the account tally on top of the
  // per-account series the default thresholds pull in (one per account for each
  // keyword the expressions mention), and it comes last.
  const std::vector<std::string> labels = perf_labels(response);
  ASSERT_FALSE(labels.empty()) << join_lines(response);
  EXPECT_EQ(labels.back(), "count") << join_lines(response);
  EXPECT_NE(std::find(labels.begin(), labels.end(), "Administrator_enabled"), labels.end()) << join_lines(response);
  EXPECT_NE(std::find(labels.begin(), labels.end(), "svc-backup_password_required"), labels.end()) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_group_members - membership drift against an expected= allow-list.
// ---------------------------------------------------------------------------

TEST(check_group_members_allow_list, matches_either_the_qualified_or_the_bare_name) {
  group_members_filter::filter_obj member;
  member.member = "BUILTIN\\Administrator";
  member.name = "Administrator";

  EXPECT_TRUE(group_members_filter::is_expected(member, {"BUILTIN\\Administrator"}));
  EXPECT_TRUE(group_members_filter::is_expected(member, {"Administrator"}));
  EXPECT_FALSE(group_members_filter::is_expected(member, {"CONTOSO\\Administrator"}));
}

TEST(check_group_members_allow_list, matching_is_case_insensitive) {
  // Windows account names are case-insensitive, so an allow-list that differs
  // only in case must not read as drift.
  group_members_filter::filter_obj member;
  member.member = "CONTOSO\\Domain Admins";
  member.name = "Domain Admins";

  EXPECT_TRUE(group_members_filter::is_expected(member, {"contoso\\domain admins"}));
  EXPECT_TRUE(group_members_filter::is_expected(member, {"DOMAIN ADMINS"}));
}

TEST(check_group_members_allow_list, an_empty_list_matches_nothing) {
  // is_expected() itself never treats "no list" as "everything allowed"; that
  // rule lives in the check, which skips the call entirely.
  group_members_filter::filter_obj member;
  member.member = "BUILTIN\\Administrator";
  member.name = "Administrator";

  EXPECT_FALSE(group_members_filter::is_expected(member, {}));
}

namespace {

std::shared_ptr<group_members_filter::filter_obj> group_member(const std::string &domain, const std::string &name, const std::string &type = "user") {
  auto row = group_member_rows.add();
  row->group = "Administrators";
  row->domain = domain;
  row->name = name;
  row->member = domain + "\\" + name;
  row->sid = "S-1-5-21-1-2-3-1001";
  row->type = type;
  return row;
}

}  // namespace

TEST_F(posture_command, group_members_defaults_to_the_administrators_group) {
  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {}), &response);

  EXPECT_EQ(requested_group, "Administrators");
}

TEST_F(posture_command, group_members_passes_the_requested_group_through) {
  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {"group=Remote Desktop Users"}), &response);

  EXPECT_EQ(requested_group, "Remote Desktop Users");
}

TEST_F(posture_command, group_members_reports_an_unreadable_group_as_an_error) {
  group_member_rows.error = "no such group: Nope";

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {"group=Nope"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("no such group"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, group_members_an_empty_group_is_ok_and_says_so) {
  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("Group is empty"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, group_members_without_an_allow_list_just_lists_the_members) {
  group_member("BUILTIN", "Administrator");
  group_member("CONTOSO", "Domain Admins", "group");

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("BUILTIN\\Administrator (user)"), std::string::npos) << join_lines(response);
  EXPECT_NE(join_lines(response).find("CONTOSO\\Domain Admins (group)"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, group_members_an_unexpected_member_is_critical) {
  group_member("BUILTIN", "Administrator");
  group_member("CONTOSO", "eve");

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(
      request_for("check_group_members", {"expected=BUILTIN\\Administrator", "top-syntax=${status}: ${problem_list}"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  // Only the drift is a problem; the allowed member is not in the problem list.
  EXPECT_NE(join_lines(response).find("CONTOSO\\eve (user)"), std::string::npos) << join_lines(response);
  EXPECT_EQ(join_lines(response).find("BUILTIN\\Administrator"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, group_members_a_fully_expected_group_is_ok) {
  group_member("BUILTIN", "Administrator");
  group_member("CONTOSO", "Domain Admins", "group");

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(
      request_for("check_group_members", {"expected=BUILTIN\\Administrator", "expected=CONTOSO\\Domain Admins"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, group_members_the_allow_list_accepts_bare_names_too) {
  group_member("CONTOSO", "Domain Admins", "group");

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {"expected=Domain Admins"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, group_members_emits_the_member_count_as_perfdata) {
  group_member("BUILTIN", "Administrator");

  PB::Commands::QueryResponseMessage::Response response;
  check_group_members_command::check(request_for("check_group_members", {}), &response);

  ASSERT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  // The member tally from set_default_perf_config("extra(count)"), last, after
  // the per-member series the default critical expression (`expected = 0`)
  // brings in.
  EXPECT_EQ(perf_labels(response), (std::vector<std::string>{"BUILTIN\\Administrator_expected", "count"})) << join_lines(response);
}

// ---------------------------------------------------------------------------
// check_users - an inventory check: no default alert, `count` is the lever.
// ---------------------------------------------------------------------------

namespace {

std::shared_ptr<users_filter::filter_obj> session(const std::string &user, const std::string &state, const std::string &type, const std::string &client) {
  auto row = user_rows.add();
  row->user = user;
  row->session_state = state;
  row->session_type = type;
  row->client = client;
  return row;
}

}  // namespace

TEST_F(posture_command, users_nobody_logged_on_is_ok_and_says_so) {
  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("No users logged on"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, users_lists_the_logged_on_sessions) {
  session("alice", "active", "console", "");
  session("bob", "active", "remote", "10.0.0.7");

  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("2 user(s) logged on"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, users_has_no_default_alert) {
  // Logged-on users are not a fault; the check is an inventory tool and must
  // stay OK until an operator says otherwise.
  for (int i = 0; i < 25; ++i) session("user" + std::to_string(i), "active", "rdp", "10.0.0.1");

  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
}

TEST_F(posture_command, users_the_count_summary_variable_drives_the_thresholds) {
  // Documented usage: `crit=count > N`. `count` is a generic summary variable
  // rather than a keyword of this check, so it is evaluated after matching.
  session("alice", "active", "console", "");
  session("bob", "active", "remote", "10.0.0.7");

  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {"crit=count > 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
}

TEST_F(posture_command, users_a_filter_narrows_what_is_counted) {
  session("alice", "active", "console", "");
  session("bob", "active", "remote", "10.0.0.7");
  session("carol", "disconnected", "rdp", "10.0.0.9");

  // Only the remote sessions are of interest, and there is one of them, so a
  // "more than one remote login" rule stays OK.
  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {"filter=session_type = 'remote'", "crit=count > 1"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::OK) << join_lines(response);
  EXPECT_NE(join_lines(response).find("1 user(s) logged on"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, users_the_client_keyword_reaches_the_remote_host) {
  session("bob", "active", "remote", "10.0.0.7");

  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {"crit=client = '10.0.0.7'"}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::CRITICAL) << join_lines(response);
  EXPECT_NE(join_lines(response).find("bob"), std::string::npos) << join_lines(response);
}

TEST_F(posture_command, users_reports_the_source_error) {
  user_rows.error = "cannot enumerate sessions";

  PB::Commands::QueryResponseMessage::Response response;
  check_users_command::check(request_for("check_users", {}), &response);

  EXPECT_EQ(response.result(), PB::Common::ResultCode::UNKNOWN) << join_lines(response);
  EXPECT_NE(join_lines(response).find("cannot enumerate sessions"), std::string::npos) << join_lines(response);
}
