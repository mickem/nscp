// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <onboarding/onboarding.hpp>
#include <sstream>
#include <vector>

#include "json_util.hpp"

#ifdef WIN32
#include <io.h>

#include <cstdio>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace json = boost::json;
namespace fs = boost::filesystem;

namespace {

const std::int64_t state_version = 1;

std::string serialize_state(const onboarding::enrolled_identity &state) {
  json::object object;
  object["version"] = state_version;
  object["private_key_pem"] = state.private_key_pem;
  object["cert_pem"] = state.cert_pem;
  object["ca_pem"] = state.ca_pem;
  object["bundle_signing_pub_pem"] = state.bundle_signing_pub_pem;
  object["server_url"] = state.server_url;
  object["mtls_url"] = state.mtls_url;
  object["mtls_server_cert_pem"] = state.mtls_server_cert_pem;
  return json::serialize(object);
}

// `required` fields carry the identity itself: an empty one would leave the
// agent trying to hand OpenSSL an empty key or to call an empty url, so a
// truncated (or hand-edited) state file is rejected outright instead.
std::string read_state_string(const json::object &object, const char *key, const bool required = true) {
  const json::value *value = object.if_contains(key);
  if (value == nullptr || !value->is_string()) {
    throw std::runtime_error(std::string("missing field ") + key);
  }
  std::string result = onboarding::detail::to_string(value->as_string());
  if (required && result.empty()) {
    throw std::runtime_error(std::string("empty field ") + key);
  }
  return result;
}

// Write `data` to `path` and flush it all the way to disk. The state file
// holds the only copy of the private key so a torn write here would strand
// the host; on POSIX the file is created 0600 as it holds secret material.
void write_file_durable(const std::string &path, const std::string &data) {
#ifdef WIN32
  FILE *file = nullptr;
  if (fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr) {
    throw onboarding::onboarding_error("Failed to open " + path + " for writing", false);
  }
  const bool ok = fwrite(data.data(), 1, data.size(), file) == data.size() && fflush(file) == 0 && _commit(_fileno(file)) == 0;
  fclose(file);
#else
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) {
    throw onboarding::onboarding_error("Failed to open " + path + " for writing", false);
  }
  bool ok = true;
  std::size_t offset = 0;
  while (ok && offset < data.size()) {
    const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
    if (written < 0) {
      ok = false;
    } else {
      offset += static_cast<std::size_t>(written);
    }
  }
  ok = ok && ::fsync(fd) == 0;
  ::close(fd);
#endif
  if (!ok) {
    boost::system::error_code ignored;
    fs::remove(path, ignored);
    throw onboarding::onboarding_error("Failed to write " + path, false);
  }
}

}  // namespace

void onboarding::save_state(const enrolled_identity &state, const std::string &path) {
  const std::string tmp = path + ".tmp";
  write_file_durable(tmp, serialize_state(state));
  boost::system::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    boost::system::error_code ignored;
    fs::remove(tmp, ignored);
    throw onboarding_error("Failed to move " + tmp + " to " + path + ": " + ec.message(), false);
  }
}

#ifndef WIN32
namespace {

// Give a directory and everything under it to (uid, gid), following no symlink
// at any level. `dir_fd` is a descriptor already opened O_NOFOLLOW, which this
// consumes (closes). Every entry is chowned through its parent's descriptor
// with AT_SYMLINK_NOFOLLOW, and every subdirectory is re-opened O_NOFOLLOW, so a
// component the (untrusted) service account swaps for a symlink between our
// steps is rejected rather than traversed - closing the root-chown-follows-
// symlink escalation, including the intermediate-directory variant a single
// lchown-by-path could not.
bool chown_subtree(int dir_fd, uid_t uid, gid_t gid, const std::string &label, std::string &error) {
  if (::fchown(dir_fd, uid, gid) != 0) {
    error = "Failed to change the owner of " + label + ": " + std::strerror(errno);
    ::close(dir_fd);
    return false;
  }
  DIR *dir = ::fdopendir(dir_fd);  // takes ownership of dir_fd
  if (dir == nullptr) {
    error = "Failed to read " + label + ": " + std::strerror(errno);
    ::close(dir_fd);
    return false;
  }
  bool ok = true;
  while (const dirent *entry = ::readdir(dir)) {
    const std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    struct stat st = {};
    if (::fstatat(dir_fd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
      error = "Failed to inspect " + label + "/" + name + ": " + std::strerror(errno);
      ok = false;
      break;
    }
    if (S_ISDIR(st.st_mode)) {
      const int child = ::openat(dir_fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC | O_NONBLOCK);
      if (child < 0) {
        error = "Failed to open " + label + "/" + name + ": " + std::strerror(errno);
        ok = false;
        break;
      }
      if (!chown_subtree(child, uid, gid, label + "/" + name, error)) {  // consumes child
        ok = false;
        break;
      }
    } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {
      // A hardlinked file (st_nlink > 1) is a second name for an inode that may
      // live anywhere; nothing we write is ever hardlinked, so leave it alone.
      if (::fchownat(dir_fd, name.c_str(), uid, gid, AT_SYMLINK_NOFOLLOW) != 0) {
        error = "Failed to change the owner of " + label + "/" + name + ": " + std::strerror(errno);
        ok = false;
        break;
      }
    }
    // Symlinks, devices, fifos and multiply-linked files are skipped.
  }
  ::closedir(dir);  // closes dir_fd
  return ok;
}

}  // namespace
#endif

bool onboarding::adopt_owner(const std::string &target, const std::string &reference, std::string &error) {
#ifdef WIN32
  // Windows has no equivalent handoff: the service runs as LocalSystem and the
  // installer writes as SYSTEM, so the material is readable as written.
  static_cast<void>(target);
  static_cast<void>(reference);
  static_cast<void>(error);
  return true;
#else
  if (::geteuid() != 0) {
    // Only root can give a file away, and a non-root enrollment already writes
    // as whoever will read it.
    return true;
  }
  struct stat reference_stat = {};
  if (::stat(reference.c_str(), &reference_stat) != 0) {
    // No reference to copy from (a from-source install that never created the
    // state directory). Leaving ownership alone is the safe answer.
    return true;
  }
  if (reference_stat.st_uid == 0 && reference_stat.st_gid == 0) {
    // Root owns the reference too, so there is nothing to hand over - this is
    // an install that genuinely runs everything as root.
    return true;
  }

  // Everything below runs as root over a tree owned by the unprivileged service
  // account - paths that account can replace between our inspecting them and our
  // touching them. Addressing anything by path string is therefore unsafe: the
  // ordinary root-chown-follows-symlink escalation (plant
  // `ln -s /etc/shadow ${fleet-folder}/x`, and even swap an intermediate
  // directory for a symlink) hands the target to nsclient:nsclient.
  //
  // So anchor on the reference directory and never touch anything by path again.
  // `reference` is ${data-path}, whose parent is root-owned (packaging creates
  // it under a root directory), so it cannot be swapped; opening it O_NOFOLLOW
  // and descending with openat(O_NOFOLLOW) keeps every step provably inside the
  // tree even though the tree itself is untrusted.
  const int base = ::open(reference.c_str(), O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
  if (base < 0) {
    // The reference is not a directory we can open (a symlink, or gone). With no
    // trusted anchor, leave ownership alone rather than resolve a path we do not
    // trust.
    return true;
  }

  // The callers always pass a path within the reference; refuse anything else
  // rather than fall back to an unanchored (unsafe) resolution.
  const fs::path rel = fs::path(target).lexically_relative(reference);
  if (rel.empty() || rel.begin()->string() == "..") {
    ::close(base);
    error = "Refusing to change the owner of " + target + ": it is not within " + reference;
    return false;
  }
  std::vector<std::string> parts;
  for (const fs::path &part : rel) {
    if (part.string() != ".") parts.push_back(part.string());
  }
  if (parts.empty()) {
    // target == reference: hand over the whole reference tree.
    return chown_subtree(base, reference_stat.st_uid, reference_stat.st_gid, target, error);  // consumes base
  }

  // Descend to the target's parent, following no symlink at any level.
  int parent_fd = base;
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    const int next = ::openat(parent_fd, parts[i].c_str(), O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC | O_NONBLOCK);
    ::close(parent_fd);
    if (next < 0) {
      error = "Failed to open " + reference + " component '" + parts[i] + "': " + std::strerror(errno);
      return false;
    }
    parent_fd = next;
  }

  const std::string &name = parts.back();
  struct stat st = {};
  if (::fstatat(parent_fd, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
    const bool absent = errno == ENOENT;
    if (!absent) error = "Failed to inspect " + target + ": " + std::strerror(errno);
    ::close(parent_fd);
    return absent;  // nothing there to hand over is success
  }
  if (S_ISLNK(st.st_mode)) {
    ::close(parent_fd);
    error = "Refusing to change the owner of " + target + ": it is a symbolic link";
    return false;
  }
  if (S_ISDIR(st.st_mode)) {
    const int fd = ::openat(parent_fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC | O_NONBLOCK);
    ::close(parent_fd);
    if (fd < 0) {
      error = "Failed to open " + target + ": " + std::strerror(errno);
      return false;
    }
    return chown_subtree(fd, reference_stat.st_uid, reference_stat.st_gid, target, error);  // consumes fd
  }
  if (S_ISREG(st.st_mode) && st.st_nlink == 1) {
    const bool ok = ::fchownat(parent_fd, name.c_str(), reference_stat.st_uid, reference_stat.st_gid, AT_SYMLINK_NOFOLLOW) == 0;
    if (!ok) error = "Failed to change the owner of " + target + ": " + std::strerror(errno);
    ::close(parent_fd);
    return ok;
  }
  // A device, fifo, or a hardlinked file: nothing of ours to hand over.
  ::close(parent_fd);
  return true;
#endif
}

boost::optional<onboarding::enrolled_identity> onboarding::load_state(const std::string &path) {
  boost::system::error_code ec;
  if (!fs::exists(path, ec)) {
    return boost::none;
  }
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in) {
    throw onboarding_error("Failed to read state file " + path, false);
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  try {
    const json::object root = json::parse(buffer.str()).as_object();
    const json::value *version = root.if_contains("version");
    if (version == nullptr || !version->is_int64() || version->as_int64() != state_version) {
      throw std::runtime_error("unsupported state file version");
    }
    enrolled_identity result;
    result.private_key_pem = read_state_string(root, "private_key_pem");
    result.cert_pem = read_state_string(root, "cert_pem");
    result.ca_pem = read_state_string(root, "ca_pem");
    result.bundle_signing_pub_pem = read_state_string(root, "bundle_signing_pub_pem");
    // The public api url is optional: enrollment falls back to the url the
    // operator passed, which may itself have been empty.
    result.server_url = read_state_string(root, "server_url", false);
    result.mtls_url = read_state_string(root, "mtls_url");
    result.mtls_server_cert_pem = read_state_string(root, "mtls_server_cert_pem");
    return result;
  } catch (const std::exception &e) {
    throw onboarding_error("State file " + path + " is corrupt: " + e.what(), false);
  }
}
