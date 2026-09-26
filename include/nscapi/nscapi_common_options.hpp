// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#define DEFAULT_PASSWORD_NAME "Password"
// The shared password the *inbound* protocols check a caller against - the WEB
// server and check_nt. It is not key material: the protocols that encrypt with
// a shared secret rather than verifying one (NSCA) keep their key in their own
// section, because a hash cannot be a key.
#define DEFAULT_PASSWORD_DESC                                                                                                        \
  "Password an inbound caller has to present. Stored hashed (pbkdf2-sha256$...) when written by `nscp web install` or `nscp web " \
  "password --set`; a clear-text value written by hand is still accepted, and is hashed in place when re-set. This is a password "    \
  "to verify against, not key material: NSCA encrypts with its shared secret instead of checking it, so it keeps its own key "        \
  "under /settings/NSCA/server (or the NSCAClient default target) and never reads this one."
