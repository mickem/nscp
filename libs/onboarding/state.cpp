// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <onboarding/onboarding.hpp>
#include <sstream>

#include "json_util.hpp"

#ifdef WIN32
#include <cstdio>
#include <io.h>
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
