#include <parsers/helpers.hpp>

#include <time.h>

namespace parsers {
	namespace where {
		long long constants::now = 0;

		long long constants::get_now() {
			return now;
		}
		void constants::reset() {
			__time64_t utctime;
			_time64(&utctime);
			now = utctime;
// 			struct tm localtime;
// 			_localtime64_s(&localtime, &utctime);
// 			now = _mktime64(&localtime);
		}


	}
}


