// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_largest_files.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <memory>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

namespace po = boost::program_options;

namespace check_largest_files_command {

namespace {

struct filter_obj {
  std::string path;
  long long size = 0;
  long long files = 0;
  long long rank = 0;

  std::string get_path() const { return path; }
  std::string get_name() const {
    const std::string::size_type pos = path.find_last_of("/\\");
    if (pos == std::string::npos || pos + 1 == path.size()) return path;
    return path.substr(pos + 1);
  }
  long long get_size() const { return size; }
  long long get_files() const { return files; }
  long long get_rank() const { return rank; }
  std::string show() const { return path; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj>> native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("path", &filter_obj::get_path, "Full path of the file or folder")
        .add_string_var("name", &filter_obj::get_name, "Name of the file or folder (the last path element)");
    // clang-format off
    registry_.add_int_legacy()
      .add_int("size", parsers::where::type_size, [](auto obj) { return obj->get_size(); },
               "Size in bytes: the file's size, or a folder's recursive total").add_scaled_byte(std::string(""), std::string(""))
      .add_int("file_count", [](auto obj) { return obj->get_files(); },
               "Number of files counted in a folder's total (1 for a file)").add_perf("", "", " files")
      .add_int("rank", [](auto obj) { return obj->get_rank(); }, "Position in the list, 1 being the largest").no_perf();
    // clang-format on
    registry_.add_human_string_context(
        "size", [](auto obj, auto context) { return str::format::format_byte_units(obj->get_size(), context->get_number_format()); }, "");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter_type;

}  // namespace

void check_with(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                const check::access::path_policy &access, const scanner &scan) {
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  std::string path, type = "files", min_size, method = "auto", timeout_state = "unknown";
  largest_files::scan_request scan_request;
  int top = 10;
  int folder_depth = 1;
  int timeout = 30;

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ok");
  filter_helper.add_syntax("${status}: ${list}", "${path} (${size})", "${path}", "%(status): No files found", "");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("path", po::value<std::string>(&path), "The folder to scan, for instance C:\\ or /var.")
    ("type", po::value<std::string>(&type)->default_value("files"),
        "What to rank: files (the largest files anywhere beneath path) or folders (the folders beneath path with the largest recursive total).")
    ("top", po::value<int>(&top)->default_value(10), "How many files (or folders) to return, largest first.")
    ("min-size", po::value<std::string>(&min_size),
        "Ignore anything smaller than this, for instance 100M or 1G (K, M, G and T are powers of 1024). In folder mode it applies to the folder totals.")
    ("pattern", po::value<std::string>(&scan_request.pattern),
        "Only count files whose name matches this mask, for instance *.log (* and ? are wildcards; case insensitive on Windows). In folder mode it "
        "selects the files that count towards the totals.")
    ("folder-depth", po::value<int>(&folder_depth)->default_value(1),
        "Folder mode: which folders are ranked. 1 ranks the direct children of path, N ranks every folder down to N levels below it and -1 every "
        "folder at any depth. Totals are always recursive.")
    ("timeout", po::value<int>(&timeout)->default_value(30),
        "Abort the scan after this many seconds and return timeout-state (UNKNOWN by default), with what was found so far in the message. 0 means "
        "no limit.")
    ("timeout-state", po::value<std::string>(&timeout_state)->default_value("unknown"),
        "The status to return when the scan timed out: ok, warning, critical or unknown. It replaces whatever the partial result would "
        "have been, since a scan that did not finish cannot say what the largest entries are.")
    ("method", po::value<std::string>(&method)->default_value("auto"),
        "How to scan. auto reads the NTFS Master File Table when it can (Windows, a local NTFS volume and administrator rights) and walks the "
        "folder tree otherwise; mft insists on the Master File Table and fails without it; walk always walks the tree.")
    ("same-filesystem", po::value<bool>(&scan_request.same_filesystem)->implicit_value(true)->default_value(true),
        "Do not descend into other filesystems mounted beneath path (Linux). On Windows a mounted volume is a reparse point and is never entered.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (path.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No path specified (use path=<folder>)");

  if (type == "files" || type == "file") {
    scan_request.folders = false;
  } else if (type == "folders" || type == "folder" || type == "dirs" || type == "directories") {
    scan_request.folders = true;
  } else {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid type: " + type + " (use files or folders)");
  }
  if (top < 1) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid top: it must be at least 1");
  if (folder_depth == 0 || folder_depth < -1)
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid folder-depth: use 1 or more, or -1 for any depth");
  if (timeout < 0) return nscapi::protobuf::functions::set_response_bad(*response, "Invalid timeout: it must be 0 or more seconds");

  if (method == "auto") {
    scan_request.method = largest_files::scan_method::automatic;
  } else if (method == "mft") {
    scan_request.method = largest_files::scan_method::mft;
  } else if (method == "walk") {
    scan_request.method = largest_files::scan_method::walk;
  } else {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid method: " + method + " (use auto, mft or walk)");
  }

  if (timeout_state != "ok" && timeout_state != "warning" && timeout_state != "critical" && timeout_state != "unknown")
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid timeout-state: " + timeout_state + " (use ok, warning, critical or unknown)");

  if (!min_size.empty()) {
    // decode_byte_units reads the leading digits and the first letter after
    // them, so 1.5G would quietly become one byte; accept only what it reads
    // whole.
    static const boost::regex size_syntax("[0-9]+([KkMmGgTt][Bb]?|[Bb])?");
    long long value = -1;
    if (boost::regex_match(min_size, size_syntax)) {
      try {
        value = str::format::decode_byte_units(min_size);
      } catch (const std::exception &) {
        value = -1;
      }
    }
    if (value < 0)
      return nscapi::protobuf::functions::set_response_bad(*response,
                                                           "Invalid min-size: " + min_size + " (use a whole number, optionally followed by K, M, G or T)");
    scan_request.min_size = static_cast<unsigned long long>(value);
  }
  if (scan_request.pattern == "*" || scan_request.pattern == "*.*") scan_request.pattern.clear();

  // Hold the root against [/settings/disk] 'file access'. Only the root needs
  // it: neither scan follows a link, so everything reported is genuinely
  // beneath a root which passed.
  {
    const check::access::decision decision = access.resolve(path);
    if (!decision.allowed) return nscapi::protobuf::functions::set_response_bad(*response, decision.error);
    path = decision.value;
  }

  if (!filter_helper.build_filter(filter)) return;

  scan_request.root = path;
  scan_request.top = static_cast<std::size_t>(top);
  scan_request.folder_depth = folder_depth;
  const auto started = std::chrono::steady_clock::now();
  if (timeout > 0) scan_request.deadline = started + std::chrono::seconds(timeout);

  const largest_files::scan_result result = scan(scan_request);
  if (!result.error.empty()) return nscapi::protobuf::functions::set_response_bad(*response, result.error);

  const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
  NSC_DEBUG_MSG("check_largest_files: scanned " + std::to_string(result.files_seen) + " files in " + std::to_string(result.folders_seen) + " folders under " +
                path + " in " + std::to_string(elapsed) + "ms (" + result.method + "), skipped " + std::to_string(result.links_skipped) + " links, " +
                std::to_string(result.unreadable) + " unreadable" + (result.timed_out ? ", timed out" : ""));

  long long rank = 0;
  for (const largest_files::entry &e : result.entries) {
    auto record = std::make_shared<filter_obj>();
    record->path = e.path;
    record->size = static_cast<long long>(e.size);
    record->files = static_cast<long long>(e.files);
    record->rank = ++rank;
    filter.match(record);
  }
  filter_helper.post_process(filter);

  if (result.timed_out) {
    std::string note = " (scan timed out after " + std::to_string(timeout) + "s";
    if (!result.fallback_reason.empty()) note += " walking the tree, as the MFT could not be used: " + result.fallback_reason;
    note += "; the result is partial)";
    const int state = nscapi::plugin_helper::translateReturn(timeout_state);
    if (response->lines_size() > 0) {
      PB::Commands::QueryResponseMessage::Response::Line *line = response->mutable_lines(0);
      std::string message = line->message();
      // The status word was rendered from the partial result; the default
      // syntax leads with it, so it is swapped for the state actually returned.
      const std::string rendered =
          nscapi::plugin_helper::translateReturn(nscapi::protobuf::functions::gbp_to_nagios_status(response->result())) + ":";
      if (message.compare(0, rendered.size(), rendered) == 0)
        message = nscapi::plugin_helper::translateReturn(state) + ":" + message.substr(rendered.size());
      line->set_message(message + note);
    }
    response->set_result(nscapi::protobuf::functions::nagios_status_to_gpb(state));
  }
}

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
           const check::access::path_policy &access) {
  check_with(request, response, access, &largest_files::scan);
}

}  // namespace check_largest_files_command
