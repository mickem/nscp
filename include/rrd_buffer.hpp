#pragma once

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

/**
 * @ingroup NSClientCompat
 * PDH collector thread (gathers performance data and allows for clients to retrieve it)
 *
 * @version 1.0
 * first version
 *
 * @date 02-13-2005
 *
 * @author mickem
 *
 * @par license
 * This code is absolutely free to use and modify. The code is provided "as is" with
 * no expressed or implied warranty. The author accepts no liability if it causes
 * any damage to your computer, causes your pet to fall ill, increases baldness
 * or makes your car start emitting strange noises when you start it up.
 * This code has no bugs, just undocumented features!
 *
 */
template<class T>
struct rrd_buffer {
	typedef T value_type;
	typedef boost::circular_buffer<T> list_type;
	list_type seconds;
	list_type minutes;
	list_type hours;
	int second_counter;
	int minute_counter;

public:
	rrd_buffer() : second_counter(0), minute_counter(0) {
		seconds.resize(60);
		minutes.resize(60);
		hours.resize(24);
	}
	value_type get_average(long time) const {
		value_type ret;
		if (time < 0)
			return ret;
		if (time <= seconds.size()) {
			for (list_type::const_iterator cit = seconds.end() - time; cit != seconds.end(); ++cit) {
				ret.add(*cit);
			}
			ret.normalize(time);
			return ret;
		}
		time /= 60;
		if (time <= minutes.size()) {
			for (list_type::const_iterator cit = minutes.end() - time; cit != minutes.end(); ++cit) {
				ret.add(*cit);
			}
			ret.normalize(time);
			return ret;
		}
		time /= 60;
		if (time >= hours.size())
			throw nscp_exception("Size larger than buffer");
		for (list_type::const_iterator cit = hours.end() - time; cit != hours.end(); ++cit) {
			ret.add(*cit);
		}
		ret.normalize(time);
		return ret;
	}
	value_type calculate_avg(list_type &buffer) const {
		value_type ret;
		BOOST_FOREACH(const value_type &entry, buffer) {
			ret.add(entry);
		}
		ret.normalize(buffer.size());
		return ret;
	}

	void push(const value_type &value) {
		seconds.push_back(value);
		if (second_counter++ >= 59) {
			second_counter = 0;
			windows::system_info::cpu_load avg = calculate_avg(seconds);
			minutes.push_back(avg);
			if (minute_counter++ >= 59) {
				minute_counter = 0;
				windows::system_info::cpu_load avg = calculate_avg(minutes);
				hours.push_back(avg);
			}
		}
	}
};
