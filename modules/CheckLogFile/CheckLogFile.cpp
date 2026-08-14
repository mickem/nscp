// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckLogFile.h"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <cstdint>
#include <deque>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <utility>

#include "bookmark_state.hpp"
#include "file_reader.hpp"
#include "realtime_thread.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

// Context used to persist bookmark positions in the core storage.
const std::string bookmark_context = "logfile.bookmarks";

bool CheckLogFile::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  thread_.reset(new real_time_thread(get_core(), get_id()));

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias(alias, "logfile");

  thread_->filters_.set_path(settings.alias().get_settings_path("real-time/checks"));

  // clang-format off
  settings.alias().add_path_to_settings()
    ("real-time", "Real-time filtering", "A set of options to configure the real time checks")
    ("real-time/checks", sh::fun_values_path([this] (auto key, auto value) { thread_->add_realtime_filter(nscapi::settings_proxy::create(get_id(), get_core()), key, value); }),
	    "Real-time filters", "A set of filters to use in real-time mode",
	    "REALTIME FILTER DEFENTION", "For more configuration options add a dedicated section"
	    )
    ;
  // clang-format on

  settings.alias()
      .add_key_to_settings("real-time")

      .add_bool("enabled", sh::bool_fun_key([this](auto value) { thread_->set_enabled(value); }, false), "Real time",
                "Spawns a background thread which waits for file changes.");

  settings.register_all();
  settings.notify();

  thread_->ensure_default(nscapi::settings_proxy::create(get_id(), get_core()));

  thread_->filters_.add_samples(settings.get_settings());

  // Restore the bookmark positions from the previous run so a restart does not
  // re-report every line of every bookmarked log file.
  nscapi::core_helper core(get_core(), get_id());
  for (const nscapi::core_helper::storage_map::value_type &e : core.get_storage_strings(bookmark_context)) {
    bookmarks_.add(e.first, e.second);
  }

  if (mode == NSCAPI::normalStart) {
    if (!thread_->start()) NSC_LOG_ERROR_STD("Failed to start collection thread");
  }
  return true;
}
bool CheckLogFile::unloadModule() {
  if (thread_ && !thread_->stop()) NSC_LOG_ERROR_STD("Failed to stop thread");

  nscapi::core_helper core(get_core(), get_id());
  for (const check_logfile::bookmarks::map_type::value_type &v : bookmarks_.get_copy()) {
    core.put_storage(bookmark_context, v.first, v.second, false, false);
  }
  return true;
}

namespace {
// Feed the records found in `contents` to the filter.
//
// Returns the number of bytes consumed by COMPLETE records, i.e. the offset
// just past the last line delimiter. When `include_tail` is set a trailing
// chunk which is not terminated by the delimiter is matched as well (but never
// counted as consumed, since it is not complete).
//
// With `max_lines` set only that many records are handed to the filter: the
// first ones when `newest_first` (the newest record is at the top of the
// file), the last ones otherwise. The records which are dropped are still
// counted as consumed - a bookmark has to advance past everything that was
// read, or the surplus of one check would come back as "new" on the next one.
// The selected records are always matched in file order, so %(list) reads the
// way the file does.
std::string::size_type match_records(const std::string &filename, const std::string &contents, const std::string &line_split, const std::string &column_split,
                                     bool strip_cr, bool include_tail, std::size_t max_lines, bool newest_first, logfile_filter::filter &filter) {
  const auto trim_cr = [strip_cr](std::string &s) {
    if (strip_cr && !s.empty() && s.back() == '\r') s.pop_back();
  };
  const auto match_one = [&](std::string line) {
    trim_cr(line);
    std::list<std::string> chunks = str::utils::split_lst(line, column_split);
    std::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(filename, line, chunks));
    filter.match(record);
  };

  // Unlimited: match each record as the buffer is walked, nothing is buffered.
  // Limited: remember the wanted ranges (never more than max_lines of them)
  // and match them once the extent of the file is known.
  typedef std::pair<std::string::size_type, std::string::size_type> range_type;
  std::deque<range_type> kept;
  const auto take = [&](std::string::size_type start, std::string::size_type len) {
    if (max_lines == 0) {
      match_one(contents.substr(start, len));
      return;
    }
    if (newest_first) {
      if (kept.size() < max_lines) kept.push_back(range_type(start, len));
      return;
    }
    kept.push_back(range_type(start, len));
    if (kept.size() > max_lines) kept.pop_front();
  };

  std::string::size_type pos = 0, lpos = 0;
  while ((pos = contents.find(line_split, pos)) != std::string::npos) {
    take(lpos, pos - lpos);
    pos += line_split.size();
    lpos = pos;
  }
  if (include_tail && lpos < contents.size()) {
    take(lpos, contents.size() - lpos);
  }
  for (const range_type &r : kept) {
    match_one(contents.substr(r.first, r.second));
  }
  return lpos;
}

// Read (at most) the leading bytes used to fingerprint a file. The stream is
// left positioned wherever the read ended; every caller seeks afterwards.
std::string read_head(std::istream &is, std::size_t len) {
  std::string head;
  if (len == 0) return head;
  head.resize(len);
  is.seekg(0, std::ios::beg);
  is.read(&head[0], static_cast<std::streamsize>(len));
  head.resize(static_cast<std::size_t>(is.gcount()));
  is.clear();
  return head;
}
}  // namespace

void CheckLogFile::check_logfile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef logfile_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string regexp, line_split, column_split;
  std::vector<std::string> file_list;
  std::string files_string;
  std::string mode;
  std::string bookmark;
  std::size_t max_lines = 0;
  std::string newest;

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax());
  filter_helper.add_syntax("${count}/${total} (${problem_list})", "${column1}", "${column1}", "%(status): Nothing found", "");
  // clang-format off
	filter_helper.get_desc().add_options()
		//		("regexp", po::value<std::string>(&regexp),					"Lookup a numeric value in the PDH index table")
		("line-split", po::value<std::string>(&line_split)->default_value("\\n"),
		"Character string used to split a file into several lines (default `\\n`).\n"
                        "The escape sequences `\\n` and `\\t` are translated to LF and TAB respectively; "
                        "all other characters are taken literally. Multi-character delimiters are "
                        "supported (for example `\\r\\n` to split strictly on CRLF, or `|||` for a  "
                        "custom separator). Setting `line-split` to an empty value (`line-split=""`) "
                        "makes the entire file content available as a single record, which is useful "
                        "together with a multi-line regular-expression filter.\\n"
                        "When the chosen delimiter ends with `\n`, a trailing carriage return is "
                        "stripped from each record so that files with CRLF line endings produce "
                        "clean lines.")
		("column-split", po::value<std::string>(&column_split)->default_value("\\t"),
			"Character string to split a line into several columns (default \\t)")
		("split", po::value<std::string>(&column_split), "Alias for split-column")
		("file", po::value<std::vector<std::string> >(&file_list), "File to read (can be specified multiple times to check multiple files.\n"
			"Notice that specifying multiple files will create an aggregate set it will not check each file individually.\n"
			"In other words if one file contains an error the entire check will result in error or if you check the count it is the global count which is used.")
		("files", po::value<std::string>(&files_string), "A comma separated list of files to scan (same as file except a list)")
		("bookmark", po::value<std::string>(&bookmark)->implicit_value("auto"),
			"Only scan lines added since the last check with the same bookmark name.\n"
			"NSClient++ remembers, per file and per bookmark, how far it read last time and resumes from there, "
			"so a line is reported once instead of on every check. The first check of a file reads it in full; "
			"a file which is truncated, rotated or replaced is detected (via its size and a fingerprint of its "
			"first bytes) and read from the beginning again. A trailing line which is not yet terminated by "
			"line-split is held back until it is complete, so half-written lines are never reported twice.\n"
			"If you set this to auto (or leave the value empty) the bookmark name is derived from the file name "
			"together with your filter, warning and critical expressions, which keeps unrelated checks of the "
			"same file from consuming each other's lines. Use an explicit name to share (or separate) positions "
			"deliberately. Positions are persisted across restarts.")
		("max-lines", po::value<std::size_t>(&max_lines)->default_value(0),
			"Only examine the newest <N> lines of each file (0, the default, means every line).\n"
			"The limit is applied per file, after any bookmark: with a bookmark the check still only sees lines "
			"added since the last check, and this caps how many of them are reported when a burst of lines was "
			"written at once. The lines dropped by the limit are never reported later either - the bookmark moves "
			"past everything which was read.\n"
			"Which end of the file holds the newest lines is controlled by `newest`.")
		("newest", po::value<std::string>(&newest)->default_value("last"),
			"Which end of the file holds the newest line: `last` (the default: lines are appended, as with most "
			"machine-written logs) or `first` (the file is rewritten with the newest line at the top, which is "
			"common in hand-maintained files such as changelogs).\n"
			"This only decides which end `max-lines` counts from; lines are always reported in the order they "
			"appear in the file. `newest=first` cannot be combined with `bookmark`, since a file which is "
			"rewritten from the top has no stable position to resume from.")
		//		("mode", po::value<std::string>(&mode),						"Mode of operation: count (count all critical/warning lines), find (find first critical/warning line)")
		;
  // clang-format on

  if (!files_string.empty()) boost::split(file_list, files_string, boost::is_any_of(","));

  if (!filter_helper.parse_options()) return;

  if (column_split.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No column-split specified");
  if (line_split.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No line-split specified");

  if (file_list.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "Need to specify at least one file: file=foo.txt");

  const bool newest_first = newest == "first";
  if (!newest_first && newest != "last") {
    return nscapi::protobuf::functions::set_response_bad(*response, "Invalid newest: " + newest + " (expected first or last)");
  }
  // A file which is rewritten with the newest line first has no stable byte
  // offset to resume from: its fingerprint changes on every write, so the
  // bookmark would restart from the beginning every check and report the whole
  // file each time. Say so instead of quietly doing that.
  if (newest_first && !bookmark.empty()) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "newest=first cannot be combined with bookmark: a file which is rewritten from the top has no position to resume from");
  }

  str::utils::replace(column_split, "\\t", "\t");
  str::utils::replace(column_split, "\\n", "\n");
  str::utils::replace(line_split, "\\t", "\t");
  str::utils::replace(line_split, "\\n", "\n");

  if (!filter_helper.build_filter(filter)) return;

  // Mirror the trailing-CR strip that file_reader::getline_str applies in the
  // realtime path: when splitting on a delimiter that ends in '\n', a CRLF
  // file read in binary mode would otherwise leave '\r' at the end of every
  // line and break exact-match column comparisons (e.g. column3 = 'Test 1').
  const bool strip_cr = !line_split.empty() && line_split.back() == '\n';

  // An "auto" bookmark is derived from the file plus the expressions which
  // decide what is interesting, so two different checks over the same file do
  // not steal each other's lines (the same rule CheckEventLog uses).
  std::string auto_suffix;
  if (bookmark == "auto") {
    auto_suffix = "],filter[" + str::utils::joinEx(filter_helper.data.filter_string, ",") + "],warn[" +
                  str::utils::joinEx(filter_helper.data.warn_string, ",") + "],crit[" + str::utils::joinEx(filter_helper.data.crit_string, ",") + "]";
  }

  for (const std::string &filename : file_list) {
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to open file: " + filename);
    }
    if (bookmark.empty()) {
      // With a line limit only the part of the file which can hold the wanted
      // records is read: from the last max_lines records to the end, or from
      // the start up to the max_lines-th record. Both fall back to reading
      // everything when the limit cannot be located cheaply; match_records
      // drops the surplus either way.
      std::string contents;
      if (max_lines > 0 && newest_first) {
        contents = check_logfile::file_reader::read_leading_records(file, line_split, max_lines);
      } else {
        if (max_lines > 0) {
          const std::uint64_t offset = check_logfile::file_reader::find_tail_offset(file, line_split, max_lines);
          file.clear();
          file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        }
        contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      }
      match_records(filename, contents, line_split, column_split, strip_cr, true, max_lines, newest_first, filter);
      continue;
    }

    const std::string name = auto_suffix.empty() ? bookmark : ("auto,file[" + filename + auto_suffix);
    const std::string key = check_logfile::bookmarks::make_key(name, filename);
    const check_logfile::bookmark::position prev = bookmarks_.get(key);

    file.seekg(0, std::ios::end);
    const std::streamoff end_pos = file.tellg();
    if (end_pos < 0) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Failed to read size of file: " + filename);
    }
    const std::uint64_t size = static_cast<std::uint64_t>(end_pos);

    // Hash exactly as many leading bytes as the stored fingerprint covers so
    // the two are comparable, and (up to the cap) as many as we have now so the
    // fingerprint keeps growing with a file which is still shorter than the cap.
    const std::size_t head_len =
        static_cast<std::size_t>(std::min<std::uint64_t>(std::max<std::uint64_t>(size, prev.fingerprint_len), check_logfile::bookmark::max_fingerprint_len));
    const std::string head = read_head(file, head_len);
    const bool head_valid = head.size() >= prev.fingerprint_len;
    const std::uint64_t head_fingerprint = head_valid ? check_logfile::bookmark::fnv1a(head.data(), static_cast<std::size_t>(prev.fingerprint_len)) : 0;

    const check_logfile::bookmark::resume_decision decision = check_logfile::bookmark::compute_resume(prev, size, head_fingerprint, head_valid);
    if (decision.restarted) {
      NSC_DEBUG_MSG_STD("Log file was truncated, rotated or replaced, reading it from the start: " + filename);
    }

    std::string::size_type consumed = 0;
    if (!decision.skip) {
      file.seekg(static_cast<std::streamoff>(decision.offset), std::ios::beg);
      const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      // A trailing record with no delimiter is not complete yet: skipping it
      // (and not counting it as consumed) is what makes the line show up once,
      // in full, on the check which sees its terminator.
      consumed = match_records(filename, contents, line_split, column_split, strip_cr, false, max_lines, newest_first, filter);
    }

    const check_logfile::bookmark::position next(decision.offset + consumed, head.size(), check_logfile::bookmark::fnv1a(head.data(), head.size()));
    bookmarks_.add(key, check_logfile::bookmark::format(next));
  }
  filter_helper.post_process(filter);
}