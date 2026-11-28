// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The Unix data source of check_file_security: mode bits and POSIX ACLs turned
// into access entries, the program behind a systemd unit, and the command run
// end to end against scratch files whose mode the test sets itself.

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem.hpp>
#include <fstream>

#include "check_file_security_unix.hpp"

namespace {

using file_security_filter::ace;
using file_security_filter::filter_obj;
using file_security_posix::acl_entry;
namespace posix = file_security_posix;

// A made-up account database, so the tests do not depend on the host's.
std::string fake_user(const unsigned long uid) {
  if (uid == 0) return "root";
  if (uid == 33) return "www-data";
  if (uid == 1000) return "alice";
  return {};
}
std::string fake_group(const unsigned long gid) {
  if (gid == 0) return "root";
  if (gid == 4) return "adm";
  if (gid == 33) return "www-data";
  return {};
}

posix::identity file_of(const unsigned long uid, const unsigned long gid, const unsigned long mode, const bool is_dir = false) {
  posix::identity id;
  id.uid = uid;
  id.gid = gid;
  id.mode = mode;
  id.is_dir = is_dir;
  return id;
}

std::string render(const std::vector<ace> &aces) {
  filter_obj obj;
  file_security_filter::apply_aces(obj, aces, {});
  return obj.get_dacl();
}

std::vector<unsigned char> acl_blob(const std::vector<acl_entry> &entries, const unsigned version = 2) {
  std::vector<unsigned char> blob = {static_cast<unsigned char>(version), 0, 0, 0};
  for (const acl_entry &entry : entries) {
    blob.push_back(static_cast<unsigned char>(entry.tag & 0xff));
    blob.push_back(static_cast<unsigned char>(entry.tag >> 8));
    blob.push_back(static_cast<unsigned char>(entry.perm & 0xff));
    blob.push_back(0);
    for (int shift = 0; shift < 32; shift += 8) blob.push_back(static_cast<unsigned char>((entry.id >> shift) & 0xff));
  }
  return blob;
}

struct scratch_dir {
  scratch_dir() : root(boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-file-security-unix-%%%%-%%%%")) {
    boost::filesystem::create_directories(root);
  }
  ~scratch_dir() {
    boost::system::error_code ec;
    boost::filesystem::remove_all(root, ec);
  }
  std::string file(const std::string &name, const mode_t mode, const std::string &content = "x") const {
    const std::string path = (root / name).string();
    boost::filesystem::create_directories(boost::filesystem::path(path).parent_path());
    std::ofstream(path.c_str()) << content;
    // chmod, not the create mode: the umask must not decide what is tested.
    EXPECT_EQ(0, ::chmod(path.c_str(), mode));
    return path;
  }
  boost::filesystem::path root;
};

struct result {
  PB::Common::ResultCode code;
  std::string text;
};
result run(const std::vector<std::string> &args) {
  PB::Commands::QueryRequestMessage::Request request;
  request.set_command("check_file_security");
  for (const std::string &a : args) request.add_arguments(a);
  PB::Commands::QueryResponseMessage::Response response;
  check_file_security_command::check(request, &response);
  result r;
  r.code = response.result();
  for (int i = 0; i < response.lines_size(); ++i) r.text += response.lines(i).message();
  return r;
}

const std::string me = std::to_string(static_cast<unsigned long>(::getuid()));

}  // namespace

// ── Mode bits ────────────────────────────────────────────────────────────────

TEST(check_file_security_unix, mode_is_four_octal_digits_with_the_special_bits) {
  EXPECT_EQ("0644", posix::mode_string(0100644));
  EXPECT_EQ("4755", posix::mode_string(0104755));
  EXPECT_EQ("1777", posix::mode_string(0041777));
  EXPECT_EQ("0000", posix::mode_string(0100000));
}

TEST(check_file_security_unix, mode_bits_become_owner_group_and_other_entries) {
  std::vector<ace> aces;
  EXPECT_FALSE(posix::build_entries(file_of(33, 4, 0100640), {}, {}, fake_user, fake_group, aces));

  ASSERT_EQ(3u, aces.size());
  EXPECT_EQ("user:www-data(rw-), group:adm(r--), other(---)", render(aces));
  EXPECT_EQ("uid:33", aces[0].sid);
  EXPECT_TRUE(aces[0].owner);
  EXPECT_FALSE(aces[0].trusted);
  EXPECT_EQ("gid:4", aces[1].sid);
  EXPECT_FALSE(aces[1].owner);
  EXPECT_TRUE(aces[2].world);
}

TEST(check_file_security_unix, root_and_the_root_group_are_the_system) {
  std::vector<ace> aces;
  posix::build_entries(file_of(0, 0, 0100664), {}, {}, fake_user, fake_group, aces);

  EXPECT_TRUE(aces[0].trusted);
  EXPECT_TRUE(aces[1].trusted);
  EXPECT_FALSE(aces[2].trusted);

  filter_obj obj;
  file_security_filter::apply_aces(obj, aces, {});
  EXPECT_EQ(0, obj.get_unexpected_write());
  EXPECT_EQ("user:root, group:root", obj.get_writable());
}

TEST(check_file_security_unix, an_id_without_an_account_is_shown_as_the_number) {
  std::vector<ace> aces;
  posix::build_entries(file_of(4242, 4343, 0100600), {}, {}, fake_user, fake_group, aces);
  EXPECT_EQ("user:4242(rw-), group:4343(---), other(---)", render(aces));
}

// ── POSIX ACLs ───────────────────────────────────────────────────────────────

TEST(check_file_security_unix, the_acl_attribute_is_decoded) {
  std::vector<acl_entry> entries;
  ASSERT_TRUE(posix::parse_acl(acl_blob({{posix::tag_user_obj, 6, 0xffffffff}, {posix::tag_user, 6, 1000}, {posix::tag_mask, 4, 0xffffffff}}), entries));
  ASSERT_EQ(3u, entries.size());
  EXPECT_EQ(static_cast<unsigned>(posix::tag_user), entries[1].tag);
  EXPECT_EQ(6u, entries[1].perm);
  EXPECT_EQ(1000ul, entries[1].id);

  EXPECT_TRUE(posix::parse_acl({}, entries));  // no attribute at all
  EXPECT_TRUE(entries.empty());
}

TEST(check_file_security_unix, an_acl_that_cannot_be_decoded_is_refused) {
  std::vector<acl_entry> entries;
  EXPECT_FALSE(posix::parse_acl(acl_blob({{posix::tag_user, 6, 1000}}, 3), entries));  // unknown version
  EXPECT_FALSE(posix::parse_acl({2, 0, 0}, entries));                                  // short header
  std::vector<unsigned char> truncated = acl_blob({{posix::tag_user, 6, 1000}});
  truncated.pop_back();
  EXPECT_FALSE(posix::parse_acl(truncated, entries));
}

TEST(check_file_security_unix, a_named_acl_user_with_write_is_found_behind_a_harmless_mode) {
  // `setfacl -m u:alice:rw` on a root:root 0640 file leaves the mode at 0660 -
  // the group bits now show the mask - while root's group still only reads.
  const std::vector<acl_entry> access = {{posix::tag_user_obj, 6, 0}, {posix::tag_user, 6, 1000}, {posix::tag_group_obj, 4, 0},
                                         {posix::tag_mask, 6, 0},     {posix::tag_other, 0, 0}};
  std::vector<ace> aces;
  EXPECT_TRUE(posix::build_entries(file_of(0, 0, 0100660), access, {}, fake_user, fake_group, aces));
  EXPECT_EQ("user:root(rw-), user:alice(rw-), group:root(r--), other(---)", render(aces));

  filter_obj obj;
  file_security_filter::apply_aces(obj, aces, {});
  EXPECT_EQ(1, obj.get_unexpected_write());
  EXPECT_EQ("user:alice", obj.get_unexpected());
  EXPECT_EQ(4, obj.get_ace_count());
}

TEST(check_file_security_unix, the_acl_mask_caps_named_entries_and_the_owning_group) {
  // A mask of r-- takes the write bit away from everything but owner and other.
  const std::vector<acl_entry> access = {{posix::tag_user_obj, 6, 0}, {posix::tag_user, 7, 1000}, {posix::tag_group_obj, 6, 0},
                                         {posix::tag_group, 6, 4},    {posix::tag_mask, 4, 0},    {posix::tag_other, 0, 0}};
  std::vector<ace> aces;
  posix::build_entries(file_of(0, 33, 0100640), access, {}, fake_user, fake_group, aces);
  EXPECT_EQ("user:root(rw-), user:alice(r--), group:www-data(r--), group:adm(r--), other(---)", render(aces));

  filter_obj obj;
  file_security_filter::apply_aces(obj, aces, {});
  EXPECT_EQ(0, obj.get_unexpected_write());
}

TEST(check_file_security_unix, a_minimal_acl_says_nothing_the_mode_does_not) {
  const std::vector<acl_entry> access = {{posix::tag_user_obj, 6, 0}, {posix::tag_group_obj, 4, 0}, {posix::tag_other, 4, 0}};
  std::vector<ace> aces;
  EXPECT_FALSE(posix::build_entries(file_of(0, 0, 0100644), access, {}, fake_user, fake_group, aces));
  EXPECT_EQ("user:root(rw-), group:root(r--), other(r--)", render(aces));
}

TEST(check_file_security_unix, a_default_acl_on_a_directory_is_judged_for_what_it_hands_down) {
  const std::vector<acl_entry> defaults = {{posix::tag_user_obj, 7, 0}, {posix::tag_user, 7, 1000}, {posix::tag_group_obj, 5, 0},
                                           {posix::tag_mask, 7, 0},     {posix::tag_other, 7, 0}};
  std::vector<ace> aces;
  EXPECT_TRUE(posix::build_entries(file_of(0, 0, 0040755, true), {}, defaults, fake_user, fake_group, aces));
  EXPECT_EQ("user:root(rwx), group:root(r-x), other(r-x), default:user:alice(rwx), default:other(rwx)", render(aces));

  filter_obj obj;
  file_security_filter::apply_aces(obj, aces, {});
  EXPECT_EQ(1, obj.get_world_writable());
  EXPECT_EQ("default:user:alice, default:other", obj.get_unexpected());

  // The same attribute on a regular file is not a thing; it must not be read in.
  std::vector<ace> file_aces;
  EXPECT_FALSE(posix::build_entries(file_of(0, 0, 0100644), {}, defaults, fake_user, fake_group, file_aces));
  EXPECT_EQ(3u, file_aces.size());
}

// ── systemd units ────────────────────────────────────────────────────────────

TEST(check_file_security_unix, the_program_is_cut_out_of_an_exec_start_line) {
  EXPECT_EQ("/usr/sbin/sshd", posix::exec_start_program("/usr/sbin/sshd -D $SSHD_OPTS"));
  EXPECT_EQ("/usr/sbin/sshd", posix::exec_start_program("  /usr/sbin/sshd"));
  EXPECT_EQ("/usr/bin/app", posix::exec_start_program("-/usr/bin/app --flag"));
  EXPECT_EQ("/usr/bin/app", posix::exec_start_program("@-+!:/usr/bin/app argv0"));
  EXPECT_EQ("/opt/my app/run", posix::exec_start_program("\"/opt/my app/run\" --flag"));
  EXPECT_EQ("", posix::exec_start_program("  "));
}

TEST(check_file_security_unix, a_unit_resolves_to_the_program_it_starts) {
  const scratch_dir dirs;
  const std::string etc = (dirs.root / "etc").string(), lib = (dirs.root / "lib").string();
  dirs.file("lib/app.service", 0644,
            "[Unit]\nDescription=app\n# ExecStart=/not/this\n[Service]\nType=simple\nExecStart=/usr/bin/app \\\n  --serve\nExecStartPost=/bin/true\n"
            "[Install]\nWantedBy=multi-user.target\n");

  std::string error;
  EXPECT_EQ("/usr/bin/app", posix::unit_program({etc, lib}, "app", error));          // ".service" is implied
  EXPECT_EQ("/usr/bin/app", posix::unit_program({etc, lib}, "app.service", error));
  EXPECT_EQ("", error);

  // A unit of the same name earlier on the search path replaces it outright.
  dirs.file("etc/app.service", 0644, "[Service]\nExecStart=/usr/local/bin/app\n");
  EXPECT_EQ("/usr/local/bin/app", posix::unit_program({etc, lib}, "app", error));
}

TEST(check_file_security_unix, a_drop_in_can_replace_the_command) {
  const scratch_dir dirs;
  const std::string etc = (dirs.root / "etc").string(), lib = (dirs.root / "lib").string();
  dirs.file("lib/app.service", 0644, "[Service]\nExecStart=/usr/bin/app\n");
  std::string error;

  // Adding to a unit that already has a command does not change which one runs.
  dirs.file("lib/app.service.d/10-extra.conf", 0644, "[Service]\nEnvironment=A=b\n");
  EXPECT_EQ("/usr/bin/app", posix::unit_program({etc, lib}, "app", error));

  // The documented override: clear, then set.
  dirs.file("etc/app.service.d/override.conf", 0644, "[Service]\nExecStart=\nExecStart=/opt/patched/app\n");
  EXPECT_EQ("/opt/patched/app", posix::unit_program({etc, lib}, "app", error));
}

TEST(check_file_security_unix, an_instance_is_started_from_its_template) {
  const scratch_dir dirs;
  const std::string lib = (dirs.root / "lib").string();
  dirs.file("lib/getty@.service", 0644, "[Service]\nExecStart=-/sbin/agetty -o '-p -- \\\\u' --noclear %I $TERM\n");

  std::string error;
  EXPECT_EQ("/sbin/agetty", posix::unit_program({lib}, "getty@tty1.service", error));
  EXPECT_EQ("/sbin/agetty", posix::unit_program({lib}, "getty@tty1", error));
}

TEST(check_file_security_unix, a_unit_that_is_missing_or_starts_nothing_is_an_error) {
  const scratch_dir dirs;
  const std::string lib = (dirs.root / "lib").string();
  dirs.file("lib/idle.service", 0644, "[Unit]\nDescription=starts nothing\n[Service]\nType=oneshot\n");
  dirs.file("secret.service", 0644, "[Service]\nExecStart=/bin/outside\n");

  std::string error;
  EXPECT_EQ("", posix::unit_program({lib}, "nope", error));
  EXPECT_EQ("Service not found: nope", error);
  EXPECT_EQ("", posix::unit_program({lib}, "idle", error));
  EXPECT_EQ("Service idle has no ExecStart command", error);
  // A unit name is never a path out of the unit directories.
  EXPECT_EQ("", posix::unit_program({lib}, "../secret", error));
  EXPECT_EQ("Service not found: ../secret", error);
}

// ── inspect() against real files ─────────────────────────────────────────────

TEST(check_file_security_unix, inspect_reads_owner_mode_and_metadata) {
  const scratch_dir dir;
  filter_obj obj;
  obj.path = dir.file("data.bin", 0640, "12345");
  std::vector<ace> aces;
  file_security_source::inspect(obj, aces);

  EXPECT_EQ(1, obj.get_exists());
  EXPECT_EQ(1, obj.get_readable());
  EXPECT_EQ(0, obj.get_is_dir());
  EXPECT_EQ(me, obj.get_owner_sid());
  EXPECT_FALSE(obj.get_owner().empty());
  EXPECT_FALSE(obj.get_group().empty());
  EXPECT_EQ("0640", obj.get_mode());
  EXPECT_EQ("", obj.get_sddl());
  EXPECT_EQ(5, obj.get_size());
  EXPECT_GT(obj.get_written(), 0);
  EXPECT_EQ(0, obj.get_setuid());
  ASSERT_EQ(3u, aces.size());
  EXPECT_EQ("uid:" + me, aces[0].sid);
}

TEST(check_file_security_unix, inspect_reports_the_special_bits) {
  const scratch_dir dir;
  filter_obj obj;
  obj.path = dir.file("tool", 06755);
  std::vector<ace> aces;
  file_security_source::inspect(obj, aces);

  EXPECT_EQ("6755", obj.get_mode());
  EXPECT_EQ(1, obj.get_setuid());
  EXPECT_EQ(1, obj.get_setgid());
  EXPECT_EQ(0, obj.get_sticky());
}

TEST(check_file_security_unix, inspect_reports_a_missing_path_without_an_error) {
  filter_obj obj;
  obj.path = "/nscp-no-such-dir-zzz/file";
  std::vector<ace> aces;
  file_security_source::inspect(obj, aces);

  EXPECT_EQ(0, obj.get_exists());
  EXPECT_EQ("", obj.get_error());
  EXPECT_EQ("does not exist", obj.get_state());
}

// ── The command, end to end ──────────────────────────────────────────────────

TEST(check_file_security_unix, command_is_critical_for_a_world_writable_file) {
  const scratch_dir dir;
  const result r = run({"path=" + dir.file("open.conf", 0666), "expected-owner=" + me});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code);
  EXPECT_NE(std::string::npos, r.text.find("world writable by other")) << r.text;
}

TEST(check_file_security_unix, command_is_ok_for_a_private_file_of_the_expected_owner) {
  const scratch_dir dir;
  const result r = run({"path=" + dir.file("private.conf", 0600), "expected-owner=" + me});
  EXPECT_EQ(PB::Common::ResultCode::OK, r.code) << r.text;
  EXPECT_NE(std::string::npos, r.text.find("mode 0600, no unexpected write access")) << r.text;
}

TEST(check_file_security_unix, command_is_critical_for_an_unexpected_owner_or_group) {
  const scratch_dir dir;
  const std::string file = dir.file("owned.conf", 0600);

  const result owner = run({"path=" + file, "expected-owner=nscp-no-such-user-zzz"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, owner.code);
  EXPECT_NE(std::string::npos, owner.text.find("unexpected owner")) << owner.text;

  const result group = run({"path=" + file, "expected-owner=" + me, "expected-group=nscp-no-such-group-zzz"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, group.code);
  EXPECT_NE(std::string::npos, group.text.find("unexpected group")) << group.text;
}

TEST(check_file_security_unix, command_can_alert_on_the_mode_itself) {
  const scratch_dir dir;
  const std::string file = dir.file("key.pem", 0644);
  const std::vector<std::string> common = {"path=" + file, "warning=none", "top-syntax=${status}: ${list}", "detail-syntax=${mode}"};

  std::vector<std::string> strict = common;
  strict.push_back("critical=mode != '0600'");
  const result bad = run(strict);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, bad.code) << bad.text;
  EXPECT_NE(std::string::npos, bad.text.find("0644")) << bad.text;

  std::vector<std::string> matching = common;
  matching.push_back("critical=mode != '0644'");
  EXPECT_EQ(PB::Common::ResultCode::OK, run(matching).code);
}

TEST(check_file_security_unix, command_walks_a_tree_and_finds_the_bad_file_inside) {
  const scratch_dir dir;
  dir.file("conf/a.conf", 0600);
  dir.file("conf/sub/b.conf", 0602);
  const std::string root = (dir.root / "conf").string();
  ASSERT_EQ(0, ::chmod(root.c_str(), 0700));
  ASSERT_EQ(0, ::chmod((dir.root / "conf" / "sub").string().c_str(), 0700));
  const std::vector<std::string> common = {"path=" + root, "expected-owner=" + me, "top-syntax=${status}: ${count} checked, ${problem_list}"};

  // Without max-depth only the directory itself is looked at.
  const result shallow = run(common);
  EXPECT_EQ(PB::Common::ResultCode::OK, shallow.code) << shallow.text;

  std::vector<std::string> one = common;
  one.push_back("max-depth=1");
  EXPECT_EQ(PB::Common::ResultCode::OK, run(one).code);

  std::vector<std::string> deep = common;
  deep.push_back("max-depth=2");
  const result found = run(deep);
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, found.code) << found.text;
  EXPECT_NE(std::string::npos, found.text.find("4 checked")) << found.text;
  EXPECT_NE(std::string::npos, found.text.find("b.conf: world writable by other")) << found.text;
  EXPECT_EQ(std::string::npos, found.text.find("a.conf")) << found.text;
}

TEST(check_file_security_unix, command_reports_a_service_that_does_not_exist) {
  const result r = run({"service=nscp-no-such-unit-zzz"});
  EXPECT_EQ(PB::Common::ResultCode::CRITICAL, r.code);
  EXPECT_NE(std::string::npos, r.text.find("Service not found: nscp-no-such-unit-zzz")) << r.text;
}

TEST(check_file_security_unix, command_requires_something_to_inspect) {
  const result r = run({});
  EXPECT_NE(std::string::npos, r.text.find("No path specified")) << r.text;
}
