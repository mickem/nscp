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

#include <parsers/helpers.hpp>

#include <boost/date_time.hpp>

namespace parsers {
	namespace where {
		long long constants::now = 0;

		long long constants::get_now() {
			return now;
		}
		namespace pt = boost::posix_time;
		namespace gt = boost::gregorian;
		namespace dt = boost::date_time;

		inline std::time_t to_time_t_epoch(pt::ptime t) {
			if (t == dt::neg_infin)
				return 0;
			else if (t == dt::pos_infin)
				return LONG_MAX;
			pt::ptime start(gt::date(1970, 1, 1));
			return (t - start).total_seconds();
		}

		void constants::reset() {
			now = to_time_t_epoch(pt::second_clock::universal_time());
		}
	}
}