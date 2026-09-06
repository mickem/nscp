// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/filesystem/path.hpp>
#include <string>

// Staging an uploaded body on disk so a module can `--import` it.
//
// The scripts API used to write the upload to `${temp}/<script name>` with a
// plain truncating ofstream. `${temp}` is the *shared* temp directory (`/tmp`,
// or `C:\Windows\Temp` for a SYSTEM service), the name was predictable from
// the script name, and neither the open nor the write was checked. Anyone with
// a local account could therefore create that file first: the service's open
// then fails (a sticky, world-writable directory with `fs.protected_regular`
// on, or an attacker-set DACL on Windows), the failure went unnoticed, and the
// module imported whatever the attacker had left at the path - registering it
// as a command that runs as the service account. Where the open did succeed,
// the attacker kept ownership and could rewrite the content in the window
// before the import read it back.
//
// This helper closes both doors: the name is random, so it cannot be planted
// in advance; the file is created exclusively (O_EXCL / CREATE_NEW, and never
// through a symlink), so a name that somehow already exists is an error rather
// than a reuse; it is owner-only from the first instant; and every write is
// checked, so the caller either gets a path whose content is exactly the
// upload or an error.
namespace upload_staging {

// Create `path` exclusively for writing. Fails if anything already exists at
// `path` (including a symlink or a directory entry of any kind), and never
// follows a symlink. On POSIX the file is created 0600. Returns a stdio stream
// positioned at offset 0, or nullptr with `error` set.
FILE *create_exclusive(const boost::filesystem::path &path, std::string &error);

// Write `content` into a newly created, randomly named file under `directory`
// and return its path. On any failure returns an empty path with `error`
// describing why, and leaves nothing behind. The caller owns the file and is
// expected to remove it once it has been consumed.
boost::filesystem::path stage(const boost::filesystem::path &directory, const std::string &content, std::string &error);

}  // namespace upload_staging
