// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The owner/DACL classification of check_file_security: which trustees count as
// write access, which of those are unexpected and which make a path world
// writable. The Windows API only supplies the entries; this is what judges them.

#include <gtest/gtest.h>

#include "check_file_security.hpp"

#include <boost/filesystem.hpp>
#include <fstream>

#ifdef WIN32
// The Windows data source (security descriptors and the service control
// manager) is exercised by the integration suite instead; the command object
// still needs it to link. Off Windows the real source is linked in and has
// tests of its own (check_file_security_unix_test.cpp).
namespace file_security_source {
std::string service_binary(const std::string &, std::string &error) {
  error = "not available in the unit test";
  return {};
}
void inspect(file_security_filter::filter_obj &obj, std::vector<file_security_filter::ace> &) { obj.error = "not available in the unit test"; }
}  // namespace file_security_source
#endif

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

// ── Entries a Unix source hands in ───────────────────────────────────────────
// The classification is shared, so what a POSIX permission class means is
// decided here: "other" is the world, root is the system, and the owner is only
// as trustworthy as the caller said.

namespace {

ace posix(const std::string &trustee, const std::string &sid, const std::string &rights) {
  unsigned long mask = 0;
  if (rights[0] == 'r') mask |= file_security_filter::mask_read_data;
  if (rights[1] == 'w') mask |= file_security_filter::mask_write_data | file_security_filter::mask_append;
  if (rights[2] == 'x') mask |= file_security_filter::mask_execute;
  ace entry(trustee, sid, mask, true);
  entry.rights = rights;
  return entry;
}
ace posix_owner(const std::string &name, const std::string &uid, const std::string &rights) {
  ace entry = posix("user:" + name, "uid:" + uid, rights);
  entry.owner = true;
  entry.trusted = uid == "0";
  return entry;
}
ace posix_other(const std::string &rights) {
  ace entry = posix("other", "", rights);
  entry.world = true;
  return entry;
}

filter_obj classify_posix(const std::string &owner, const std::string &uid, const std::vector<ace> &aces,
                          const std::vector<std::string> &allow_write = std::vector<std::string>(),
                          const std::vector<std::string> &expected_owners = std::vector<std::string>()) {
  filter_obj obj;
  obj.path = "/etc/app.conf";
  obj.exists = 1;
  obj.readable = 1;
  obj.owner = owner;
  obj.owner_sid = uid;
  file_security_filter::apply_aces(obj, aces, allow_write, expected_owners);
  file_security_filter::apply_owner(obj, expected_owners);
  return obj;
}

}  // namespace

TEST(check_file_security, a_root_owned_0644_file_has_no_unexpected_writers) {
  ace group = posix("group:root", "gid:0", "r--");
  group.trusted = true;
  const filter_obj obj = classify_posix("root", "0", {posix_owner("root", "0", "rw-"), group, posix_other("r--")});

  EXPECT_EQ(0, obj.get_unexpected_write());
  EXPECT_EQ(0, obj.get_world_writable());
  EXPECT_EQ("user:root", obj.get_writable());
  EXPECT_EQ("user:root(rw-), group:root(r--), other(r--)", obj.get_dacl());
}

TEST(check_file_security, the_other_class_with_write_is_world_writable) {
  const filter_obj obj = classify_posix("root", "0", {posix_owner("root", "0", "rw-"), posix("group:root", "gid:0", "r--"), posix_other("rw-")});

  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_EQ("other", obj.get_unexpected());
  EXPECT_EQ("world writable by other", obj.get_state());
  // ...and, as on Windows, naming it in allow-write is how a path that is meant
  // to be open (a spool directory) is accepted.
  EXPECT_EQ(0, classify_posix("root", "0", {posix_other("rwx")}, {"other"}).get_world_writable());
}

TEST(check_file_security, a_non_root_owner_is_unexpected_until_the_caller_expects_it) {
  const std::vector<ace> aces = {posix_owner("www-data", "33", "rw-"), posix("group:www-data", "gid:33", "r--"), posix_other("r--")};

  const filter_obj unexpected = classify_posix("www-data", "33", aces);
  EXPECT_EQ(1, unexpected.get_unexpected_write());
  EXPECT_EQ(0, unexpected.get_world_writable());
  EXPECT_EQ("writable by user:www-data", unexpected.get_state());

  // An owner can always chmod what it owns, so naming it as the expected owner
  // has to accept its write bit too - by name or by uid.
  EXPECT_EQ(0, classify_posix("www-data", "33", aces, {}, {"www-data"}).get_unexpected_write());
  EXPECT_EQ(0, classify_posix("www-data", "33", aces, {}, {"33"}).get_unexpected_write());
  // A different expected owner accepts nothing.
  const filter_obj wrong = classify_posix("www-data", "33", aces, {}, {"root"});
  EXPECT_EQ(1, wrong.get_unexpected_write());
  EXPECT_EQ(0, wrong.get_owner_expected());
}

TEST(check_file_security, expecting_an_owner_does_not_excuse_other_writers) {
  // The expected-owner pass covers the owner class only: a group of the same
  // name with a write bit is still somebody else writing.
  const filter_obj obj =
      classify_posix("app", "1000", {posix_owner("app", "1000", "rw-"), posix("group:app", "gid:1000", "rw-"), posix_other("r--")}, {}, {"app"});

  EXPECT_EQ(1, obj.get_unexpected_write());
  EXPECT_EQ("group:app", obj.get_unexpected());
}

TEST(check_file_security, unix_trustees_are_allow_listed_by_name_kind_or_id) {
  const std::vector<ace> aces = {posix("group:adm", "gid:4", "rw-")};

  EXPECT_EQ(1, classify_posix("root", "0", aces).get_unexpected_write());
  EXPECT_EQ(0, classify_posix("root", "0", aces, {"adm"}).get_unexpected_write());        // bare name
  EXPECT_EQ(0, classify_posix("root", "0", aces, {"group:adm"}).get_unexpected_write());  // kind and name
  EXPECT_EQ(0, classify_posix("root", "0", aces, {"gid:4"}).get_unexpected_write());      // id
  EXPECT_EQ(1, classify_posix("root", "0", aces, {"user:adm"}).get_unexpected_write());   // a user of that name is somebody else
  // A default-ACL entry is the same trustee under another heading.
  EXPECT_EQ(0, classify_posix("root", "0", {posix("default:group:adm", "gid:4", "rw-")}, {"adm"}).get_unexpected_write());
}

TEST(check_file_security, expected_group_matches_name_and_gid) {
  filter_obj obj;
  obj.group = "shadow";
  obj.group_sid = "42";

  file_security_filter::apply_group(obj, {});
  EXPECT_EQ(1, obj.get_group_expected());
  file_security_filter::apply_group(obj, {"shadow"});
  EXPECT_EQ(1, obj.get_group_expected());
  file_security_filter::apply_group(obj, {"42"});
  EXPECT_EQ(1, obj.get_group_expected());
  file_security_filter::apply_group(obj, {"root"});
  EXPECT_EQ(0, obj.get_group_expected());
}

TEST(check_file_security, state_sums_a_unix_path_up_like_ls) {
  filter_obj obj = classify_posix("root", "0", {posix_owner("root", "0", "rw-"), posix_other("r--")});
  obj.group = "shadow";
  obj.group_sid = "42";
  obj.mode = "0640";
  EXPECT_EQ("owner root:shadow mode 0640, no unexpected write access", obj.get_state());

  file_security_filter::apply_group(obj, {"root"});
  EXPECT_EQ("unexpected group shadow", obj.get_state());
}

TEST(check_file_security, timestamps_render_as_dates_and_stay_empty_when_unknown) {
  filter_obj obj;
  EXPECT_EQ("", obj.get_written_s());
  EXPECT_EQ(0, obj.get_age());
  obj.written = 86400;
  EXPECT_EQ("1970-01-02 00:00:00", obj.get_written_s());
}

// ── The directory walk behind max-depth ──────────────────────────────────────

namespace {

struct scratch_tree {
  scratch_tree() : root(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-file-security-%%%%-%%%%")) {
    boost::filesystem::create_directories(root / "a" / "b");
    touch(root / "top.txt");
    touch(root / "a" / "mid.txt");
    touch(root / "a" / "b" / "deep.txt");
  }
  ~scratch_tree() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(root, ec);
  }
  static void touch(const boost::filesystem::path &file) { std::ofstream(file.string().c_str()) << "x"; }
  std::vector<std::string> walk(const int depth) const {
    std::vector<std::string> found;
    std::vector<std::pair<std::string, std::string>> failed;
    file_security_filter::walk(root.string(), depth, found, failed);
    EXPECT_TRUE(failed.empty());
    for (std::string &entry : found) entry = boost::filesystem::relative(entry, root).generic_string();
    return found;
  }
  boost::filesystem::path root;
};

}  // namespace

TEST(check_file_security, walk_goes_as_deep_as_asked_and_no_deeper) {
  const scratch_tree tree;

  EXPECT_TRUE(tree.walk(0).empty());
  EXPECT_EQ((std::vector<std::string>{"a", "top.txt"}), tree.walk(1));
  EXPECT_EQ((std::vector<std::string>{"a", "a/b", "a/mid.txt", "top.txt"}), tree.walk(2));
  EXPECT_EQ((std::vector<std::string>{"a", "a/b", "a/b/deep.txt", "a/mid.txt", "top.txt"}), tree.walk(9));
}

TEST(check_file_security, walk_of_a_file_or_a_missing_path_finds_nothing) {
  const scratch_tree tree;
  std::vector<std::string> found;
  std::vector<std::pair<std::string, std::string>> failed;

  file_security_filter::walk((tree.root / "top.txt").string(), 3, found, failed);
  file_security_filter::walk((tree.root / "no-such-dir").string(), 3, found, failed);
  EXPECT_TRUE(found.empty());
  EXPECT_TRUE(failed.empty());
}
