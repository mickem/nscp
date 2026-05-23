#pragma once

#include <string>
#include <utility>

// Shared cert/key file loading used by both `ServerMongooseImpl` and
// `ServerBeastImpl`. Kept private to libs/mongoose-cpp/ (no
// NSCAPI_EXPORT) — only the two server implementations need it, and
// keeping it out of the public surface avoids growing the wrapper
// while we still expect to delete the mongoose backend (Phase 5).

namespace Mongoose {
namespace cert_loader {

/// Read the contents of `path` into a string. Throws nsclient_exception
/// (with `hint` woven into the message) on I/O failure.
std::string load_file(const std::string& path, const std::string& hint);

/// Load a certificate (and optionally a private-key) pair from disk.
/// When `key_path` is empty the returned key buffer is the same string
/// as the certificate buffer — matches the behaviour of the original
/// load_certificates helper in ServerMongooseImpl.cpp.
std::pair<std::string, std::string> load_certificates(const std::string& cert_path, const std::string& key_path);

}  // namespace cert_loader
}  // namespace Mongoose
