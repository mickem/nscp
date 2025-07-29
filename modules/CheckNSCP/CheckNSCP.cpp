/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "CheckNSCP.h"

#include <config.h>

#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <file_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_settings_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/helpers.hpp>
#include <str/format.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckNSCP::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  start_ = boost::posix_time::microsec_clock::local_time();

  std::string path;
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  crashFolder = get_core()->expand_path(CRASH_ARCHIVE_FOLDER);
  NSC_DEBUG_MSG_STD("Crash folder is: " + crashFolder.string());
  return true;
}

bool CheckNSCP::unloadModule() { return true; }
std::string render(int, const std::string, int, std::string message) { return message; }
void CheckNSCP::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  if (message.level() != PB::Log::LogEntry_Entry_Level_LOG_CRITICAL && message.level() != PB::Log::LogEntry_Entry_Level_LOG_ERROR) return;
  {
    boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) return;
    error_count_++;
    last_error_ = message.message();
  }
}

int get_crashes(boost::filesystem::path root, std::string &last_crash) {
  if (!boost::filesystem::is_directory(root)) {
    return 0;
  }
  int count = 0;

  time_t last_write = std::time(0);
  boost::filesystem::directory_iterator begin(root), end;
  for (const boost::filesystem::path &p : boost::make_iterator_range(begin, end)) {
    if (boost::filesystem::is_regular_file(p) && file_helpers::meta::get_extension(p) == "txt") count++;
    time_t lw = boost::filesystem::last_write_time(p);
    if (lw > last_write) {
      last_write = lw;
      last_crash = file_helpers::meta::get_filename(p);
    }
  }
  return count;
}

std::size_t CheckNSCP::get_errors(std::string &last_error) {
  boost::unique_lock<boost::timed_mutex> lock(mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
  if (!lock.owns_lock()) {
    last_error = "Failed to get lock";
    return error_count_ + 1;
  }
  last_error = last_error_;
  return error_count_;
}

struct nscp_version {
  int release;
  int major_version;
  int minor_version;
  int build;
  bool has_build;
  std::string date;

  nscp_version() : release(0), major_version(0), minor_version(0), build(0), has_build(false) {}
  nscp_version(const nscp_version &other)
      : release(other.release),
        major_version(other.major_version),
        minor_version(other.minor_version),
        build(other.build),
        date(other.date),
        has_build(other.has_build) {}
  nscp_version &operator=(const nscp_version &other) {
    release = other.release;
    major_version = other.major_version;
    minor_version = other.minor_version;
    build = other.build;
    date = other.date;
    has_build = other.has_build;
    return *this;
  }
  nscp_version(std::string v) : release(0), major_version(0), minor_version(0), build(0), has_build(false) {
    str::utils::token v2 = str::utils::split2(v, " ");
    date = v2.second;
    std::list<std::string> vl = str::utils::split_lst(v2.first, ".");
    if (vl.empty() || vl.size() > 4) {
      throw nsclient::nsclient_exception("Failed to parse version: " + v);
    }
    if (!vl.empty()) {
      release = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      major_version = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      minor_version = str::stox<int>(vl.front());
      vl.pop_front();
    }
    if (!vl.empty()) {
      build = str::stox<int>(vl.front());
      has_build = true;
    }
  }
  std::string to_string() const {
    if (has_build) {
      return str::xtos(release) + "." + str::xtos(major_version) + "." + str::xtos(minor_version) + "." + str::xtos(build);
    }
    return str::xtos(release) + "." + str::xtos(major_version) + "." + str::xtos(minor_version);
  }
};

namespace check_nscp_version {
struct filter_obj {
  nscp_version version;

  filter_obj() {}
  filter_obj(nscp_version version) : version(version) {}

  long long get_major(parsers::where::evaluation_context context) const { return version.major_version; }
  long long get_minor(parsers::where::evaluation_context context) const { return version.minor_version; }
  long long get_build(parsers::where::evaluation_context context) const { return version.build; }
  long long get_release(parsers::where::evaluation_context context) const { return version.release; }
  std::string get_version_s(parsers::where::evaluation_context context) const { return version.to_string(); }
  std::string get_date_s(parsers::where::evaluation_context context) const { return version.date; }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  // clang-format off
		filter_obj_handler() {
			registry_.add_string()
				("version", &filter_obj::get_version_s, "The NSClient++ Version as a string")
				("date", &filter_obj::get_date_s, "The NSClient++ Build date")
				;
			registry_.add_int()
				("release", &filter_obj::get_release, "The release (the 0 in 0.1.2.3)")
				("major", &filter_obj::get_major, "The major (the 1 in 0.1.2.3)")
				("minor", &filter_obj::get_minor, "The minor (the 2 in 0.1.2.3)")
				("build", &filter_obj::get_build, "The build (the 3 in 0.1.2.3) not available in release versions after 0.6.0")
				;
    // clang-format on
  }
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

void check(const nscp_version &version, const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${version} (${date})", "version", "", "");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  boost::shared_ptr<filter_obj> record(new filter_obj(version));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace check_nscp_version

void CheckNSCP::check_nscp_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  nscp_version version;
  try {
    version = nscp_version(get_core()->getApplicationVersionString());
  } catch (const nsclient::nsclient_exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse version: " + e.reason());
    return;
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse version: " + utf8::utf8_from_native(e.what()));
    return;
  }
  check_nscp_version::check(version, request, response);
}

void CheckNSCP::check_nscp(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  po::options_description desc = nscapi::program_options::create_desc(request);
  po::variables_map vm;
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;
  response->set_result(PB::Common::ResultCode::OK);
  std::string last, message;
  int crash_count = get_crashes(crashFolder, last);
  str::format::append_list(message, str::xtos(crash_count) + " crash(es)", std::string(", "));
  if (crash_count > 0) {
    response->set_result(PB::Common::ResultCode::CRITICAL);
    str::format::append_list(message, std::string("last crash: " + last), std::string(", "));
  }

  int err_count = get_errors(last);
  str::format::append_list(message, str::xtos(err_count) + " error(s)", std::string(", "));
  if (err_count > 0) {
    response->set_result(PB::Common::ResultCode::CRITICAL);
    str::format::append_list(message, std::string("last error: " + last), std::string(", "));
  }
  boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();
  ;
  boost::posix_time::time_duration td = end - start_;

  std::stringstream uptime;
  uptime << "uptime " << td;
  str::format::append_list(message, uptime.str(), std::string(", "));
  response->add_lines()->set_message(message);
}