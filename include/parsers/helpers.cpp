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