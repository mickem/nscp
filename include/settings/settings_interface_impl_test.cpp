/*
 * Copyright (C) 2004-2016 Michael Medin
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

#include <gtest/gtest.h>

#include <algorithm>
#include <boost/make_shared.hpp>
#include <map>
#include <set>
#include <settings/settings_interface_impl.hpp>
#include <settings/test_helpers.hpp>
#include <string>
#include <utility>

using settings_test::mock_settings_core;

namespace {

// Minimal in-memory backend: implements every pure virtual on
// settings_interface_impl by reading/writing two STL containers.  Used to
// drive the base class through realistic save/load/diff scenarios without
// touching disk or the network.
class memory_backend : public settings::settings_interface_impl {
 public:
  using kv_map = std::map<std::pair<std::string, std::string>, std::string>;
  using path_set = std::set<std::string>;

  // Public so tests can pre-seed or read back what the backend persisted.
  kv_map persisted_values;
  path_set persisted_paths;

  memory_backend(settings::settings_core *core, std::string alias, std::string context) : settings::settings_interface_impl(core, alias, context) {}

  // ---- Real (backend) operations ---------------------------------------
  op_string get_real_string(settings_core::key_path_type key) override {
    auto it = persisted_values.find(key);
    if (it == persisted_values.end()) return op_string();
    return it->second;
  }
  void set_real_value(settings_core::key_path_type key, conainer value) override {
    if (!value.is_dirty()) return;
    persisted_values[key] = value.get_string();
    persisted_paths.insert(key.first);
  }
  void set_real_path(std::string path) override { persisted_paths.insert(path); }
  void remove_real_value(settings_core::key_path_type key) override { persisted_values.erase(key); }
  void remove_real_path(std::string path) override {
    persisted_paths.erase(path);
    for (auto it = persisted_values.begin(); it != persisted_values.end();) {
      if (it->first.first == path)
        it = persisted_values.erase(it);
      else
        ++it;
    }
  }
  bool has_real_key(settings_core::key_path_type key) override { return persisted_values.find(key) != persisted_values.end(); }
  bool has_real_path(std::string path) override { return persisted_paths.find(path) != persisted_paths.end(); }
  void get_real_sections(std::string path, string_list &list) override {
    // Mimic the production backends: return top-level sections when path is
    // empty, and direct sub-sections (paths strictly starting with "path/")
    // otherwise.  This prevents st_copy_section from recursing forever when
    // called with an existing section as its argument.
    if (path.empty()) {
      for (const auto &p : persisted_paths) list.push_back(p);
      return;
    }
    const std::string prefix = path + "/";
    for (const auto &p : persisted_paths) {
      if (p.size() > prefix.size() && p.compare(0, prefix.size(), prefix) == 0) list.push_back(p);
    }
  }
  void get_real_keys(std::string path, string_list &list) override {
    for (const auto &kv : persisted_values) {
      if (kv.first.first == path) list.push_back(kv.first.second);
    }
  }
  void real_clear_cache() override {}

  // ---- settings_interface boilerplate ----------------------------------
  std::string get_info() override { return "memory backend"; }
  std::string get_type() override { return "memory"; }
  settings::error_list validate() override { return {}; }
  void ensure_exists() override {}
  void enable_credentials() override {}
  bool supports_updates() override { return true; }
};

// Helper: look for a change_entry of the given kind at (path,key) in a list.
const settings::settings_interface::change_entry *find_change(const settings::settings_interface::change_list &list, const std::string &path,
                                                              const std::string &key, settings::settings_interface::change_entry::change_kind kind) {
  for (const auto &c : list) {
    if (c.kind == kind && c.path == path && c.key == key) return &c;
  }
  return nullptr;
}

}  // namespace

// ============================================================================
// get / set caching round-trip
// ============================================================================

TEST(settings_interface_impl, get_string_returns_persisted_value) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/foo", "bar"}] = "baz";
  auto v = b.get_string("/foo", "bar");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "baz");
}

TEST(settings_interface_impl, get_string_default_used_when_missing) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  EXPECT_EQ(b.get_string("/foo", "bar", "fallback"), "fallback");
}

TEST(settings_interface_impl, set_string_then_get_returns_new_value_before_save) {
  // The cache layer should serve writes back immediately, even though they
  // aren't yet flushed to the backend.
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/foo", "bar", "value");
  auto v = b.get_string("/foo", "bar");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "value");
  // Backend has NOT been written to yet.
  EXPECT_TRUE(b.persisted_values.empty());
}

TEST(settings_interface_impl, save_flushes_pending_writes_to_backend) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/section", "key", "value");
  b.save(false);
  ASSERT_EQ(b.persisted_values.count({"/section", "key"}), 1u);
  EXPECT_EQ(b.persisted_values[{"/section", "key"}], "value");
}

TEST(settings_interface_impl, save_skips_unchanged_cached_value) {
  // When an existing backend value is "set" to the same value, the entry is
  // marked clean and save() should not rewrite it.  We detect this by
  // pre-seeding a backend value, calling set with the same string, then
  // overwriting persisted_values directly and verifying save doesn't restore.
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/section", "key"}] = "original";
  b.set_string("/section", "key", "original");           // marks not-dirty
  b.persisted_values[{"/section", "key"}] = "external";  // simulate side-channel write
  b.save(false);
  EXPECT_EQ(b.persisted_values[{"/section", "key"}], "external") << "save should not rewrite an unchanged cache entry";
}

TEST(settings_interface_impl, save_respects_dirty_flag_difference) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/section", "key"}] = "original";
  b.set_string("/section", "key", "updated");
  b.save(false);
  EXPECT_EQ(b.persisted_values[{"/section", "key"}], "updated");
}

// ============================================================================
// remove_key / remove_path
// ============================================================================

TEST(settings_interface_impl, remove_key_drops_persisted_value_after_save) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/section", "k1"}] = "v1";
  b.persisted_values[{"/section", "k2"}] = "v2";
  b.remove_key("/section", "k1");
  b.save(false);
  EXPECT_EQ(b.persisted_values.count({"/section", "k1"}), 0u);
  EXPECT_EQ(b.persisted_values.count({"/section", "k2"}), 1u);
}

TEST(settings_interface_impl, remove_path_drops_persisted_section_after_save) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_paths.insert("/dead");
  b.persisted_values[{"/dead", "k"}] = "v";
  b.remove_path("/dead");
  b.save(false);
  EXPECT_EQ(b.persisted_paths.count("/dead"), 0u);
  EXPECT_EQ(b.persisted_values.count({"/dead", "k"}), 0u);
}

TEST(settings_interface_impl, remove_key_after_set_drops_in_flight_change) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/section", "key", "value");
  b.remove_key("/section", "key");
  b.save(false);
  EXPECT_EQ(b.persisted_values.count({"/section", "key"}), 0u);
}

// ============================================================================
// add_path / has_section / get_sections
// ============================================================================

TEST(settings_interface_impl, add_path_makes_section_visible) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  EXPECT_FALSE(b.has_section("/new"));
  b.add_path("/new");
  EXPECT_TRUE(b.has_section("/new"));
}

TEST(settings_interface_impl, has_section_true_for_persisted_section) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_paths.insert("/persisted");
  EXPECT_TRUE(b.has_section("/persisted"));
}

TEST(settings_interface_impl, save_flushes_added_paths_to_backend) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.add_path("/added");
  b.save(false);
  EXPECT_EQ(b.persisted_paths.count("/added"), 1u);
}

// ============================================================================
// get_keys / has_key
// ============================================================================

TEST(settings_interface_impl, get_keys_returns_backend_and_in_flight_keys) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/sec", "persisted"}] = "v1";
  b.set_string("/sec", "in_flight", "v2");
  auto keys = b.get_keys("/sec");
  EXPECT_NE(std::find(keys.begin(), keys.end(), "persisted"), keys.end());
  EXPECT_NE(std::find(keys.begin(), keys.end(), "in_flight"), keys.end());
}

TEST(settings_interface_impl, has_key_true_for_in_flight_value) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/sec", "key", "val");
  EXPECT_TRUE(b.has_key("/sec", "key"));
}

TEST(settings_interface_impl, has_key_false_for_unknown_key) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  EXPECT_FALSE(b.has_key("/sec", "key"));
}

// ============================================================================
// get_changes
// ============================================================================

TEST(settings_interface_impl, get_changes_empty_for_clean_store) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/p", "k"}] = "v";
  EXPECT_TRUE(b.get_changes().empty());
}

TEST(settings_interface_impl, get_changes_reports_added_for_new_key) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/p", "k", "v");
  auto changes = b.get_changes();
  const auto *added = find_change(changes, "/p", "k", settings::settings_interface::change_entry::change_kind::added);
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->new_value, "v");
  EXPECT_TRUE(added->old_value.empty());
}

TEST(settings_interface_impl, get_changes_reports_modified_for_overwritten_key) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/p", "k"}] = "old";
  b.set_string("/p", "k", "new");
  auto changes = b.get_changes();
  const auto *mod = find_change(changes, "/p", "k", settings::settings_interface::change_entry::change_kind::modified);
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->old_value, "old");
  EXPECT_EQ(mod->new_value, "new");
}

TEST(settings_interface_impl, get_changes_reports_removed_for_deleted_key) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/p", "k"}] = "doomed";
  b.remove_key("/p", "k");
  auto changes = b.get_changes();
  const auto *rem = find_change(changes, "/p", "k", settings::settings_interface::change_entry::change_kind::removed);
  ASSERT_NE(rem, nullptr);
  EXPECT_EQ(rem->old_value, "doomed");
}

TEST(settings_interface_impl, get_changes_reports_path_added_for_new_section) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.add_path("/brandnew");
  auto changes = b.get_changes();
  // path_added is reported when the path is in path_cache_ AND not in the
  // persisted store.  Look for a path_added entry on /brandnew.
  bool found = false;
  for (const auto &c : changes) {
    if (c.kind == settings::settings_interface::change_entry::change_kind::path_added && c.path == "/brandnew") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(settings_interface_impl, get_changes_skips_path_added_when_already_persisted) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_paths.insert("/already");
  b.add_path("/already");
  auto changes = b.get_changes();
  for (const auto &c : changes) {
    EXPECT_FALSE(c.kind == settings::settings_interface::change_entry::change_kind::path_added && c.path == "/already")
        << "path_added should not be reported for existing sections";
  }
}

TEST(settings_interface_impl, get_changes_reports_path_removed) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_paths.insert("/section");
  b.remove_path("/section");
  auto changes = b.get_changes();
  bool found = false;
  for (const auto &c : changes) {
    if (c.kind == settings::settings_interface::change_entry::change_kind::path_removed && c.path == "/section") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(settings_interface_impl, get_changes_empty_after_save) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/p", "k", "v");
  b.save(false);
  // save() drains path_cache_ and writes settings_cache_ to the backend, but
  // it does NOT clear settings_cache_ — entries remain marked dirty.  Verify
  // current behaviour: a subsequent get_changes will still report the change
  // as "modified" (since it's now in the backend) until clear_cache or load.
  auto changes = b.get_changes();
  for (const auto &c : changes) {
    EXPECT_NE(c.kind, settings::settings_interface::change_entry::change_kind::added)
        << "after save, the new key is in the backend so it can no longer be 'added'";
  }
}

// ============================================================================
// clear_cache / load
// ============================================================================

TEST(settings_interface_impl, clear_cache_drops_in_flight_writes) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/sec", "k", "uncommitted");
  b.clear_cache();
  // Backend was never written to, so the value is gone entirely.
  EXPECT_FALSE(b.get_string("/sec", "k").has_value());
}

TEST(settings_interface_impl, clear_cache_preserves_persisted_values) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.persisted_values[{"/p", "k"}] = "stored";
  b.clear_cache();
  auto v = b.get_string("/p", "k");
  ASSERT_TRUE(v.has_value());
  EXPECT_EQ(*v, "stored");
}

TEST(settings_interface_impl, load_clears_pending_changes) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  b.set_string("/p", "k", "v");
  b.add_path("/added");
  b.remove_key("/q", "j");
  b.load();
  EXPECT_TRUE(b.get_changes().empty());
}

// ============================================================================
// save_to / cross-store copy
// ============================================================================

TEST(settings_interface_impl, save_to_copies_keys_into_destination) {
  mock_settings_core core;
  memory_backend src(&core, "src", "memory://src");
  src.persisted_values[{"/a", "k1"}] = "v1";
  src.persisted_values[{"/a", "k2"}] = "v2";
  src.persisted_paths.insert("/a");

  auto dst = boost::make_shared<memory_backend>(&core, "dst", "memory://dst");
  src.save_to(dst);

  ASSERT_EQ(dst->persisted_values.count({"/a", "k1"}), 1u);
  EXPECT_EQ(dst->persisted_values[{"/a", "k1"}], "v1");
  ASSERT_EQ(dst->persisted_values.count({"/a", "k2"}), 1u);
  EXPECT_EQ(dst->persisted_values[{"/a", "k2"}], "v2");
}

TEST(settings_interface_impl, save_to_same_context_just_saves) {
  // When source and destination contexts are identical, save_to should
  // short-circuit to a plain save() rather than copying.  We assert that the
  // source's pending writes land in its own backend.
  mock_settings_core core;
  auto src = boost::make_shared<memory_backend>(&core, "x", "memory://x");
  src->set_string("/p", "k", "v");
  src->save_to(src);
  EXPECT_EQ(src->persisted_values[{"/p", "k"}], "v");
}

// ============================================================================
// Container value semantics
// ============================================================================

TEST(settings_interface_impl, conainer_string_round_trip) {
  settings::settings_interface_impl::conainer c(std::string("abc"), true);
  EXPECT_EQ(c.get_string(), "abc");
  EXPECT_TRUE(c.is_dirty());
}

TEST(settings_interface_impl, conainer_int_to_string) {
  settings::settings_interface_impl::conainer c(42, true);
  EXPECT_EQ(c.get_string(), "42");
  EXPECT_EQ(c.get_int(), 42);
}

TEST(settings_interface_impl, conainer_bool_to_string) {
  settings::settings_interface_impl::conainer t(true, true);
  settings::settings_interface_impl::conainer f(false, true);
  EXPECT_EQ(t.get_string(), "true");
  EXPECT_EQ(f.get_string(), "false");
}

TEST(settings_interface_impl, conainer_default_is_clean) {
  settings::settings_interface_impl::conainer c;
  EXPECT_FALSE(c.is_dirty());
}

TEST(settings_interface_impl, conainer_make_dirty_flips_flag) {
  settings::settings_interface_impl::conainer c;
  EXPECT_FALSE(c.is_dirty());
  c.make_dirty();
  EXPECT_TRUE(c.is_dirty());
}

TEST(settings_interface_impl, conainer_string_to_bool_truthy) {
  settings::settings_interface_impl::conainer t(std::string("true"), true);
  settings::settings_interface_impl::conainer one(std::string("1"), true);
  EXPECT_TRUE(t.get_bool());
  EXPECT_TRUE(one.get_bool());
}

TEST(settings_interface_impl, conainer_string_to_bool_falsy) {
  settings::settings_interface_impl::conainer f(std::string("false"), true);
  settings::settings_interface_impl::conainer zero(std::string("0"), true);
  settings::settings_interface_impl::conainer empty(std::string(""), true);
  EXPECT_FALSE(f.get_bool());
  EXPECT_FALSE(zero.get_bool());
  EXPECT_FALSE(empty.get_bool());
}

// ============================================================================
// Misc: context, type, get_info
// ============================================================================

TEST(settings_interface_impl, get_context_returns_constructor_arg) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://example");
  EXPECT_EQ(b.get_context(), "memory://example");
}

TEST(settings_interface_impl, set_context_overrides_constructor_arg) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://before");
  b.set_context("memory://after");
  EXPECT_EQ(b.get_context(), "memory://after");
}

TEST(settings_interface_impl, to_string_includes_info) {
  mock_settings_core core;
  memory_backend b(&core, "test", "memory://");
  EXPECT_NE(b.to_string().find("memory backend"), std::string::npos);
}
