// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_disk_write.hpp"

#include <boost/algorithm/string/join.hpp>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <vector>

#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace po = boost::program_options;

namespace check_disk_write_command {

struct filter_obj {
  write_result result;

  explicit filter_obj(const write_result &result) : result(result) {}

  std::string get_path() const { return result.path; }
  std::string get_issues() const { return result.issues; }
  std::string get_message() const {
    if (!result.issues.empty()) return result.issues;
    return "wrote and read back " + str::xtos(result.size) + " bytes in " + str::xtos(result.total_time) + "ms";
  }
  long long get_size() const { return result.size; }
  long long get_write_time() const { return result.write_time; }
  long long get_read_time() const { return result.read_time; }
  long long get_total_time() const { return result.total_time; }
  long long get_has_issues() const { return result.issues.empty() ? 0 : 1; }
  std::string show() const { return "write test " + result.path + ": " + get_message(); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("path", &filter_obj::get_path, "Path of the test file")
        .add_string_var("issues", &filter_obj::get_issues, "Human-readable description of any problems found (empty when the write test succeeded)")
        .add_string_var("message", &filter_obj::get_message, "Human readable outcome of the write test");
    // Each keyword gets its own perf suffix so thresholds on several of them
    // produce distinct perf-data labels ("<file> write_time", "<file> size", ...).
    registry_.add_int_var("size", &filter_obj::get_size, "Number of bytes written to (and read back from) the test file")
        .add_int_perf("B", "", " size")
        .add_int_var("write_time", &filter_obj::get_write_time, "Time spent creating, writing and flushing the file to disk (ms)")
        .add_int_perf("ms", "", " write_time")
        .add_int_var("read_time", &filter_obj::get_read_time, "Time spent reading back and verifying the file (ms)")
        .add_int_perf("ms", "", " read_time")
        .add_int_var("total_time", &filter_obj::get_total_time, "Total time for the create/write/read/delete cycle (ms)")
        .add_int_perf("ms", "", " total_time");
    registry_.add_int_var("has_issues", &filter_obj::get_has_issues, "1 when the write test failed, else 0").no_perf();
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

namespace {
// A check should be a quick probe, not a benchmark: cap how much data one
// invocation may write so a typo (size=1G) cannot tie up a check thread.
constexpr long long max_test_size = 1024 * 1024;

long long elapsed_ms(const std::chrono::steady_clock::time_point &from) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - from).count();
}

std::string last_error() {
  const char *msg = strerror(errno);
  return msg ? msg : "unknown error";
}

// Race-free exclusive create: a leftover (or unrelated) file at the target
// path must fail the open (EEXIST) instead of being overwritten, with no
// window between an existence check and the create. POSIX uses fopen's C11
// "x" modifier; on Windows the "x" is UCRT-only — the CRT linked by the
// legacy XP-targeting build treats it as an invalid mode character and
// fail-fasts the whole process (0xc0000409) — so use _sopen_s with _O_EXCL,
// which every supported CRT provides.
FILE *open_exclusive(const std::string &path) {
#ifdef WIN32
  int fd = -1;
  if (_sopen_s(&fd, path.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0) return nullptr;
  FILE *file = _fdopen(fd, "wb");
  if (file == nullptr) _close(fd);
  return file;
#else
  return fopen(path.c_str(), "wbx");
#endif
}
}  // namespace

write_result perform_write_test(const std::string &path, const long long size) {
  write_result result;
  result.path = path;
  result.size = size;
  std::vector<std::string> issues;

  // A recognizable, deterministic pattern: easy to identify if a test file is
  // ever left behind, and verifiable on read back.
  static const std::string pattern = "NSClient++ disk write test data. ";
  std::vector<char> chunk(64 * 1024);
  for (std::size_t i = 0; i < chunk.size(); ++i) chunk[i] = pattern[i % pattern.size()];

  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  bool created = false;

  // Never touch a file we did not create: the exclusive create reports a
  // leftover (or unrelated) file at the target path instead of overwriting
  // and deleting it.
  FILE *file = open_exclusive(path);
  if (file == nullptr) {
    if (errno == EEXIST) {
      issues.push_back("file already exists (refusing to overwrite it)");
    } else {
      issues.push_back("failed to create file: " + last_error());
    }
  } else {
    created = true;
    for (long long remaining = size; remaining > 0 && issues.empty();) {
      const std::size_t count = remaining < static_cast<long long>(chunk.size()) ? static_cast<std::size_t>(remaining) : chunk.size();
      if (fwrite(chunk.data(), 1, count, file) != count) {
        issues.push_back("failed to write to file: " + last_error());
        break;
      }
      remaining -= count;
    }
    if (issues.empty() && fflush(file) != 0) {
      issues.push_back("failed to flush file: " + last_error());
    }
    // Push the data through the OS cache to the device so "writable" means
    // the disk actually accepted the data, not just the page cache.
#ifdef WIN32
    if (issues.empty() && _commit(_fileno(file)) != 0) {
      issues.push_back("failed to sync file to disk: " + last_error());
    }
#else
    if (issues.empty() && fsync(fileno(file)) != 0) {
      issues.push_back("failed to sync file to disk: " + last_error());
    }
#endif
    fclose(file);
  }
  result.write_time = elapsed_ms(start);

  if (created && issues.empty()) {
    const std::chrono::steady_clock::time_point read_start = std::chrono::steady_clock::now();
    file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
      issues.push_back("failed to read file back: " + last_error());
    } else {
      // Mirror the write loop chunk for chunk so the verify buffer lines up.
      std::vector<char> read_buffer(chunk.size());
      for (long long remaining = size; remaining > 0 && issues.empty();) {
        const std::size_t count = remaining < static_cast<long long>(chunk.size()) ? static_cast<std::size_t>(remaining) : chunk.size();
        if (fread(read_buffer.data(), 1, count, file) != count) {
          // A short read is not necessarily a short file: a mid-read I/O error
          // also returns a short count, and flaky disks are what we are here
          // to catch.
          if (ferror(file)) {
            issues.push_back("failed to read file back: " + last_error());
          } else {
            issues.push_back("failed to read file back: file is shorter than what was written");
          }
          break;
        }
        if (memcmp(read_buffer.data(), chunk.data(), count) != 0) {
          issues.push_back("read back data does not match what was written");
          break;
        }
        remaining -= count;
      }
      if (issues.empty()) {
        if (fread(read_buffer.data(), 1, 1, file) != 0) {
          issues.push_back("failed to read file back: file is larger than what was written");
        } else if (ferror(file)) {
          issues.push_back("failed to read file back: " + last_error());
        }
      }
      fclose(file);
    }
    result.read_time = elapsed_ms(read_start);
  }

  if (created && std::remove(path.c_str()) != 0) {
    issues.push_back("failed to delete file: " + last_error());
  }
  result.total_time = elapsed_ms(start);
  result.issues = boost::algorithm::join(issues, ", ");
  return result;
}

void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const write_tester &tester) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string file_path;
  std::string size_arg;

  filter_type filter;
  filter_helper.add_options("", "has_issues = 1", "", filter.get_filter_syntax(), "unknown");
  // The top-syntax embeds `%(list)` so the single record renders in per-item
  // context for every status (see check_single_file for the details).
  filter_helper.add_syntax("${status}: ${list}", "%(path): %(message)", "%(path)", "No write test performed", "%(status): %(list)");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("file", po::value<std::string>(&file_path), "The test file to create (must not already exist; it is deleted after the test).")
    ("path", po::value<std::string>(&file_path), "Alias for file.")
    ("size", po::value<std::string>(&size_arg)->default_value("1k"), "The amount of data to write, in bytes or with a byte unit (e.g. 512, 4k, 1M). Maximum is 1M.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (file_path.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No file specified (use file=<path>)");
  }
  long long size = 0;
  try {
    size = str::format::decode_byte_units(size_arg);
  } catch (const std::exception &) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid size: " + size_arg);
  }
  if (size < 0) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid size: " + size_arg);
  }
  if (size > max_test_size) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Size too large: " + size_arg + " (maximum is 1M)");
  }

  if (!filter_helper.build_filter(filter)) return;

  const std::shared_ptr<filter_obj> record(new filter_obj(tester(file_path, size)));
  filter.match(record);
  filter_helper.post_process(filter);
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_with(request, response, &perform_write_test);
}

}  // namespace check_disk_write_command
