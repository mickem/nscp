#include <parsers/helpers.hpp>

#include <time.h>

namespace parsers {
	namespace where {
		long long constants::now = 0;

		long long constants::get_now() {
			return now;
		}
		void constants::reset() {
			__time64_t ltime;
			_time64(&ltime);
			now = ltime;
		}


	}
}


