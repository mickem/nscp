// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <files/safe_file.hpp>
#include <fstream>
#include <istream>
#include <memory>
#include <sstream>
#include <string>

// check_logfile's reader: the most demanding user of the stream (it seeks).
#include "file_reader.hpp"

#ifdef _WIN32
#include <windows.h>

#include <str/utf8.hpp>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

using safe_file::device_streambuf;
using safe_file::is_win32_device_path;
using safe_file::open_stream;
using safe_file::read_all;
using safe_file::read_status;
namespace fr = check_logfile::file_reader;

namespace {

// An in-memory device, so the streambuf's buffer and seek bookkeeping are
// exercised on every OS.
struct memory_device {
  std::string data;
  std::size_t pos = 0;
  int reads = 0;
  bool read(char *buf, std::size_t len, std::size_t &got) {
    ++reads;
    got = std::min(len, data.size() - std::min(pos, data.size()));
    if (got > 0) data.copy(buf, got, pos);
    pos += got;
    return true;
  }
  bool seek(std::uint64_t offset) {
    pos = static_cast<std::size_t>(offset);
    return true;
  }
  bool size(std::uint64_t &out) {
    out = data.size();
    return true;
  }
};

const std::string kLog = "alpha\nbravo\r\ncharlie\ndelta\necho\nfoxtrot\ngolf\nhotel\n" + std::string(200, 'x') + "\nlast-no-newline";

// Every buffer size from a single byte up to larger than the content, so a
// delimiter, a seek target or a tail scan chunk falls on every boundary.
const std::size_t kBufferSizes[] = {1, 2, 3, 5, 7, 16, 64, 4096};

}  // namespace

TEST(DeviceStreambuf, ReadsLikeAStringStream) {
  for (std::size_t bs : kBufferSizes) {
    device_streambuf<memory_device> buf(memory_device{kLog}, bs);
    std::istream is(&buf);
    std::string got((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    EXPECT_EQ(kLog, got) << "buffer " << bs;
  }
}

TEST(DeviceStreambuf, SeekAndTellMatchAStringStream) {
  for (std::size_t bs : kBufferSizes) {
    device_streambuf<memory_device> buf(memory_device{kLog}, bs);
    std::istream is(&buf);
    std::istringstream ref(kLog);
    EXPECT_EQ(0, is.tellg());

    char a[10] = {}, b[10] = {};
    is.read(a, 9);
    ref.read(b, 9);
    EXPECT_EQ(std::string(b, 9), std::string(a, 9)) << "buffer " << bs;
    EXPECT_EQ(ref.tellg(), is.tellg()) << "buffer " << bs;

    is.seekg(0, std::ios::end);
    EXPECT_EQ(static_cast<std::streamoff>(kLog.size()), static_cast<std::streamoff>(is.tellg()));

    // Backwards, forwards, relative and inside the buffer.
    for (std::streamoff target : {std::streamoff(3), std::streamoff(0), std::streamoff(40), std::streamoff(41), std::streamoff(12)}) {
      is.clear();
      is.seekg(target, std::ios::beg);
      ASSERT_EQ(target, static_cast<std::streamoff>(is.tellg()));
      std::string line, ref_line;
      ref.clear();
      ref.seekg(target, std::ios::beg);
      fr::getline_str(is, line, "\n");
      fr::getline_str(ref, ref_line, "\n");
      EXPECT_EQ(ref_line, line) << "buffer " << bs << " offset " << target;
      EXPECT_EQ(ref.tellg(), is.tellg()) << "buffer " << bs << " offset " << target;
    }
    is.clear();
    is.seekg(5, std::ios::beg);
    is.seekg(-2, std::ios::cur);
    EXPECT_EQ(3, static_cast<std::streamoff>(is.tellg()));
    EXPECT_EQ('h', is.get());
    is.seekg(-4, std::ios::end);
    EXPECT_EQ('l', is.get());
  }
}

TEST(DeviceStreambuf, RefusesNegativeAndWriteSeeks) {
  device_streambuf<memory_device> buf(memory_device{kLog}, 8);
  std::istream is(&buf);
  is.seekg(-1, std::ios::beg);
  EXPECT_TRUE(is.fail());
  is.clear();
  EXPECT_EQ(std::streampos(std::streamoff(-1)), buf.pubseekoff(0, std::ios::beg, std::ios::out));
}

TEST(DeviceStreambuf, TellAndSeekInsideTheBuffer) {
  device_streambuf<memory_device> buf(memory_device{kLog}, 16);
  std::istream is(&buf);
  is.get();
  EXPECT_EQ(1, static_cast<std::streamoff>(is.tellg()));
  is.seekg(4);
  EXPECT_EQ('a', is.get());
}

TEST(DeviceStreambuf, FileReaderHelpersAgreeWithAStringStream) {
  for (std::size_t bs : kBufferSizes) {
    for (const std::string &delim : {std::string("\n"), std::string("\r\n"), std::string("o\n")}) {
      for (std::uint64_t max : {1, 2, 3, 100}) {
        device_streambuf<memory_device> buf(memory_device{kLog}, bs);
        std::istream is(&buf);
        std::istringstream ref(kLog);
        const std::uint64_t off = fr::find_tail_offset(is, delim, max);
        EXPECT_EQ(fr::find_tail_offset(ref, delim, max), off) << "buffer " << bs;

        is.clear();
        ref.clear();
        is.seekg(static_cast<std::streamoff>(off));
        ref.seekg(static_cast<std::streamoff>(off));
        bool t1 = false, t2 = false;
        EXPECT_EQ(fr::read_capped(ref, 100, t2), fr::read_capped(is, 100, t1)) << "buffer " << bs;
        EXPECT_EQ(t2, t1);

        is.clear();
        ref.clear();
        is.seekg(0);
        ref.seekg(0);
        EXPECT_EQ(fr::read_leading_records(ref, delim, max), fr::read_leading_records(is, delim, max)) << "buffer " << bs;
      }
    }
  }
}

namespace {
struct temp_file {
  std::string path;
  explicit temp_file(const std::string &contents) {
    path = ::testing::TempDir() + "nscp_safe_file_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".log";
    std::ofstream out(path.c_str(), std::ios::binary);
    out << contents;
  }
  ~temp_file() { std::remove(path.c_str()); }
};
}  // namespace

TEST(SafeFile, ReadsAndSeeksARegularFile) {
  const temp_file f(kLog);
  std::string error;
  const std::unique_ptr<std::streambuf> buf = open_stream(f.path, error);
  ASSERT_TRUE(buf) << error;
  std::istream is(buf.get());
  is.seekg(0, std::ios::end);
  EXPECT_EQ(static_cast<std::streamoff>(kLog.size()), static_cast<std::streamoff>(is.tellg()));
  is.seekg(6);
  std::string line;
  ASSERT_TRUE(fr::getline_str(is, line, "\n"));
  EXPECT_EQ("bravo", line);
}

TEST(SafeFile, AMissingFileIsAnErrorNamingIt) {
  const std::string path = ::testing::TempDir() + "nscp_safe_file_test_does_not_exist.log";
  std::string error;
  EXPECT_FALSE(open_stream(path, error));
  EXPECT_EQ(0u, error.find("Failed to open file: " + path)) << error;
}

// --- read_all: the bounded whole-file read ---------------------------------

TEST(SafeFile, ReadAllReadsTheFile) {
  const temp_file f(kLog);
  std::string out, error;
  ASSERT_EQ(read_status::ok, read_all(f.path, 0, out, error)) << error;
  EXPECT_EQ(kLog, out);
  ASSERT_EQ(read_status::ok, read_all(f.path, kLog.size(), out, error)) << error;
  EXPECT_EQ(kLog, out);
}

TEST(SafeFile, ReadAllRefusesAFileOverTheLimit) {
  const temp_file f(kLog);
  std::string out, error;
  EXPECT_EQ(read_status::too_large, read_all(f.path, kLog.size() - 1, out, error));
  EXPECT_TRUE(error.empty()) << error;
}

TEST(SafeFile, ReadAllReportsAMissingFile) {
  std::string out, error;
  EXPECT_EQ(read_status::failed, read_all(::testing::TempDir() + "nscp_safe_file_test_missing", 0, out, error));
  EXPECT_NE(std::string::npos, error.find("Failed to open file")) << error;
}

#ifndef _WIN32
TEST(SafeFilePosix, CharacterDeviceIsRefusedBeforeAnythingIsRead) {
  // /dev/zero reports a size of 0 and never ends.
  std::string out, error;
  EXPECT_EQ(read_status::failed, read_all("/dev/zero", 0, out, error));
  EXPECT_EQ("Not a regular file: /dev/zero", error);
  EXPECT_TRUE(out.empty());
}

TEST(SafeFilePosix, SymlinkToADeviceIsRefused) {
  const std::string link = ::testing::TempDir() + "nscp_safe_file_test_link";
  ::unlink(link.c_str());
  ASSERT_EQ(0, ::symlink("/dev/zero", link.c_str()));
  std::string error;
  EXPECT_FALSE(open_stream(link, error));
  ::unlink(link.c_str());
  EXPECT_EQ("Not a regular file: " + link, error);
}

TEST(SafeFilePosix, FifoIsRefusedWithoutBlocking) {
  // Opening a FIFO with no writer blocks a plain open() forever.
  const std::string fifo = ::testing::TempDir() + "nscp_safe_file_test_fifo";
  ::unlink(fifo.c_str());
  ASSERT_EQ(0, ::mkfifo(fifo.c_str(), 0600));
  std::string error;
  EXPECT_FALSE(open_stream(fifo, error));
  ::unlink(fifo.c_str());
  EXPECT_EQ("Not a regular file: " + fifo, error);
}

TEST(SafeFilePosix, DirectoryIsRefused) {
  std::string error;
  EXPECT_FALSE(open_stream("/", error));
  EXPECT_EQ("Not a regular file: /", error);
}

TEST(SafeFilePosix, LimitIsEnforcedWhileReadingAFileThatReportsNoSize) {
  // A /proc file is a regular file whose size reads as 0 while it has
  // content: only the per-chunk check can stop it.
  struct stat st{};
  ASSERT_EQ(0, ::stat("/proc/self/status", &st));
  ASSERT_EQ(0, st.st_size);
  std::string out, error;
  EXPECT_EQ(read_status::too_large, read_all("/proc/self/status", 16, out, error));
  ASSERT_EQ(read_status::ok, read_all("/proc/self/status", 0, out, error)) << error;
  EXPECT_FALSE(out.empty());
}

TEST(SafeFilePosix, WindowsDeviceNamesAreOrdinaryNamesHere) {
  // `\\.\pipe\x` is a file name like any other on POSIX: it is not
  // refused by name, it simply does not exist.
  EXPECT_FALSE(safe_file::refused_by_name("\\\\.\\pipe\\x"));
  EXPECT_FALSE(safe_file::refused_by_name("con"));
}
#endif

// --- is_win32_device_path: portable, so the rules are tested everywhere ----

TEST(DevicePath, DeviceNamespaceIsADevice) {
  for (const char *p : {"\\\\.\\pipe\\x", "//./pipe/x", "\\\\.\\C:", "\\\\.\\PhysicalDrive0", "\\\\.\\C:\\Windows\\win.ini", "\\??\\C:\\x.log",
                        "\\\\?\\C:", "\\\\?\\GLOBALROOT\\Device\\NamedPipe\\x", "\\\\?\\Volume{1}\\x.log", "\\\\?\\pipe\\x", "\\\\?\\UNC\\host\\pipe\\x"}) {
    EXPECT_TRUE(is_win32_device_path(p)) << p;
  }
}

TEST(DevicePath, PipeAndMailslotSharesOnAnyHostAreDevices) {
  for (const char *p : {"\\\\host\\pipe\\x", "\\\\127.0.0.1\\PIPE\\x", "//localhost/pipe/x", "\\\\host\\mailslot\\x"}) {
    EXPECT_TRUE(is_win32_device_path(p)) << p;
  }
}

TEST(DevicePath, DosDeviceNamesAreDevicesInAnyDirectory) {
  for (const char *p :
       {"CON", "con.txt", "C:\\logs\\nul", "C:\\logs\\COM1.log", "C:\\logs\\lpt9", "C:\\logs\\CONIN$", "C:\\logs\\aux .txt", "C:nul", "\\\\?\\C:\\logs\\con"}) {
    EXPECT_TRUE(is_win32_device_path(p)) << p;
  }
}

TEST(DevicePath, FilesAreNotDevices) {
  for (const char *p : {"C:\\logs\\app.log", "C:/logs/app.log", "\\\\?\\C:\\logs\\app.log", "\\\\server\\share\\app.log", "\\\\?\\UNC\\server\\share\\app.log",
                        "\\\\server\\pipes\\app.log", "C:\\logs\\console.log", "C:\\logs\\com10.log", "C:\\logs\\com.log", "C:\\pipe\\x.log", "/var/log/syslog",
                        "relative.log", "C:\\con\\x.log"}) {
    EXPECT_FALSE(is_win32_device_path(p)) << p;
  }
}

#ifdef _WIN32
namespace {

std::wstring pipe_name(const wchar_t *tag) { return L"\\\\.\\pipe\\nscp-safe-file-test-" + std::wstring(tag) + L"-" + std::to_wstring(GetCurrentProcessId()); }

// Create a pipe server, let `connect` open the client end and keep it open,
// then impersonate that client and return the impersonation level the client
// granted. -1 when the server could not impersonate at all.
template <class Connect>
int impersonation_level_granted(const std::wstring &name, Connect connect) {
  const HANDLE server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
  EXPECT_NE(INVALID_HANDLE_VALUE, server) << GetLastError();
  if (server == INVALID_HANDLE_VALUE) return -2;
  int level = -1;
  {
    // A freshly created instance is listening, so the client connects at once
    // and ConnectNamedPipe reports ERROR_PIPE_CONNECTED instead of waiting.
    auto client = connect(utf8::cvt<std::string>(name));
    const BOOL connected = ConnectNamedPipe(server, nullptr);
    const DWORD err = GetLastError();
    EXPECT_TRUE(connected || err == ERROR_PIPE_CONNECTED) << err;
    if (ImpersonateNamedPipeClient(server)) {
      HANDLE token = nullptr;
      if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        SECURITY_IMPERSONATION_LEVEL lvl;
        DWORD len = 0;
        if (GetTokenInformation(token, TokenImpersonationLevel, &lvl, sizeof(lvl), &len)) level = static_cast<int>(lvl);
        CloseHandle(token);
      }
      RevertToSelf();
    }
  }
  CloseHandle(server);
  return level;
}

struct handle_holder {
  HANDLE h;
  explicit handle_holder(void *handle) : h(handle == nullptr ? INVALID_HANDLE_VALUE : static_cast<HANDLE>(handle)) {}
  handle_holder(handle_holder &&o) noexcept : h(o.h) { o.h = INVALID_HANDLE_VALUE; }
  ~handle_holder() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
};

}  // namespace

// The exposure this reader closes: std::ifstream, which check_logfile used
// before, opens a named pipe with no quality-of-service flags, and the pipe's
// server can then act as the agent's account.
TEST(SafeFileWin, IfstreamGrantsAPipeServerImpersonation) {
  const int level = impersonation_level_granted(pipe_name(L"ifstream"), [](const std::string &path) {
    std::unique_ptr<std::ifstream> f(new std::ifstream(utf8::cvt<std::wstring>(path).c_str(), std::ios::in | std::ios::binary));
    EXPECT_TRUE(f->is_open());
    return f;
  });
  EXPECT_EQ(static_cast<int>(SecurityImpersonation), level);
}

TEST(SafeFileWin, PipeServerOnlyLearnsWhoConnected) {
  const int level = impersonation_level_granted(pipe_name(L"reader"), [](const std::string &path) {
    handle_holder h(safe_file::detail::create_for_read(path));
    EXPECT_NE(INVALID_HANDLE_VALUE, h.h) << GetLastError();
    return h;
  });
  // ImpersonateNamedPipeClient succeeds, but with a token that can only be
  // queried: it cannot open or touch anything as the agent.
  EXPECT_EQ(static_cast<int>(SecurityIdentification), level);
}

TEST(SafeFileWin, NamedPipeIsRefused) {
  const std::wstring name = pipe_name(L"refused");
  const HANDLE server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0, 0, 0, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, server);
  std::string error;
  const std::unique_ptr<std::streambuf> buf = open_stream(utf8::cvt<std::string>(name), error);
  CloseHandle(server);
  EXPECT_FALSE(buf);
  EXPECT_NE(std::string::npos, error.find("names a device or a pipe")) << error;
}

TEST(SafeFileWin, RawVolumeIsRefused) {
  // GetFileType says FILE_TYPE_DISK for a raw volume: only the name check
  // stands between \\.\C: and every byte on the drive.
  std::string error;
  EXPECT_FALSE(open_stream("\\\\.\\C:", error));
  EXPECT_NE(std::string::npos, error.find("names a device or a pipe")) << error;
}

TEST(SafeFileWin, DirectoryIsRefused) {
  std::string error;
  EXPECT_FALSE(open_stream(::testing::TempDir(), error));
  EXPECT_FALSE(error.empty());
}

TEST(SafeFileWin, ConsoleDeviceIsRefused) {
  std::string error;
  EXPECT_FALSE(open_stream("CONIN$", error));
  EXPECT_NE(std::string::npos, error.find("names a device or a pipe")) << error;
}

TEST(SafeFileWin, NamesOutsideTheAnsiCodePageAreOpened) {
  const std::wstring wide = utf8::cvt<std::wstring>(::testing::TempDir()) + L"nscp-日本-ж.log";
  {
    const HANDLE h = CreateFileW(wide.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, h);
    DWORD written = 0;
    WriteFile(h, "one\ntwo\n", 8, &written, nullptr);
    CloseHandle(h);
  }
  std::string error;
  const std::unique_ptr<std::streambuf> buf = open_stream(utf8::cvt<std::string>(wide), error);
  DeleteFileW(wide.c_str());
  ASSERT_TRUE(buf) << error;
  std::istream is(buf.get());
  std::string line;
  ASSERT_TRUE(fr::getline_str(is, line, "\n"));
  EXPECT_EQ("one", line);
}

TEST(SafeFileWin, AFileHeldOpenForWritingIsRead) {
  const std::wstring wide = utf8::cvt<std::wstring>(::testing::TempDir()) + L"nscp-held-open.log";
  const HANDLE writer =
      CreateFileW(wide.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, writer);
  DWORD written = 0;
  WriteFile(writer, "held\n", 5, &written, nullptr);
  std::string error;
  {
    const std::unique_ptr<std::streambuf> buf = open_stream(utf8::cvt<std::string>(wide), error);
    EXPECT_TRUE(buf) << error;
    if (buf) {
      std::istream is(buf.get());
      std::string line;
      EXPECT_TRUE(fr::getline_str(is, line, "\n"));
      EXPECT_EQ("held", line);
    }
  }
  CloseHandle(writer);
  DeleteFileW(wide.c_str());
}
#endif
