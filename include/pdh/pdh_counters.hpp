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
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error/error.hpp>
#include <pdh/pdh_interface.hpp>

namespace PDH {
	class PDHCounter;
	class PDHCounterListener {
	public:
		virtual void collect(const PDHCounter &counter) = 0;
		virtual void attach(const PDHCounter *counter) = 0;
		virtual void detach(const PDHCounter *counter) = 0;
		virtual DWORD getFormat() const = 0;
	};

	class PDHCounter {
	private:
		PDH::PDH_HCOUNTER hCounter_;
		pdh_instance counter_;
		PDH_FMT_COUNTERVALUE data_;

	public:

		PDHCounter(pdh_instance counter);
		~PDHCounter();
		pdh_error validate();

		counter_info getCounterInfo(BOOL bExplainText = FALSE);
		const PDH::PDH_HCOUNTER getCounter() const;
		const std::string getName() const;
		const std::string get_path() const;
		void addToQuery(PDH::PDH_HQUERY hQuery);
		void remove();
		pdh_error collect();
		double getDoubleValue() const;
		__int64 getInt64Value() const;
		long getIntValue() const;
		std::wstring getStringValue() const;
	};
}