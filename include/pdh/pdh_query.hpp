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
#include <sstream>

#include <boost/shared_ptr.hpp>

#include <pdh.h>
#include <pdhmsg.h>

#include <error/error.hpp>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_resolver.hpp>
#include <pdh/pdh_counters.hpp>

namespace PDH {
	class PDHQuery : public PDH::subscriber {
	public:
		typedef boost::shared_ptr<PDHCounter> counter_type;
		typedef std::list<counter_type> counter_list_type;
		counter_list_type counters_;
		PDH::PDH_HQUERY hQuery_;
		bool hasDisplayedInvalidCOunter_;
	public:
		PDHQuery() : hQuery_(NULL), hasDisplayedInvalidCOunter_(false) {}
		virtual ~PDHQuery(void);

		void addCounter(pdh_instance counter);
		void removeAllCounters();

		bool has_counters();

		virtual void on_unload();
		virtual void on_reload();

		void open();
		void close();

		void gatherData(bool ignore_errors = false);
		inline void collect();

		PDH::PDH_HQUERY getQueryHandle() const;
	};
}