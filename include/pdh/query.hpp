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

namespace PDH {
	class PDHQuery : public PDH::PDHImplSubscriber {
	private:
		typedef std::list<PDHCounter*> CounterList;
		CounterList counters_;
		PDH::PDH_HQUERY hQuery_;
	public:
		PDHQuery() : hQuery_(NULL) {
		}
		virtual ~PDHQuery(void) {
			removeAllCounters();
		}

		PDHCounter* addCounter(std::wstring name, PDHCounterListener *listener) {
			PDHCounter *counter = new PDHCounter(name, listener);
			counters_.push_back(counter);
			return counter;
		}
		PDHCounter* addCounter(std::wstring name) {
			PDHCounter *counter = new PDHCounter(name);
			counters_.push_back(counter);
			return counter;
		}
		void removeAllCounters() {
			if (hQuery_)
				close();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
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
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void gatherData() {
			collect();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->collect();
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