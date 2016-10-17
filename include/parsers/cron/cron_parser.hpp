#pragma once

#include <strEx.h>
#include <error.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>


namespace cron_parser {
	struct next_value {
		int value;
		bool overflow;
		next_value(int value, bool overflow) : value(value), overflow(overflow) {}
		next_value(const next_value &other) : value(other.value), overflow(other.overflow) {}
		next_value& operator= (const next_value &other) {
			value = other.value;
			overflow = other.overflow;
			return *this;
		}
	};
	struct schedule_item {
		int value_;
		int min_;
		int max_;
		bool star_;
		schedule_item() : value_(0), min_(0), max_(0), star_(false) {}
		schedule_item(const schedule_item &other) : value_(other.value_), min_(other.min_), max_(other.max_), star_(other.star_) {}
		schedule_item& operator= (const schedule_item &other) {
			value_ = other.value_;
			min_ = other.min_;
			max_ = other.max_;
			star_ = other.star_;
			return *this;
		}

		static schedule_item parse(std::string value, int min_value, int max_value) {
			schedule_item v;
			v.min_ = min_value;
			v.max_ = max_value;
			if (value == "*") {
				v.star_ = true;
				return v;
			}
			try {
				v.value_ = boost::lexical_cast<int>(value.c_str());
				if (v.value_ < v.min_ || v.value_ > v.max_)
					throw nscp_exception("Invalid value: " + value);
				return v;
			} catch (...) {
				throw nscp_exception("Invalid value: " + value);
			}
			throw nscp_exception("Invalid value: " + value);
		}
		bool is_valid_for(int v) const {
			if (star_)
				return true;
			if (value_ == v)
				return true;
			return false;
		}

		next_value find_next(int value) const {
			for (int i = value; i <= max_; i++) {
				if (is_valid_for(i)) {
					return next_value(i, false);
				}
			}
			for (int i = min_; i < value; i++) {
				if (is_valid_for(i)) {
					return next_value(i, true);
				}
			}
			throw nscp_exception("Failed to find match for: " + value);
		}

		std::string to_string() const {
			if (star_)
				return "*";
			return strEx::s::xtos(value_);
		}


	};
	struct schedule {
		schedule_item min;
		schedule_item hour;
		schedule_item dom;
		schedule_item mon;
		schedule_item dow;

		bool is_valid_for(boost::posix_time::ptime now_time) const {
			return mon.is_valid_for(now_time.date().month()) &&
				dom.is_valid_for(now_time.date().day()) &&
				dow.is_valid_for(now_time.date().day_of_week()) &&
				hour.is_valid_for(now_time.time_of_day().hours()) &&
				min.is_valid_for(now_time.time_of_day().minutes());
		}

		boost::posix_time::ptime find_next(boost::posix_time::ptime now_time) const {
			using namespace boost::posix_time;
			using namespace boost::gregorian;
			// TODO: dow


			// Never run "Now" always wait for next...
			if (is_valid_for(now_time))
				now_time += minutes(1);

			next_value nmin = min.find_next(now_time.time_of_day().minutes());
			next_value nhour = hour.find_next(now_time.time_of_day().hours());
			next_value ndom = dom.find_next(now_time.date().day());
			next_value nmon = mon.find_next(now_time.date().month());
			next_value ndow = dow.find_next(now_time.date().day_of_week());

			time_duration t(hours(nhour.value) + minutes(nmin.value));
			if (nmin.overflow)
				t += hours(1);
			date d(now_time.date().year(), nmon.value, ndom.value);
			if (nhour.overflow)
				d += days(1);
			if (ndom.overflow) {
				d += months(1);
				t = time_duration(0, 0, 0);
			}
			if (ndow.overflow) {
				d += weeks(1);
				t = time_duration(0, 0, 0);
			}
			if (nmon.overflow) {
				d = date(now_time.date().year()+1, nmon.value, 1);
				t = time_duration(0, 0, 0);
			}


			return ptime(d, t);
		}

		std::string to_string() const {
			return min.to_string() + " " + hour.to_string() + " " + dom.to_string() + " " + mon.to_string() + " " + dow.to_string();
		}
	};

	inline schedule parse(std::string s) {
		// min hour dom mon dow
		// min: 0-59, hour: 0-23, dom: 1-31, mon: 1-12, dow: 0-6
		std::vector<std::string> v = strEx::s::split<std::vector<std::string> >(s, " ");
		schedule ret;
		if (v.size() != 5)
			throw nscp_exception("invalid cron syntax: " + s);
		ret.min = schedule_item::parse(v[0], 0, 59);
		ret.hour = schedule_item::parse(v[1], 0, 23);
		ret.dom = schedule_item::parse(v[2], 1, 31);
		ret.mon = schedule_item::parse(v[3], 1, 12);
		ret.dow = schedule_item::parse(v[4], 0, 6);
		return ret;
	}
}

