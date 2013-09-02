/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error.hpp>
#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_counters.hpp>

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
			throw pdh_exception(getName() +  " PdhRemoveCounter failed", status);
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