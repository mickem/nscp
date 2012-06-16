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
#include <pdh/resolver.hpp>
#include <boost/shared_ptr.hpp>

namespace PDH {
	class PDHQuery : public PDH::PDHImplSubscriber {
	public:
		typedef boost::shared_ptr<PDHCounter> counter_ptr;
		typedef boost::shared_ptr<PDHCounterListener> listener_ptr;
		typedef std::list<counter_ptr> CounterList;
		CounterList counters_;
		PDH::PDH_HQUERY hQuery_;
		bool hasDisplayedInvalidCOunter_;
	public:
		PDHQuery() : hQuery_(NULL), hasDisplayedInvalidCOunter_(false) {
		}
		virtual ~PDHQuery(void) {
			removeAllCounters();
		}

		counter_ptr addCounter(std::wstring name, listener_ptr listener) {
			counter_ptr counter = counter_ptr(new PDHCounter(name, listener));
			counters_.push_back(counter);
			return counter;
		}
		counter_ptr addCounter(std::wstring name) {
			counter_ptr counter = counter_ptr(new PDHCounter(name));
			counters_.push_back(counter);
			return counter;
		}
		void removeAllCounters() {
			if (hQuery_)
				close();
			counters_.clear();
		}

		virtual void on_unload() {
			if (hQuery_ == NULL)
				return;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->remove();
			}
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhCloseQuery(hQuery_);
			if (status.is_error())
				throw PDHException(_T("PdhCloseQuery failed"), status);
			hQuery_ = NULL;
		}
		virtual void on_reload() {
			if (hQuery_ != NULL)
				return;
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhOpenQuery( NULL, 0, &hQuery_ );
			if (status.is_error())
				throw PDHException(_T("PdhOpenQuery failed"), status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->addToQuery(getQueryHandle());
			}
		}

		void open() {
			if (hQuery_ != NULL)
				throw PDHException(_T("query is not null!"));
			PDH::PDHFactory::get_impl()->add_listener(this);
			on_reload();
		}

		void close() {
			if (hQuery_ == NULL)
				throw PDHException(_T("query is null!"));
			PDH::PDHFactory::get_impl()->remove_listener(this);
			on_unload();
			counters_.clear();
		}

		void gatherData() {
			collect();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				PDH::PDHError status = (*it)->collect();
				if (status.is_negative_denominator()) {
					Sleep(500);
					collect();
					status = (*it)->collect();
				}
				if (status.is_negative_denominator()) {
					if (!hasDisplayedInvalidCOunter_) {
						hasDisplayedInvalidCOunter_ = true;
						throw PDHException(_T("Negative denominator issue (check FAQ for ways to solve this): ") + (*it)->getName(), status);
					}
				} else if (status.is_error()) {
					throw PDHException(_T("Failed to poll counter: ") + (*it)->getName(), status);
				}
			}
		}
		inline void collect() {
			PDH::PDHError status = PDH::PDHFactory::get_impl()->PdhCollectQueryData(hQuery_);
			if (status.is_error())
				throw PDHException(_T("PdhCollectQueryData failed: "), status);
		}

		PDH::PDH_HQUERY getQueryHandle() const {
			return hQuery_;
		}
	};
}