// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_certificate.hpp"

#include <string>
#include <vector>

#include <boost/algorithm/string/join.hpp>

#include <nscapi/nscapi_program_options.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/helpers.hpp>

#include "cert_filter.hpp"
#include "cert_source.hpp"

namespace po = boost::program_options;

namespace check_certificate_command {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  modern_filter::data_container data;
  modern_filter::cli_helper<cert_filter::filter> filter_helper(request, response, data);

  std::vector<std::string> paths;
  bool recursive = false;
  std::string store;
  std::string location = "LocalMachine";
  std::string ca_file;
  std::string password;

  cert_filter::filter filter;
  // Default thresholds mirror Icinga's check_certificate (warn 30d, crit 10d).
  filter_helper.add_options("expires_in < 30", "expires_in < 10", "", filter.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${problem_list}", "${subject} expires in ${expires_in}d (${valid_to})", "${subject}", "No certificates found",
                           "%(status): all %(count) certificate(s) are ok");
  // clang-format off
  filter_helper.get_desc().add_options()
    ("file", po::value<std::vector<std::string>>(&paths), "A certificate file (PEM or DER) or a directory of them. Can be given multiple times.")
    ("path", po::value<std::vector<std::string>>(&paths), "Alias for file.")
    ("recursive", po::value<bool>(&recursive)->implicit_value(true)->default_value(false), "Recurse into directories given via file=/path=.")
    ("password", po::value<std::string>(&password), "Password for PKCS#12 (.pfx/.p12) files.")
    ("ca", po::value<std::string>(&ca_file), "CA bundle to evaluate the 'trusted' keyword against (defaults to the system trust store).")
    ("store", po::value<std::string>(&store), "Windows certificate store to enumerate (e.g. My, Root, CA). Windows only.")
    ("location", po::value<std::string>(&location)->default_value("LocalMachine"), "Windows store location: LocalMachine or CurrentUser. Windows only.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(filter)) return;

  std::vector<cert_filter::filter_obj_ptr> certs;
  std::vector<std::string> errors;

  if (!paths.empty()) cert_source::load_files(paths, recursive, ca_file, password, certs, errors);

  if (!store.empty()) {
#ifdef WIN32
    std::string store_error;
    cert_source::load_store(store, location, ca_file, certs, store_error);
    if (!store_error.empty()) errors.push_back(store_error);
#else
    return nscapi::protobuf::functions::set_response_bad(*response, "store= (certificate store) is only supported on Windows; use file= on this platform");
#endif
  }

  if (paths.empty() && store.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Nothing to check: specify file=<path> (a cert file or directory)"
#ifdef WIN32
                                                                    " or store=<name>"
#endif
    );
  }

  if (certs.empty()) {
    const std::string why = errors.empty() ? "" : (": " + boost::algorithm::join(errors, "; "));
    return nscapi::protobuf::functions::set_response_bad(*response, "No certificates found" + why);
  }

  parsers::where::constants::reset();
  for (const cert_filter::filter_obj_ptr &c : certs) filter.match(c);
  filter_helper.post_process(filter);
}

}  // namespace check_certificate_command
