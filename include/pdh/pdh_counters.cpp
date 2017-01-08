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
#include <error/error.hpp>
#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_counters.hpp>

#include <utf8.hpp>

namespace PDH {
	PDHCounter::PDHCounter(pdh_instance counter) : counter_(counter), hCounter_(NULL) {}
	PDHCounter::~PDHCounter() {
		if (hCounter_ != NULL)
			remove();
	}

	pdh_error PDHCounter::validate() {
		return factory::get_impl()->PdhValidatePath(utf8::cvt<std::wstring>(counter_->get_counter()).c_str(), false);
	}

	counter_info PDHCounter::getCounterInfo(BOOL bExplainText) {
		if (hCounter_ == NULL)
			throw pdh_exception("Counter is null!");
		BYTE *lpBuffer = new BYTE[1025];
		DWORD bufSize = 1024;
		pdh_error status = factory::get_impl()->PdhGetCounterInfo(hCounter_, bExplainText, &bufSize, (PDH_COUNTER_INFO*)lpBuffer);
		if (status.is_error())
			throw pdh_exception(getName() + " getCounterInfo failed (no query)", status);
		return counter_info(lpBuffer, bufSize, TRUE);
	}
	const PDH::PDH_HCOUNTER PDHCounter::getCounter() const {
		return hCounter_;
	}
	const std::string PDHCounter::getName() const {
		return counter_->get_name();
	}
	const std::string PDHCounter::get_path() const {
		return counter_->get_counter();
	}
	void PDHCounter::addToQuery(PDH::PDH_HQUERY hQuery) {
		if (hQuery == NULL)
			throw pdh_exception(getName(), "addToQuery failed (no query).");
		if (hCounter_ != NULL)
			throw pdh_exception(getName(), "addToQuery failed (already opened).");
		pdh_error status = factory::get_impl()->PdhAddCounter(hQuery, utf8::cvt<std::wstring>(counter_->get_counter()).c_str(), 0, &hCounter_);
		if (status.is_not_found()) {
			hCounter_ = NULL;
			status = factory::get_impl()->PdhAddEnglishCounter(hQuery, utf8::cvt<std::wstring>(counter_->get_counter()).c_str(), 0, &hCounter_);
		}
		if (status.is_error()) {
			hCounter_ = NULL;
			throw pdh_exception(getName() + " PdhAddCounter failed", status);
		}
		if (hCounter_ == NULL)
			throw pdh_exception("Counter is null!");
	}
	void PDHCounter::remove() {
		if (hCounter_ == NULL)
			return;
		pdh_error status = factory::get_impl()->PdhRemoveCounter(hCounter_);
		if (status.is_error())
			throw pdh_exception(getName() + " PdhRemoveCounter failed", status);
		hCounter_ = NULL;
	}
	pdh_error PDHCounter::collect() {
		pdh_error status;
		if (hCounter_ == NULL)
			return pdh_error(false);
		status = factory::get_impl()->PdhGetFormattedCounterValue(hCounter_, counter_->get_format(), NULL, &data_);
		if (!status.is_error()) {
			counter_->collect(data_);
		}
		return status;
	}
	double PDHCounter::getDoubleValue() const {
		return data_.doubleValue;
	}
	__int64 PDHCounter::getInt64Value() const {
		return data_.largeValue;
	}
	long PDHCounter::getIntValue() const {
		return data_.longValue;
	}
	std::wstring PDHCounter::getStringValue() const {
		return data_.WideStringValue;
	}
}