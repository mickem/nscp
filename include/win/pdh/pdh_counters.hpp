// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <pdh.h>

#include <error/error.hpp>
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

 public:
  explicit PDHCounter(const pdh_instance &counter);
  ~PDHCounter();
  pdh_error validate() const;

  counter_info getCounterInfo(BOOLEAN bExplainText = FALSE) const;
  PDH_HCOUNTER getCounter() const;
  std::string getName() const;
  std::string get_path() const;
  void addToQuery(PDH_HQUERY hQuery);
  void remove();
  pdh_error collect();
  double getDoubleValue() const;
  __int64 getInt64Value() const;
  long getIntValue() const;
  std::wstring getStringValue() const;
};
}  // namespace PDH