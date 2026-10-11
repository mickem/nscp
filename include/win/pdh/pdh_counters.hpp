// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <error/error.hpp>
#include <memory>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
class PDHCounter;
class PDHCounterListener {
 public:
  virtual ~PDHCounterListener() = default;
  virtual void collect(const PDHCounter &counter) = 0;
  virtual void attach(const PDHCounter *counter) = 0;
  virtual void detach(const PDHCounter *counter) = 0;
  virtual DWORD getFormat() const = 0;
};

class PDHCounter {
  PDH_HCOUNTER hCounter_;
  pdh_instance counter_;
  PDH_FMT_COUNTERVALUE data_;
  // The implementation hCounter_ came from, which every later call on it
  // must go to: bound by addToQuery(), to the query's own.
  std::shared_ptr<impl_interface> impl_;
  // Consecutive gathers that ended without a value because PDH had no data
  // for the instance (INVALID_DATA after the retry, or NO_INSTANCE). Kept by
  // PDHQuery::gatherData().
  int missing_ticks_;

 public:
  explicit PDHCounter(const pdh_instance &counter);
  ~PDHCounter();
  pdh_error validate() const;

  counter_info getCounterInfo(BOOLEAN bExplainText = FALSE) const;
  PDH_HCOUNTER getCounter() const;
  std::string getName() const;
  std::string get_path() const;
  const pdh_instance &get_instance() const { return counter_; }
  int missing_ticks() const { return missing_ticks_; }
  void note_missing() { ++missing_ticks_; }
  void note_present() { missing_ticks_ = 0; }
  void addToQuery(const std::shared_ptr<impl_interface> &impl, PDH_HQUERY hQuery);
  void remove();
  pdh_error collect();
  double getDoubleValue() const;
  __int64 getInt64Value() const;
  long getIntValue() const;
  std::wstring getStringValue() const;
};
}  // namespace PDH