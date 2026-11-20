// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace icinga_agent {

// Normalise a certificate fingerprint as an operator writes it - the output
// of `openssl x509 -noout -fingerprint -sha256` ("SHA256 Fingerprint=AB:CD:.."),
// with or without the prefix, colons or spaces, in either case - to 64
// lower-case hex digits. False when it is not a SHA-256 fingerprint.
bool normalize_fingerprint(const std::string &text, std::string &out);

}  // namespace icinga_agent
