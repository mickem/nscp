#pragma once

#include <string>

// Helpers for hashing per-user WEB passwords. Format:
//
//   pbkdf2-sha256$<iterations>$<saltHex>$<hashHex>
//
// The same format is used by user_manager when comparing on login. ONLY the
// per-user password under /settings/WEB/server/users/<u>/password should be
// hashed - the global /settings/default/password is shared with NRPE / NSCA /
// NSClient and must remain in its on-disk form for those modules to read it.
namespace web_password {
// True if the value already looks like a stored hash (has the prefix above).
bool is_hashed(const std::string& s);

// Returns the hashed form, or an empty string on failure (RNG / KDF). When
// built without OpenSSL, returns the input unchanged so the WEB module still
// links - the on-disk value is then stored in cleartext (with a constant-time
// compare on login). Hash-on-disk requires USE_SSL.
std::string hash_password(const std::string& password);

// Verifies a plaintext password against the stored value. Accepts either a
// hashed value (verified with PBKDF2) or a legacy plaintext value (compared
// constant-time).
bool verify_password(const std::string& password, const std::string& stored);
}  // namespace web_password
