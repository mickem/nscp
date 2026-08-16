// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace activation_filter {

// The ApplicationID every Windows SKU shares in SoftwareLicensingProduct; Office
// and other licensed products carry different ones.
extern const char *windows_application_id;

// SLDATATYPE / SL_GENUINE_STATE mirrors, redefined here so the decoding logic
// (and its unit tests) build on every platform.
enum license_status {
  status_unlicensed = 0,
  status_licensed = 1,
  status_oob_grace = 2,
  status_oot_grace = 3,
  status_non_genuine_grace = 4,
  status_notification = 5,
  status_extended_grace = 6,
};
enum genuine_state {
  genuine_is_genuine = 0,
  genuine_invalid_license = 1,
  genuine_tampered = 2,
  genuine_offline = 3,
  genuine_unknown = -1,
};

// Human readable name of a SoftwareLicensingProduct.LicenseStatus value.
std::string license_status_name(long long status);
// Human readable name of an SL_GENUINE_STATE value.
std::string genuine_state_name(long long state);
// GracePeriodRemaining is expressed in minutes; render whole days (rounded down).
long long grace_minutes_to_days(long long minutes);

// One licensed product (normally just Windows itself).
struct filter_obj {
  filter_obj()
      : license_status(status_unlicensed),
        license_status_reason(0),
        grace_minutes(0),
        grace_days(0),
        genuine_status(genuine_unknown),
        licensed(0),
        genuine(0),
        is_windows(0) {}

  std::string get_name() const { return name; }
  std::string get_description() const { return description; }
  std::string get_id() const { return id; }
  std::string get_key() const { return key; }
  std::string get_channel() const { return channel; }
  std::string get_status() const { return license_status_name(license_status); }
  std::string get_genuine_state() const { return genuine_state_name(genuine_status); }
  long long get_license_status() const { return license_status; }
  long long get_license_status_reason() const { return license_status_reason; }
  long long get_grace_minutes() const { return grace_minutes; }
  long long get_grace_days() const { return grace_days; }
  long long get_licensed() const { return licensed; }
  long long get_genuine() const { return genuine; }
  long long get_is_windows() const { return is_windows; }
  std::string show() const { return name.empty() ? id : name; }

  std::string name;         // e.g. "Windows(R), Professional edition"
  std::string description;  // e.g. "Windows(R) Operating System, VOLUME_KMSCLIENT channel"
  std::string id;           // SKU id (GUID)
  std::string app_id;       // application id (GUID); Windows uses windows_application_id
  std::string key;          // PartialProductKey (the last five characters)
  std::string channel;      // ProductKeyChannel: Retail, Volume:MAK, Volume:GVLK, OEM, ...

  long long license_status;         // raw LicenseStatus (0-6)
  long long license_status_reason;  // raw LicenseStatusReason (HRESULT-ish code)
  long long grace_minutes;          // GracePeriodRemaining, minutes (0 = no grace period applies)
  long long grace_days;             // grace_minutes rendered as whole days
  long long genuine_status;         // SL_GENUINE_STATE, -1 when it could not be determined
  long long licensed;               // 1 when license_status == status_licensed
  long long genuine;                // 1 when genuine_status == genuine_is_genuine
  long long is_windows;             // 1 when app_id is the Windows application id
};

// Fill in the derived fields (licensed/genuine/grace_days/is_windows) from the
// raw values read off the platform. Kept platform neutral so it is unit tested.
void finalize(filter_obj &obj);

typedef std::shared_ptr<filter_obj> filter_obj_ptr;
typedef parsers::where::filter_handler_impl<filter_obj_ptr> native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace activation_filter

namespace activation_source {
// Windows only (WMI SoftwareLicensingProduct + SLIsGenuineLocal); the Unix stub
// sets `error`. When `all_products` is false only Windows itself is reported.
void gather(bool all_products, bool with_genuine, std::vector<activation_filter::filter_obj_ptr> &out, std::string &error);
}  // namespace activation_source

namespace check_activation_command {
void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
