// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <str/utf8.hpp>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_interface.hpp>

#include "pdh_resolver.hpp"

namespace PDH {
PDHCounter::PDHCounter(const pdh_instance &counter) : hCounter_(nullptr), counter_(counter), data_() {}
PDHCounter::~PDHCounter() {
  if (hCounter_ != nullptr) remove();
}

pdh_error PDHCounter::validate() const { return factory::get_impl()->PdhValidatePath(utf8::cvt<std::wstring>(counter_->get_counter()).c_str(), false); }

counter_info PDHCounter::getCounterInfo(BOOLEAN bExplainText) const {
  if (hCounter_ == nullptr) throw pdh_exception("Counter is null!");
  // counter_info copies the fields it needs out of the buffer; it does not
  // take ownership.
  auto buffer = std::make_unique<BYTE[]>(1025);
  DWORD bufSize = 1024;
  const pdh_error status = factory::get_impl()->PdhGetCounterInfo(hCounter_, bExplainText, &bufSize, reinterpret_cast<PDH_COUNTER_INFO_W *>(buffer.get()));
  if (status.is_error()) throw pdh_exception(getName() + " getCounterInfo failed (no query)", status);
  return counter_info(buffer.get(), bufSize, TRUE);
}
PDH_HCOUNTER PDHCounter::getCounter() const { return hCounter_; }
std::string PDHCounter::getName() const { return counter_->get_name(); }
std::string PDHCounter::get_path() const { return counter_->get_counter(); }
namespace {
// Add a counter path to the query, falling back to PdhAddEnglishCounter if
// the localized lookup fails in a way that suggests a locale mismatch.
// Different Windows SKUs / CU levels have been observed returning
// PDH_CSTATUS_NO_OBJECT, PDH_CSTATUS_BAD_COUNTERNAME, or PDH_INVALID_ARGUMENT
// for the same "English counter path on non-English Windows" case (issues
// #652, #906), so we treat all of them as cues to retry with the English API.
pdh_error add_counter_with_english_fallback(PDH_HQUERY hQuery, const std::wstring &path, PDH_HCOUNTER *out) {
  *out = nullptr;
  pdh_error status = factory::get_impl()->PdhAddCounter(hQuery, path.c_str(), 0, out);
  if (status.is_possible_locale_mismatch()) {
    *out = nullptr;
    status = factory::get_impl()->PdhAddEnglishCounter(hQuery, path.c_str(), 0, out);
  }
  return status;
}
}  // namespace

void PDHCounter::addToQuery(PDH_HQUERY hQuery) {
  if (hQuery == nullptr) throw pdh_exception(getName(), "addToQuery failed (no query).");
  if (hCounter_ != nullptr) throw pdh_exception(getName(), "addToQuery failed (already opened).");

  pdh_error status;
  switch (counter_->get_resolution()) {
    case types::resolution_english:
      // Force English counter names regardless of the system locale.
      hCounter_ = nullptr;
      status = factory::get_impl()->PdhAddEnglishCounter(hQuery, utf8::cvt<std::wstring>(counter_->get_counter()).c_str(), 0, &hCounter_);
      break;

    case types::resolution_index: {
      // Force numeric-index -> localized-name expansion, then add the localized
      // path. expand_index is a no-op when the path contains no counter indices.
      std::string expanded = counter_->get_counter();
      PDHResolver::expand_index(expanded);
      hCounter_ = nullptr;
      status = factory::get_impl()->PdhAddCounter(hQuery, utf8::cvt<std::wstring>(expanded).c_str(), 0, &hCounter_);
      break;
    }

    case types::resolution_auto:
    default: {
      const std::wstring path = utf8::cvt<std::wstring>(counter_->get_counter());
      status = add_counter_with_english_fallback(hQuery, path, &hCounter_);

      // If the path still looks wrong, try one more time with numeric counter
      // indices expanded to their localized names. expand_index is a no-op when
      // the path contains no digits, so this is cheap when it doesn't apply.
      if (status.is_possible_locale_mismatch()) {
        std::string expanded = counter_->get_counter();
        PDHResolver::expand_index(expanded);
        status = add_counter_with_english_fallback(hQuery, utf8::cvt<std::wstring>(expanded), &hCounter_);
        if (status.is_possible_locale_mismatch()) {
          hCounter_ = nullptr;
          throw pdh_exception(expanded + " counter not found", status);
        }
      }
      break;
    }
  }

  if (status.is_error()) {
    hCounter_ = nullptr;
    throw pdh_exception(getName() + " PdhAddCounter failed", status);
  }
  if (hCounter_ == nullptr) throw pdh_exception("Counter is null!");
}
void PDHCounter::remove() {
  if (hCounter_ == nullptr) return;
  const pdh_error status = factory::get_impl()->PdhRemoveCounter(hCounter_);
  if (status.is_error()) throw pdh_exception(getName() + " PdhRemoveCounter failed", status);
  hCounter_ = nullptr;
}
pdh_error PDHCounter::collect() {
  if (hCounter_ == nullptr) return {};
  const pdh_error status = factory::get_impl()->PdhGetFormattedCounterValue(hCounter_, counter_->get_format(), nullptr, &data_);
  if (!status.is_error()) {
    counter_->collect(data_);
  }
  return status;
}
double PDHCounter::getDoubleValue() const { return data_.doubleValue; }
__int64 PDHCounter::getInt64Value() const { return data_.largeValue; }
long PDHCounter::getIntValue() const { return data_.longValue; }
std::wstring PDHCounter::getStringValue() const { return data_.WideStringValue; }
}  // namespace PDH
