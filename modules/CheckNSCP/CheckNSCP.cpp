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
#include <net/http/client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_exec.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

#include "check_nscp_helpers.hpp"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

using check_nscp_helpers::nscp_version;

bool CheckNSCP::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  start_ = boost::posix_time::microsec_clock::local_time();

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("nscp", alias, "check");
  crashFolder = get_core()->expand_path(CRASH_ARCHIVE_FOLDER);
  NSC_DEBUG_MSG_STD("Crash folder is: " + crashFolder.string());

  // Default the CA bundle to the trusted system store (${ca-path} expands to
  // certificate-path/windows-ca.pem on Windows, /etc/ssl/certs/ca-certificates.crt
  // on Linux). The same setting is used by CheckNet's check_http and lets the
  // update check validate api.github.com out of the box.
  const std::string default_ca = get_core()->expand_path("${ca-path}");

  // clang-format off
  settings.alias().add_path_to_settings()
      ("update", "Update check",
       "Configuration for the check_nscp_update command which checks GitHub for newer NSClient++ releases.")
      ;

  settings.alias().add_key_to_settings("update")
      .add_int("cache hours", sh::uint_key(&update_cache_hours_, 24),
               "Cache duration",
               "Number of hours to cache the latest version lookup. The GitHub API is queried at most once per cache window to avoid rate limits.")
      .add_bool("check experimental", sh::bool_key(&update_check_experimental_, false),
               "Include pre-releases",
               "When true, GitHub pre-releases (experimental builds) are also considered when determining the latest available version. When false (default) only stable releases are considered.")
      .add_string("url", sh::string_key(&update_url_, "https://api.github.com/repos/mickem/nscp/releases"),
               "Update URL",
               "Base URL of the GitHub releases API used to look up the latest NSClient++ version. Point this at a mirror or internal proxy when running in environments without direct GitHub access.")
      .add_string("tls version", sh::string_key(&update_tls_version_, "tlsv1.2+"),
               "Minimum TLS version",
               "Minimum TLS protocol version accepted when fetching the GitHub releases endpoint. Defaults to tlsv1.2+ which permits TLS 1.2 and TLS 1.3 only. Allowed values: tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3.")
      .add_string("verify mode", sh::string_key(&update_verify_mode_, "peer"),
               "Certificate verify mode",
               "TLS certificate verification mode applied to the update endpoint. Defaults to 'peer' so the server certificate chain is validated against the configured CA bundle. Set to 'none' to disable verification (not recommended).")
      .add_string("ca", sh::string_key(&update_ca_, default_ca),
               "CA bundle",
               "Path to a CA bundle used to verify the update endpoint certificate. Defaults to the trusted system CA store; point at a private bundle when running behind a TLS-inspecting proxy.")
      ;
  // clang-format on

  settings.register_all();
  settings.notify();

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

int get_crashes(const boost::filesystem::path &root, std::string &last_crash) {
  if (!boost::filesystem::is_directory(root)) {
    return 0;
  }
  int count = 0;

  time_t last_write = std::time(nullptr);
  const boost::filesystem::directory_iterator begin(root);
  const boost::filesystem::directory_iterator end;
  for (const auto &p : boost::make_iterator_range(begin, end)) {
    if (boost::filesystem::is_regular_file(p) && file_helpers::meta::get_extension(p) == "txt") count++;
    const time_t lw = boost::filesystem::last_write_time(p);
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

namespace check_nscp_version {
struct filter_obj {
  nscp_version version;

  filter_obj() = default;
  explicit filter_obj(const nscp_version &version) : version(version) {}
  std::string show() const { return version.to_string(); }

  long long get_major() const { return version.major_version; }
  long long get_minor() const { return version.minor_version; }
  long long get_build() const { return version.build; }
  long long get_release() const { return version.release; }
  std::string get_version_s() const { return version.to_string(); }
  std::string get_date_s() const { return version.date; }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string("version", &filter_obj::get_version_s, "The NSClient++ Version as a string")
        .add_string("date", &filter_obj::get_date_s, "The NSClient++ Build date");
    registry_.add_int_x("release", &filter_obj::get_release, "The release (the 0 in 0.1.2.3)")
        .add_int_x("major", &filter_obj::get_major, "The major (the 1 in 0.1.2.3)")
        .add_int_x("minor", &filter_obj::get_minor, "The minor (the 2 in 0.1.2.3)")
        .add_int_x("build", &filter_obj::get_build, "The build (the 3 in 0.1.2.3) not available in release versions after 0.6.0");
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

  const boost::shared_ptr<filter_obj> record(new filter_obj(version));
  filter.match(record);

  filter_helper.post_process(filter);
}

}  // namespace check_nscp_version

namespace check_nscp_update {

using check_nscp_helpers::compare;
using check_nscp_helpers::sanitize_tag;

struct filter_obj {
  nscp_version current;
  nscp_version latest;
  std::string latest_tag;
  std::string published;
  std::string url;
  std::string error;
  bool have_latest;

  filter_obj() : have_latest(false) {}

  std::string show() const {
    if (!error.empty()) return "error: " + error;
    if (!have_latest) return current.to_string();
    return current.to_string() + " (latest: " + latest.to_string() + ")";
  }

  long long get_update_available() const {
    if (!have_latest) return 0;
    return compare(current, latest) < 0 ? 1 : 0;
  }
  long long get_versions_behind() const {
    if (!have_latest) return 0;
    const int c = compare(current, latest);
    return c < 0 ? -c : 0;
  }
  std::string get_version_s() const { return current.to_string(); }
  std::string get_date_s() const { return current.date; }
  long long get_release() const { return current.release; }
  long long get_major() const { return current.major_version; }
  long long get_minor() const { return current.minor_version; }
  long long get_build() const { return current.build; }

  std::string get_latest_version_s() const { return have_latest ? latest.to_string() : std::string(); }
  long long get_latest_release() const { return have_latest ? latest.release : 0; }
  long long get_latest_major() const { return have_latest ? latest.major_version : 0; }
  long long get_latest_minor() const { return have_latest ? latest.minor_version : 0; }
  long long get_latest_build() const { return have_latest ? latest.build : 0; }
  std::string get_latest_tag_s() const { return latest_tag; }
  std::string get_published_s() const { return published; }
  std::string get_url_s() const { return url; }
  std::string get_error_s() const { return error; }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : native_context {
  filter_obj_handler() {
    registry_.add_string("version", &filter_obj::get_version_s, "The currently installed NSClient++ version")
        .add_string("date", &filter_obj::get_date_s, "The build date of the currently installed NSClient++")
        .add_string("latest_version", &filter_obj::get_latest_version_s, "The latest available NSClient++ version (empty if lookup failed)")
        .add_string("tag", &filter_obj::get_latest_tag_s, "The GitHub tag of the latest release")
        .add_string("published", &filter_obj::get_published_s, "Publication date of the latest release")
        .add_string("url", &filter_obj::get_url_s, "URL of the latest release on GitHub")
        .add_string("error", &filter_obj::get_error_s, "Error message if the latest version could not be determined (empty when ok)");
    registry_.add_int_x("release", &filter_obj::get_release, "The release component of the installed version (the 0 in 0.1.2.3)")
        .add_int_x("major", &filter_obj::get_major, "The major component of the installed version (the 1 in 0.1.2.3)")
        .add_int_x("minor", &filter_obj::get_minor, "The minor component of the installed version (the 2 in 0.1.2.3)")
        .add_int_x("build", &filter_obj::get_build, "The build component of the installed version (the 3 in 0.1.2.3)")
        .add_int_x("latest_release", &filter_obj::get_latest_release, "The release component of the latest available version")
        .add_int_x("latest_major", &filter_obj::get_latest_major, "The major component of the latest available version")
        .add_int_x("latest_minor", &filter_obj::get_latest_minor, "The minor component of the latest available version")
        .add_int_x("latest_build", &filter_obj::get_latest_build, "The build component of the latest available version")
        .add_int_x("update_available", &filter_obj::get_update_available,
                   "1 when the latest available version is newer than the running version, 0 otherwise (and 0 if the lookup failed)")
        .add_int_x("versions_behind", &filter_obj::get_versions_behind,
                   "Difference between latest and current version components (largest meaningful component) when an update is available, 0 otherwise");
  }
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_nscp_update

void CheckNSCP::check_nscp_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) const {
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

void CheckNSCP::check_nscp_update(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_nscp_update::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("update_available = 1", "update_available = 1", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${version} (latest: ${latest_version})", "version", "", "");

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  boost::shared_ptr<check_nscp_update::filter_obj> record(new check_nscp_update::filter_obj());

  // Parse the running version up-front. If we cannot determine the running
  // version there is no point in querying GitHub; bail with a UNKNOWN-style
  // bad response.
  try {
    record->current = nscp_version(get_core()->getApplicationVersionString());
  } catch (const std::exception &e) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to parse running version: " + utf8::utf8_from_native(e.what()));
    return;
  }

  // Snapshot configuration and decide whether the cached value is still fresh.
  bool include_prerelease;
  std::string url;
  std::string tls_version;
  std::string verify_mode;
  std::string ca_bundle;
  bool need_refresh = true;
  std::string cached_tag;
  std::string cached_published;
  std::string cached_url;
  std::string cached_version;
  std::string cached_error;
  {
    unsigned int cache_hours;
    boost::unique_lock<boost::timed_mutex> lock(update_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!lock.owns_lock()) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to acquire update cache lock");
      return;
    }
    cache_hours = update_cache_hours_;
    include_prerelease = update_check_experimental_;
    url = update_url_;
    tls_version = update_tls_version_;
    verify_mode = update_verify_mode_;
    ca_bundle = update_ca_;
    if (update_cache_valid_) {
      const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
      const boost::posix_time::time_duration age = now - update_cached_at_;
      if (age.is_negative() || age < boost::posix_time::hours(static_cast<long>(cache_hours))) {
        need_refresh = false;
        cached_tag = update_cached_tag_;
        cached_published = update_cached_published_;
        cached_url = update_cached_url_;
        cached_version = update_cached_version_;
        cached_error = update_cached_error_;
      }
    }
  }

  if (need_refresh) {
    // Pick the right endpoint: /releases (full list, can include pre-releases)
    // when experimental builds are accepted; /releases/latest (stable only)
    // otherwise. If the caller customized the URL we honor it as-is unless it
    // ends in "/releases" in which case we apply the same heuristic.
    std::string fetch_url = url;
    if (fetch_url.size() >= 9 && fetch_url.substr(fetch_url.size() - 9) == "/releases" && !include_prerelease) {
      fetch_url += "/latest";
    }

    std::string fetched_tag;
    std::string fetched_published;
    std::string fetched_html_url;
    std::string fetch_error;
    try {
      const http::parsed_url parsed = http::parse_url(fetch_url);
      if (parsed.host.empty()) {
        fetch_error = "invalid update URL: " + fetch_url;
      } else {
        // Defaults: "tlsv1.2+" rejects anything below TLS 1.2; verify="peer"
        // validates the server certificate against the configured CA bundle
        // (defaulting to the system trust store via ${ca-path}). The hostname
        // is checked separately by ssl_socket via rfc2818_verification.
        http::http_client_options opts(parsed.protocol, tls_version, verify_mode, ca_bundle);
        http::request rq("GET", parsed.host, parsed.path);
        rq.add_header("Accept", "application/vnd.github+json");
        // GitHub requires a User-Agent. Identify ourselves with the running
        // version so server-side logs/metrics can attribute the requests.
        rq.add_header("User-Agent", "NSClient++/" + record->current.to_string() + " check_nscp_update");
        http::simple_client client(opts);
        const http::response resp = client.fetch(parsed.host, parsed.port, rq);
        if (resp.status_code_ < 200 || resp.status_code_ > 299) {
          fetch_error = "HTTP " + str::xtos(resp.status_code_) + " from " + fetch_url;
        } else if (!check_nscp_helpers::parse_releases_payload(resp.payload_, include_prerelease, fetched_tag, fetched_html_url, fetched_published,
                                                               fetch_error)) {
          // fetch_error populated by parse_releases_payload.
        }
      }
    } catch (const std::exception &e) {
      fetch_error = std::string("HTTP request failed: ") + utf8::utf8_from_native(e.what());
    }

    std::string parsed_version_string;
    if (fetch_error.empty()) {
      const std::string sanitized = check_nscp_update::sanitize_tag(fetched_tag);
      try {
        nscp_version v(sanitized);
        parsed_version_string = v.to_string();
      } catch (const std::exception &e) {
        fetch_error = "Failed to parse latest tag '" + fetched_tag + "': " + utf8::utf8_from_native(e.what());
      }
    }

    {
      boost::unique_lock<boost::timed_mutex> lock(update_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
      if (lock.owns_lock()) {
        update_cached_at_ = boost::posix_time::second_clock::universal_time();
        update_cache_valid_ = true;
        update_cached_tag_ = fetched_tag;
        update_cached_published_ = fetched_published;
        update_cached_url_ = fetched_html_url;
        update_cached_version_ = parsed_version_string;
        update_cached_error_ = fetch_error;
      }
    }
    cached_tag = fetched_tag;
    cached_published = fetched_published;
    cached_url = fetched_html_url;
    cached_version = parsed_version_string;
    cached_error = fetch_error;
  }

  if (!cached_error.empty()) {
    record->error = cached_error;
  } else if (!cached_version.empty()) {
    try {
      record->latest = nscp_version(cached_version);
      record->have_latest = true;
      record->latest_tag = cached_tag;
      record->published = cached_published;
      record->url = cached_url;
    } catch (const std::exception &e) {
      record->error = std::string("Failed to parse cached version: ") + utf8::utf8_from_native(e.what());
    }
  }

  filter.match(record);
  filter_helper.post_process(filter);
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

  auto err_count = get_errors(last);
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