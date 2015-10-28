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