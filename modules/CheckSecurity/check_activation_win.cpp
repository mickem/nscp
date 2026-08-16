// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Windows activation/licensing state:
//   SoftwareLicensingProduct (root\CIMV2) for the per-product license status,
//   grace period and channel, plus SLIsGenuineLocal (slc.dll) as a fallback for
//   the genuine state on builds that do not expose GenuineStatus over WMI.

#include <Windows.h>

#include <win/wmi/wmi_query.hpp>

#include "check_activation.hpp"

namespace activation_source {

namespace {

// Property availability varies between Windows builds, so every read tolerates a
// missing/NULL value instead of failing the whole query.
std::string read_str(const wmi_impl::row &r, const char *name) {
  try {
    const std::string value = r.get_string(name);
    return value == "<NULL>" ? std::string() : value;
  } catch (...) {
    return {};
  }
}
long long read_int(const wmi_impl::row &r, const char *name, const long long def) {
  try {
    return r.get_int(name);
  } catch (...) {
    return def;
  }
}

// Minimal mirror of the slc.dll surface we use (slpublic.h), declared locally so
// the module does not depend on that header or on linking Slc.lib.
typedef GUID SLID_local;
typedef HRESULT(WINAPI *SLIsGenuineLocal_fn)(const SLID_local *app_id, int *genuine_state, void *ui_options);

const SLID_local windows_slid = {0x55c92734, 0xd682, 0x4d71, {0x98, 0x3e, 0xd6, 0xec, 0x3f, 0x16, 0x05, 0x9f}};

// Ask the Software Licensing client for the genuine state of Windows. Returns
// activation_filter::genuine_unknown when slc.dll (or the call) is unavailable.
long long query_genuine_state() {
  const HMODULE slc = LoadLibraryW(L"slc.dll");
  if (slc == nullptr) return activation_filter::genuine_unknown;
  long long result = activation_filter::genuine_unknown;
  const auto is_genuine = reinterpret_cast<SLIsGenuineLocal_fn>(GetProcAddress(slc, "SLIsGenuineLocal"));
  if (is_genuine != nullptr) {
    int state = 0;
    if (SUCCEEDED(is_genuine(&windows_slid, &state, nullptr))) result = state;
  }
  FreeLibrary(slc);
  return result;
}

}  // namespace

void gather(const bool all_products, const bool with_genuine, std::vector<activation_filter::filter_obj_ptr> &out, std::string &error) {
  // SELECT * and read defensively: the property set differs between builds and a
  // named-column SELECT fails the whole query when one column is missing. Only
  // products with an installed key are of interest, which also keeps the (large)
  // SKU catalogue out of the result set.
  std::string query = "select * from SoftwareLicensingProduct where PartialProductKey is not null";
  if (!all_products) query += " and ApplicationID='" + std::string(activation_filter::windows_application_id) + "'";

  try {
    wmi_impl::query wmi_q(query, "root\\CIMV2", "", "");
    wmi_impl::row_enumerator rows = wmi_q.execute();
    while (rows.has_next()) {
      const wmi_impl::row r = rows.get_next();
      auto o = std::make_shared<activation_filter::filter_obj>();
      o->name = read_str(r, "Name");
      o->description = read_str(r, "Description");
      o->id = read_str(r, "ID");
      o->app_id = read_str(r, "ApplicationID");
      o->key = read_str(r, "PartialProductKey");
      o->channel = read_str(r, "ProductKeyChannel");
      o->license_status = read_int(r, "LicenseStatus", activation_filter::status_unlicensed);
      o->license_status_reason = read_int(r, "LicenseStatusReason", 0);
      o->grace_minutes = read_int(r, "GracePeriodRemaining", 0);
      // Windows 8 and later expose the genuine state on the product itself; older
      // builds (and some SKUs) do not, hence the slc.dll fallback below.
      o->genuine_status = with_genuine ? read_int(r, "GenuineStatus", activation_filter::genuine_unknown) : activation_filter::genuine_unknown;

      // A product without an installed key is a catalogue entry, not a license.
      if (o->key.empty()) continue;
      activation_filter::finalize(*o);
      out.push_back(o);
    }
  } catch (const wmi_impl::wmi_exception &e) {
    error = std::string("Failed to query the licensing state (SoftwareLicensingProduct): ") + e.what();
    return;
  }

  if (!with_genuine) return;
  // Fall back to the Software Licensing client for Windows itself when WMI did
  // not tell us; one call covers the machine, so only make it when needed.
  bool need_genuine = false;
  for (const activation_filter::filter_obj_ptr &o : out) {
    if (o->is_windows && o->genuine_status == activation_filter::genuine_unknown) need_genuine = true;
  }
  if (!need_genuine) return;
  const long long state = query_genuine_state();
  if (state == activation_filter::genuine_unknown) return;
  for (const activation_filter::filter_obj_ptr &o : out) {
    if (o->is_windows && o->genuine_status == activation_filter::genuine_unknown) {
      o->genuine_status = state;
      activation_filter::finalize(*o);
    }
  }
}

}  // namespace activation_source
