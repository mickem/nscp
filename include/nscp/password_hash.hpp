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
// True if the value is a complete stored hash: the prefix above, an iteration
// count in range, and a salt and a hash that are both non-empty hex. This is
// the question every *writer* has - "is this already hashed, or a password I
// still have to hash?" - and the prefix alone cannot answer it: a password may
// legitimately start with "pbkdf2-sha256$" (it is only text), and storing such
// a password unchanged would leave a value nothing can verify, locking the
// operator out of an agent whose command reported success.
bool is_hashed(const std::string& s);

// Returns the hashed form, or an empty string on failure (RNG / KDF). When
// built without OpenSSL, returns the input unchanged so the WEB module still
// links - the on-disk value is then stored in cleartext (with a constant-time
// compare on login). Hash-on-disk requires USE_SSL.
std::string hash_password(const std::string& password);

// Verifies a plaintext password against the stored value. Accepts either a
// hashed value (verified with PBKDF2) or a legacy plaintext value (compared
// constant-time). A value that carries the prefix but does not parse is a
// damaged hash rather than a password that happens to look like one, and is
// rejected: the stored string is never a credential in its own right. Writers
// keep that case from arising by hashing anything is_hashed() rejects.
bool verify_password(const std::string& password, const std::string& stored);
}  // namespace password_hash
