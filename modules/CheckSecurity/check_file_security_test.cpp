// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The owner/DACL classification of check_file_security: which trustees count as
// write access, which of those are unexpected and which make a path world
// writable. The Windows API only supplies the entries; this is what judges them.

#include <gtest/gtest.h>

#include "check_file_security.hpp"

// The data source is the platform-specific half of the check (security
// descriptors and the service control manager) and is exercised by the
// integration suite instead; the command object still needs it to link.
namespace file_security_source {
bool supported() { return false; }
std::string service_binary(const std::string &, std::string &error) {
  error = "not available in the unit test";
  return {};
}
void inspect(file_security_filter::filter_obj &obj, std::vector<file_security_filter::ace> &) { obj.error = "not available in the unit test"; }
}  // namespace file_security_source

namespace {

using file_security_filter::ace;
using file_security_filter::filter_obj;

const unsigned long read_execute = file_security_filter::mask_read_data | file_security_filter::mask_read_attrs | file_security_filter::mask_execute;
const unsigned long modify = read_execute | file_security_filter::mask_write_data | file_security_filter::mask_append | file_security_filter::mask_delete;

ace allow(const std::string &name, const std::string &sid, const unsigned long mask) { return ace(name, sid, mask, true); }
ace deny(const std::string &name, const std::string &sid, const unsigned long mask) { return ace(name, sid, mask, false); }

filter_obj classify(const std::vector<ace> &aces, const std::vector<std::string> &allow_write = std::vector<std::string>()) {
  filter_obj obj;
  obj.path = "C:\\ProgramData\\nsclient";
  obj.exists = 1;
  obj.readable = 1;
  file_security_filter::apply_aces(obj, aces, allow_write);
  return obj;
}

}  // namespace

TEST(check_file_security, hardened_acl_has_no_unexpected_writers) {
  const filter_obj obj = classify({
      allow("NT AUTHORITY\\SYSTEM", "S-1-5-18", file_security_filter::mask_generic_all),
      allow("BUILTIN\\Administrators", "S-1-5-32-544", file_security_filter::mask_generic_all),
      allow("BUILTIN\\Users", "S-1-5-32-545", read_execute),
  });

  EXPECT_EQ(0, obj.get_unexpected_write());
  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_EQ(3, obj.get_ace_count());
  EXPECT_EQ("NT AUTHORITY\\SYSTEM, BUILTIN\\Administrators", obj.get_writable());
  EXPECT_EQ("", obj.get_unexpected());
}

TEST(check_file_security, everyone_with_write_is_world_writable) {
  const filter_obj obj = classify({
      allow("NT AUTHORITY\\SYSTEM", "S-1-5-18", file_security_filter::mask_generic_all),
      allow("Everyone", "S-1-1-0", modify),
  });

  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_EQ(1, obj.get_unexpected_write());
  EXPECT_EQ("Everyone", obj.get_unexpected());
  EXPECT_NE(std::string::npos, obj.get_state().find("world writable by Everyone"));
}

TEST(check_file_security, users_with_write_is_world_writable_too) {
  const filter_obj obj = classify({allow("BUILTIN\\Users", "S-1-5-32-545", modify)});

  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_EQ("BUILTIN\\Users", obj.get_unexpected());
}

TEST(check_file_security, everyone_with_read_only_is_not_writable) {
  const filter_obj obj = classify({allow("Everyone", "S-1-1-0", read_execute)});

  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_EQ(0, obj.get_unexpected_write());
  EXPECT_EQ("", obj.get_writable());
}

TEST(check_file_security, benign_write_attribute_bits_do_not_count_as_write) {
  // Windows hands FILE_WRITE_EA/FILE_WRITE_ATTRIBUTES out widely; neither lets a
  // trustee change the content, so they must not raise a finding on their own.
  const filter_obj obj =
      classify({allow("Everyone", "S-1-1-0", read_execute | file_security_filter::mask_write_ea | file_security_filter::mask_write_attrs)});

  EXPECT_EQ(0, obj.get_unexpected_write());
}

TEST(check_file_security, taking_ownership_counts_as_write_access) {
  const filter_obj obj = classify({allow("Authenticated Users", "S-1-5-11", file_security_filter::mask_write_owner)});

  EXPECT_EQ(1, obj.get_world_writable());
}

TEST(check_file_security, an_ordinary_account_with_write_is_unexpected_but_not_world_writable) {
  const filter_obj obj = classify({allow("WS01\\bob", "S-1-5-21-1-2-3-1001", modify)});

  EXPECT_EQ(1, obj.get_unexpected_write());
  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_NE(std::string::npos, obj.get_state().find("writable by WS01\\bob"));
}

TEST(check_file_security, allow_write_silences_a_known_trustee) {
  const std::vector<ace> aces = {allow("WS01\\backup", "S-1-5-21-1-2-3-1002", modify)};

  EXPECT_EQ(1, classify(aces).get_unexpected_write());
  EXPECT_EQ(0, classify(aces, {"backup"}).get_unexpected_write());                       // bare name
  EXPECT_EQ(0, classify(aces, {"WS01\\backup"}).get_unexpected_write());                 // qualified name
  EXPECT_EQ(0, classify(aces, {"S-1-5-21-1-2-3-1002"}).get_unexpected_write());          // SID
  EXPECT_EQ(0, classify({allow("Everyone", "S-1-1-0", modify)}, {"Everyone"}).get_world_writable());
}

TEST(check_file_security, a_deny_entry_overrides_the_matching_grant) {
  const filter_obj obj = classify({
      allow("Everyone", "S-1-1-0", modify),
      deny("Everyone", "S-1-1-0", file_security_filter::write_bits),
  });

  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_EQ(0, obj.get_unexpected_write());
  EXPECT_NE(std::string::npos, obj.get_dacl().find("!Everyone("));
}

TEST(check_file_security, a_partial_deny_leaves_the_remaining_write_access_visible) {
  // Denying only DELETE still leaves Everyone able to change the content.
  const filter_obj obj = classify({
      allow("Everyone", "S-1-1-0", modify),
      deny("Everyone", "S-1-1-0", file_security_filter::mask_delete),
  });

  EXPECT_EQ(1, obj.get_world_writable());
}

TEST(check_file_security, an_inherited_grant_counts_and_is_marked_in_the_dacl) {
  // Inherited access is the common case for a data folder, so it must be judged
  // exactly like an entry set on the object itself.
  const filter_obj obj = classify({ace("Everyone", "S-1-1-0", modify, true, true)});

  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_EQ("~Everyone(RWXD)", obj.get_dacl());
}

TEST(check_file_security, a_trustee_is_listed_once_however_many_entries_it_has) {
  const filter_obj obj = classify({
      allow("Everyone", "S-1-1-0", file_security_filter::mask_write_data),
      allow("Everyone", "S-1-1-0", file_security_filter::mask_delete),
  });

  EXPECT_EQ("Everyone", obj.get_writable());
}

TEST(check_file_security, rights_are_rendered_compactly) {
  EXPECT_EQ("F", file_security_filter::rights_summary(file_security_filter::mask_generic_all));
  EXPECT_EQ("F", file_security_filter::rights_summary(file_security_filter::mask_all_access));
  EXPECT_EQ("RX", file_security_filter::rights_summary(read_execute));
  EXPECT_EQ("RWXD", file_security_filter::rights_summary(modify));
  EXPECT_EQ("P", file_security_filter::rights_summary(file_security_filter::mask_write_owner));
  EXPECT_EQ("-", file_security_filter::rights_summary(0));
}

TEST(check_file_security, expected_owner_matches_sid_qualified_and_bare_names) {
  filter_obj obj;
  obj.owner = "BUILTIN\\Administrators";
  obj.owner_sid = "S-1-5-32-544";

  file_security_filter::apply_owner(obj, {});  // no list -> anything goes
  EXPECT_EQ(1, obj.get_owner_expected());
  file_security_filter::apply_owner(obj, {"Administrators"});
  EXPECT_EQ(1, obj.get_owner_expected());
  file_security_filter::apply_owner(obj, {"BUILTIN\\Administrators"});
  EXPECT_EQ(1, obj.get_owner_expected());
  file_security_filter::apply_owner(obj, {"S-1-5-32-544"});
  EXPECT_EQ(1, obj.get_owner_expected());
  file_security_filter::apply_owner(obj, {"NT AUTHORITY\\SYSTEM"});
  EXPECT_EQ(0, obj.get_owner_expected());
}

TEST(check_file_security, state_reports_a_missing_or_unreadable_path) {
  filter_obj missing;
  missing.path = "C:\\no\\such\\file";
  EXPECT_EQ("does not exist", missing.get_state());

  filter_obj failed;
  failed.error = "Service not found: nope";
  EXPECT_EQ("Service not found: nope", failed.get_state());

  filter_obj unreadable;
  unreadable.exists = 1;
  unreadable.error = "Access is denied";
  EXPECT_EQ("security descriptor unreadable: Access is denied", unreadable.get_state());
}

TEST(check_file_security, state_names_the_owner_when_everything_is_in_order) {
  filter_obj obj = classify({allow("NT AUTHORITY\\SYSTEM", "S-1-5-18", file_security_filter::mask_generic_all)});
  obj.owner = "BUILTIN\\Administrators";
  EXPECT_EQ("owner BUILTIN\\Administrators, no unexpected write access", obj.get_state());
}

TEST(check_file_security, state_reports_an_unexpected_owner) {
  filter_obj obj = classify({allow("NT AUTHORITY\\SYSTEM", "S-1-5-18", file_security_filter::mask_generic_all)});
  obj.owner = "WS01\\bob";
  obj.owner_sid = "S-1-5-21-1-2-3-1001";
  file_security_filter::apply_owner(obj, {"BUILTIN\\Administrators"});

  EXPECT_EQ(0, obj.get_owner_expected());
  EXPECT_EQ("unexpected owner WS01\\bob", obj.get_state());
}

TEST(check_file_security, a_specific_deny_cancels_a_generic_grant) {
  // icacls /grant writes GENERIC_WRITE where /deny writes the specific bits;
  // the two forms must cancel. Without generic mapping this reported the path
  // as world writable although Windows denies every write.
  const filter_obj obj = classify({
      allow("Everyone", "S-1-1-0", file_security_filter::mask_generic_write),
      deny("Everyone", "S-1-1-0",
           file_security_filter::mask_write_data | file_security_filter::mask_append | file_security_filter::mask_delete |
               file_security_filter::mask_write_dac | file_security_filter::mask_write_owner | file_security_filter::mask_write_ea |
               file_security_filter::mask_write_attrs),
  });

  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_EQ(0, obj.get_unexpected_write());
}

TEST(check_file_security, a_generic_deny_cancels_the_matching_specific_grant) {
  // The mirrored case: specific allow, GENERIC_WRITE deny. The content bits are
  // denied; DELETE is not part of FILE_GENERIC_WRITE, so a modify grant still
  // leaves the trustee able to delete - which is write access and stays flagged.
  const filter_obj content_only = classify({
      allow("Everyone", "S-1-1-0", file_security_filter::mask_write_data | file_security_filter::mask_append),
      deny("Everyone", "S-1-1-0", file_security_filter::mask_generic_write),
  });
  EXPECT_EQ(0, content_only.get_world_writable());

  const filter_obj with_delete = classify({
      allow("Everyone", "S-1-1-0", modify),
      deny("Everyone", "S-1-1-0", file_security_filter::mask_generic_write),
  });
  EXPECT_EQ(1, with_delete.get_world_writable());
}

TEST(check_file_security, generic_all_grants_render_as_full_and_count_as_write) {
  const filter_obj obj = classify({allow("Everyone", "S-1-1-0", file_security_filter::mask_generic_all)});
  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_NE(std::string::npos, obj.get_dacl().find("Everyone(F)"));
}
