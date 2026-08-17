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
  // account, which is to say over paths that account can replace between our
  // deciding to walk them and our touching them. That makes the ordinary
  // root-chown-follows-symlink escalation available: plant
  // `ln -s /etc/shadow ${fleet-folder}/x` and the next `sudo nscp enroll` hands
  // /etc/shadow to nsclient:nsclient. So: never traverse a symlink, and never
  // resolve one when changing ownership.
  boost::system::error_code ec;
  const fs::file_status status = fs::symlink_status(target, ec);
  if (ec || !fs::exists(status)) {
    // Nothing there to hand over.
    return true;
  }
  if (fs::is_symlink(status)) {
    error = "Refusing to change the owner of " + target + ": it is a symbolic link";
    return false;
  }

  std::vector<fs::path> targets;
  targets.push_back(target);
  if (fs::is_directory(status)) {
    // recursive_directory_iterator does not descend into symlinked directories
    // by default, so the walk itself stays inside the tree; the entries it
    // yields still have to be handled one at a time below.
    for (fs::recursive_directory_iterator it(target, ec), end; it != end && !ec; it.increment(ec)) {
      targets.push_back(it->path());
    }
  }

  for (const fs::path &path : targets) {
    struct stat entry_stat = {};
    if (::lstat(path.string().c_str(), &entry_stat) != 0) {
      error = "Failed to inspect " + path.string() + ": " + std::strerror(errno);
      return false;
    }
    // Only regular files and directories are ours to give away. Skipping the
    // rest - symlinks above all - keeps the rule simple: nothing reachable
    // from this tree but living outside it can be affected.
    if (!S_ISREG(entry_stat.st_mode) && !S_ISDIR(entry_stat.st_mode)) continue;
    // A hardlink is a second name for a file that may live anywhere, and
    // chowning it changes the owner of that one file everywhere it is named.
    // Nothing we write here is ever hardlinked, so an extra link means someone
    // else made it.
    if (S_ISREG(entry_stat.st_mode) && entry_stat.st_nlink > 1) continue;

    // lchown, not chown: on a symlink that slipped past the checks above this
    // retargets the link itself rather than whatever it points at.
    if (::lchown(path.string().c_str(), reference_stat.st_uid, reference_stat.st_gid) != 0) {
      error = "Failed to change the owner of " + path.string() + ": " + std::strerror(errno);
      return false;
    }
  }
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
