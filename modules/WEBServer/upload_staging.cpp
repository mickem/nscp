// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "upload_staging.hpp"

#include <boost/filesystem.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>

#ifdef WIN32
#include <windows.h>
// windows.h first; io.h/fcntl.h build on its types.
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = boost::filesystem;

namespace upload_staging {

namespace {

std::string errno_text() {
  const char *msg = std::strerror(errno);
  return msg ? msg : "unknown error";
}

// Enough random name attempts to make a collision a non-event, and few enough
// that a directory we cannot write to fails promptly with its real error.
const int max_attempts = 4;

}  // namespace

FILE *create_exclusive(const fs::path &path, std::string &error) {
#ifdef WIN32
  // CREATE_NEW: the call fails if anything already exists at the path.
  // FILE_FLAG_OPEN_REPARSE_POINT: and "exists" includes a symbolic link or
  // junction sitting there, rather than looking through it at its target.
  // No sharing: nothing else can open the file while it is being written.
  const HANDLE handle =
      ::CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    error = "CreateFile(" + path.string() + ") failed: error=" + std::to_string(::GetLastError());
    return nullptr;
  }
  const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle), _O_WRONLY | _O_BINARY);
  if (fd == -1) {
    ::CloseHandle(handle);
    error = "_open_osfhandle(" + path.string() + ") failed: " + errno_text();
    return nullptr;
  }
  FILE *file = ::_fdopen(fd, "wb");
  if (file == nullptr) {
    error = "_fdopen(" + path.string() + ") failed: " + errno_text();
    ::_close(fd);
    return nullptr;
  }
  return file;
#else
  // O_EXCL: fail if the name exists. O_NOFOLLOW: a symlink at the name counts
  // as existing rather than being followed to wherever it points. 0600: the
  // content is only ever meant for this process (and the module it hands the
  // path to, which runs in it).
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) {
    error = "open(" + path.string() + ") failed: " + errno_text();
    return nullptr;
  }
  FILE *file = ::fdopen(fd, "wb");
  if (file == nullptr) {
    error = "fdopen(" + path.string() + ") failed: " + errno_text();
    ::close(fd);
    return nullptr;
  }
  return file;
#endif
}

fs::path stage(const fs::path &directory, const std::string &content, std::string &error) {
  boost::system::error_code ec;
  if (!fs::is_directory(directory, ec)) {
    error = "staging directory " + directory.string() + " does not exist or is not a directory";
    return fs::path();
  }

  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    // 64 bits of randomness in the name: nobody can pre-create it, and nobody
    // watching the directory learns anything from the pattern.
    const fs::path candidate = directory / fs::unique_path("nscp-upload-%%%%%%%%%%%%%%%%");

    FILE *file = create_exclusive(candidate, error);
    if (file == nullptr) continue;  // taken, or not writable: `error` says which; try once more

    const bool written = content.empty() || std::fwrite(content.data(), 1, content.size(), file) == content.size();
    const bool flushed = std::fflush(file) == 0;
    const std::string write_error = (written && flushed) ? "" : errno_text();
    const bool closed = std::fclose(file) == 0;
    if (written && flushed && closed) {
      error.clear();
      return candidate;
    }
    // Half an upload is worse than none: the module would import it as-is.
    fs::remove(candidate, ec);
    error = "failed to write " + candidate.string() + ": " + (write_error.empty() ? errno_text() : write_error);
    return fs::path();
  }
  return fs::path();
}

}  // namespace upload_staging
