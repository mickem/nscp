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
#include <sstream>

#include <boost/shared_ptr.hpp>

#include <pdh.h>
#include <pdhmsg.h>

#include <error.hpp>

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