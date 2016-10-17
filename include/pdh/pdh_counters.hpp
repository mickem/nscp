/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error.hpp>
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