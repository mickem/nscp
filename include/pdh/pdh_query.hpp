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