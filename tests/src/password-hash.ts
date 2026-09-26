// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/**
 * The stored password form, and reading a value back out of an INI.
 *
 * `pbkdf2-sha256$<iterations>$<saltHex>$<hashHex>` is what
 * `include/nscp/password_hash.cpp` writes, and these helpers are the
 * TypeScript side of that one format: a test either seeds a stored hash for
 * the agent to verify against, or checks that something the agent wrote is one.
 * Both were copy-pasted into several suites before, which is how the C++ parser
 * came to accept hex these regexes reject.
 */
import * as crypto from "crypto";
import * as fs from "fs";

/** The parameters password_hash.cpp uses: 100k iterations, 16-byte salt, 32-byte hash. */
export const PBKDF2_ITERATIONS = 100000;
const SALT_BYTES = 16;
const HASH_BYTES = 32;

/**
 * The stored form, anchored. Either case of hex, matching what
 * `password_hash.cpp` parses - the writers emit lower case, but a value written
 * by hand is still a hash and the three implementations of this check (here,
 * the C++ parser and tests/msi/helpers.py) have to agree on that.
 */
export const STORED_HASH_RE = /^pbkdf2-sha256\$(\d+)\$([0-9a-fA-F]+)\$([0-9a-fA-F]+)$/;

/** A stored hash of `password`, with a fresh random salt. */
export function pbkdf2StoredForm(password: string, iterations: number = PBKDF2_ITERATIONS): string {
  const salt = crypto.randomBytes(SALT_BYTES);
  const hash = crypto.pbkdf2Sync(password, salt, iterations, HASH_BYTES, "sha256");
  return `pbkdf2-sha256$${iterations}$${salt.toString("hex")}$${hash.toString("hex")}`;
}

/** True when `value` has the stored form at all, whatever it is a hash of. */
export function isStoredHash(value: string | undefined): boolean {
  return !!value && STORED_HASH_RE.test(value);
}

/** True when `stored` is a stored hash *of* `password` — the KDF re-run over its own salt. */
export function storedHashMatches(stored: string | undefined, password: string): boolean {
  const m = stored?.match(STORED_HASH_RE);
  if (!m) return false;
  const [, iterations, saltHex, hashHex] = m;
  // An odd-length field is not hex, and asking node for half a byte throws.
  if (saltHex.length % 2 !== 0 || hashHex.length % 2 !== 0) return false;
  const derived = crypto.pbkdf2Sync(
    password,
    Buffer.from(saltHex, "hex"),
    Number(iterations),
    hashHex.length / 2,
    "sha256",
  );
  return derived.toString("hex") === hashHex;
}

/** Value of `key` under `[section]` in an INI file, undefined when absent. */
export function iniValue(file: string, section: string, key: string): string | undefined {
  let inSection = false;
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";") || line.startsWith("#")) continue;
    if (line.startsWith("[")) {
      inSection = line === `[${section}]`;
      continue;
    }
    if (!inSection) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    if (line.slice(0, eq).trim() === key) return line.slice(eq + 1).trim();
  }
  return undefined;
}
