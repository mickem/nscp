// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

// Helpers for hashing passwords the agent stores in its settings. Format:
//
//   pbkdf2-sha256$<iterations>$<saltHex>$<hashHex>
//
// Used for the per-user WEB passwords under /settings/WEB/server/users/<u>
// and for the shared /settings/default/password that `nscp web install` and
// `nscp web password --set` write. Every reader that only has to *compare* a
// password (the WEB server, the check_nt server) verifies through
// verify_password() and so accepts either form. NSCAServer derives its
// transport key from the password itself, so it needs the clear-text value and
// refuses a hashed one (set a clear-text password under /settings/NSCA/server
// on such an agent).
namespace password_hash {
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
}  // namespace password_hash
