// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "storage_facts.hpp"

#include <cctype>
#include <nscapi/nscapi_facts_helper.hpp>

namespace storage_facts {

const char *const set_storage = "storage";
const char *const key_volumes = "volumes";
const char *const id_volumes = "storage.volumes";

void publish(const std::vector<volume> &volumes, const std::time_t taken_at, nscapi::facts::response &out) {
  // The list is written even when it is empty: a host with no volume we could
  // read has told us something, and an absent list would read as "not
  // collected".
  nscapi::facts::record_list list = out.set(set_storage).list(key_volumes);
  for (const volume &v : volumes) {
    nscapi::facts::section record = list.record(v.id);
    record.value("device", v.device).value("filesystem", v.filesystem).value("type", v.type).value("label", v.label);
    // Zero is "not read" (a remote volume, a drive with no media), and the
    // builder writes a number as it is given, so the guard is here.
    if (v.size_bytes > 0) record.value("size_bytes", v.size_bytes);
  }
  out.gathered(set_storage, taken_at);
}

namespace {
int hex_value(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

std::string decode_udev_label(const std::string &escaped) {
  std::string out;
  out.reserve(escaped.size());
  for (std::string::size_type i = 0; i < escaped.size(); ++i) {
    if (escaped[i] == '\\' && i + 3 < escaped.size() && escaped[i + 1] == 'x') {
      const int high = hex_value(escaped[i + 2]);
      const int low = hex_value(escaped[i + 3]);
      if (high >= 0 && low >= 0) {
        out += static_cast<char>(high * 16 + low);
        i += 3;
        continue;
      }
    }
    out += escaped[i];
  }
  return out;
}

}  // namespace storage_facts
