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

#pragma once

#include <list>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_interface.hpp>
#include <win/pdh/pdh_resolver.hpp>

namespace PDH {
class PDHQuery : public subscriber {
 public:
  typedef std::shared_ptr<PDHCounter> counter_type;
  typedef std::list<counter_type> counter_list_type;
  counter_list_type counters_;
  PDH_HQUERY hQuery_;
  bool has_displayed_invalid_counter_;
  bool listener_registered_;

  PDHQuery() : hQuery_(nullptr), has_displayed_invalid_counter_(false), listener_registered_(false) {}
  virtual ~PDHQuery() noexcept;

  void addCounter(const pdh_instance& instance);
  void removeAllCounters();

  bool has_counters() const;

  void on_unload() override;
  void on_reload() override;

  bool is_open() const;
  void open();
  void close();

  void gatherData(bool ignore_errors = false);
  inline void collect() const;

  PDH_HQUERY getQueryHandle() const;
};
}  // namespace PDH