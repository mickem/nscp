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
#include <pdh/core.hpp>

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
		std::wstring name_;
		PDH_FMT_COUNTERVALUE data_;
		typedef boost::shared_ptr<PDHCounterListener> listener_ptr;
		listener_ptr listener_;

	public:

		PDHCounter(std::wstring name, listener_ptr listener) : name_(name), listener_(listener), hCounter_(NULL){}
		PDHCounter(std::wstring name) : name_(name), hCounter_(NULL){}
		virtual ~PDHCounter(void) {
			if (hCounter_ != NULL)
				remove();
		}

		void setListener(listener_ptr listener) {
			listener_ = listener;
		}

		PDHCounterInfo getCounterInfo(BOOL bExplainText = FALSE) {
			if (hCounter_ == NULL)
				throw PDHException(_T("Counter is null!"));
			BYTE *lpBuffer = new BYTE[1025];
			DWORD bufSize = 1024;
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhGetCounterInfo(hCounter_, bExplainText, &bufSize, (PDH_COUNTER_INFO*)lpBuffer);
			if (status.is_error())
				throw PDHException(name_, _T("getCounterInfo failed (no query)"), status);
			return PDHCounterInfo(lpBuffer, bufSize, TRUE);
		}
		const PDH::PDH_HCOUNTER getCounter() const {
			return hCounter_;
		}
		const std::wstring getName() const {
			return name_;
		}
		void addToQuery(PDH::PDH_HQUERY hQuery) {
			if (hQuery == NULL)
				throw PDHException(name_, std::wstring(_T("addToQuery failed (no query).")));
			if (hCounter_ != NULL)
				throw PDHException(name_, std::wstring(_T("addToQuery failed (already opened).")));
			if (listener_)
				listener_->attach(this);
			LPCWSTR name = name_.c_str();
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhAddCounter(hQuery, name, 0, &hCounter_);
			if (status.is_error()) {
				hCounter_ = NULL;
				throw PDHException(name_, _T("PdhAddCounter failed"), status);
			}
			if (hCounter_ == NULL)
				throw PDHException(_T("Counter is null!"));
		}
		void remove() {
			if (hCounter_ == NULL)
				return;
			if (listener_)
				listener_->detach(this);
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhRemoveCounter(hCounter_);
			if (status.is_error())
				throw PDHException(name_, _T("PdhRemoveCounter failed"), status);
			hCounter_ = NULL;
		}
		void collect() {
			if (hCounter_ == NULL)
				return;
			if (!listener_)
				return;
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhGetFormattedCounterValue(hCounter_, listener_->getFormat(), NULL, &data_);
			if (status.is_error()) {
				throw PDHException(name_, _T("PdhGetFormattedCounterValue failed"), status);
			}
			listener_->collect(*this);
		}
		double getDoubleValue() const {
			return data_.doubleValue;
		}
		__int64 getInt64Value() const {
			return data_.largeValue;
		}
		long getIntValue() const {
			return data_.longValue;
		}
		std::wstring getStringValue() const {
			return data_.WideStringValue;
		}
	};
}