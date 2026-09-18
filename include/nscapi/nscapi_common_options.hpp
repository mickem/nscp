// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#define DEFAULT_PASSWORD_NAME "Password"
#define DEFAULT_PASSWORD_DESC                                                                                                                     \
  "Password used to authenticate against server. Stored hashed (pbkdf2-sha256$...) when written by `nscp web install` or `nscp web "         \
  "password --set`; a clear-text value is still accepted. NSCA derives its encryption key from the clear-text value, so an agent "           \
  "that serves NSCA keeps a clear-text password under /settings/NSCA/server."