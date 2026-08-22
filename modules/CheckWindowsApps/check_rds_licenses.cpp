// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "check_rds_licenses.hpp"

#include <boost/program_options.hpp>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>
#include <vector>
#include <win/com_helpers.hpp>
#include <win/wmi/wmi_query.hpp>

#include "check_rds_internal.hpp"

namespace po = boost::program_options;

namespace check_rds {

using check_rds_internal::keypack_type_name;
using check_rds_internal::license_key_pack;

namespace {

// Fetch all Win32_TSLicenseKeyPack rows. Returns false when the class does
// not exist on this host (the Remote Desktop licensing role is not
// installed); throws wmi_exception on real errors.
bool fetch_key_packs(std::vector<license_key_pack> &out) {
  // The class only exists on RD licensing servers; WMI reports its absence as
  // an invalid-class/not-found error from either ExecQuery or the first Next.
  const auto is_missing_class = [](const HRESULT hr) { return hr == WBEM_E_INVALID_CLASS || hr == WBEM_E_NOT_FOUND; };
  try {
    wmi_impl::query wmi_query(
        "SELECT KeyPackId, TypeAndModel, ProductVersion, KeyPackType, TotalLicenses, IssuedLicenses, AvailableLicenses FROM Win32_TSLicenseKeyPack",
        "root\\cimv2", "", "");
    wmi_impl::row_enumerator rows = wmi_query.execute();
    while (rows.has_next()) {
      const wmi_impl::row &row = rows.get_next();
      license_key_pack pack;
      pack.id = row.get_int("KeyPackId");
      pack.description = row.get_string("TypeAndModel");
      pack.product_version = row.get_string("ProductVersion");
      pack.keypack_type = row.get_int("KeyPackType");
      pack.total = row.get_int("TotalLicenses");
      pack.issued = row.get_int("IssuedLicenses");
      pack.available = row.get_int("AvailableLicenses");
      out.push_back(pack);
    }
    return true;
  } catch (const wmi_impl::wmi_exception &e) {
    if (is_missing_class(e.get_code())) return false;
    throw;
  }
}

}  // namespace

namespace check_rds_licenses_filter {

struct filter_obj {
  license_key_pack pack;
  explicit filter_obj(license_key_pack pack) : pack(std::move(pack)) {}

  std::string show() const { return pack.description + " (" + std::to_string(pack.issued) + "/" + std::to_string(pack.total) + " issued)"; }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler() {
    registry_.add_string_var("description", [](auto obj) { return obj->pack.description; }, "License type and model (e.g. 'RDS Per User CAL')");
    registry_.add_string_var("product_version", [](auto obj) { return obj->pack.product_version; }, "Product version the key pack applies to");
    registry_.add_string_var("type", [](auto obj) { return keypack_type_name(obj->pack.keypack_type); },
                             "Key pack type: unknown, retail, volume, concurrent, temporary, open or built-in");
    registry_.add_int_var("id", parsers::where::type_int, [](auto obj) { return obj->pack.id; }, "Key pack id");
    registry_.add_int_var("keypack_type", parsers::where::type_int, [](auto obj) { return obj->pack.keypack_type; }, "Raw KeyPackType value");
    registry_.add_int_var("total_licenses", parsers::where::type_int, [](auto obj) { return obj->pack.total; }, "Total licenses in the key pack")
        .add_int_perf("", "", "_total");
    registry_.add_int_var("issued", parsers::where::type_int, [](auto obj) { return obj->pack.issued; }, "Licenses issued to clients")
        .add_int_perf("", "", "_issued");
    registry_.add_int_var("available", parsers::where::type_int, [](auto obj) { return obj->pack.available; }, "Licenses still available")
        .add_int_perf("", "", "_available");
  }
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_rds_licenses_filter

void check_rds_licenses(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  using check_rds_licenses_filter::filter;
  using check_rds_licenses_filter::filter_obj;

  modern_filter::data_container data;
  modern_filter::cli_helper<filter> filter_helper(request, response, data);

  filter f;
  // The `total_licenses > 0` guard keeps the built-in/unlimited packs (which report no
  // meaningful counts) from tripping the exhaustion thresholds.
  filter_helper.add_options("available < 10 and total_licenses > 0", "available = 0 and total_licenses > 0", "", f.get_filter_syntax(), "unknown");
  filter_helper.add_syntax("${status}: ${list}", "${description}: ${issued}/${total_licenses} issued, ${available} available", "${description}",
                           "No license key packs found", "");
  if (!filter_helper.parse_options()) return;
  if (!filter_helper.build_filter(f)) return;
  f.add_manual_perf("issued");
  f.add_manual_perf("available");

  const com_helper::mta_scope com;
  try {
    std::vector<license_key_pack> packs;
    if (!fetch_key_packs(packs)) {
      return nscapi::protobuf::functions::set_response_bad(
          *response, "Remote Desktop licensing information not available: the Remote Desktop licensing role is not installed (Win32_TSLicenseKeyPack missing)");
    }
    for (const license_key_pack &pack : packs) f.match(std::make_shared<filter_obj>(pack));
  } catch (const wmi_impl::wmi_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to query Win32_TSLicenseKeyPack: " + e.reason());
  }

  filter_helper.post_process(f);
}

}  // namespace check_rds
