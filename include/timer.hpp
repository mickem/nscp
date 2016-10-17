#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>

#include <strEx.h>

namespace pt = boost::posix_time;

struct timer {
	boost::posix_time::ptime start_time;
	std::list<std::string> times;
	std::string current;

	timer() : start_time(pt::microsec_clock::local_time()) {}

	void check(std::string tag) {
		pt::ptime time = pt::microsec_clock::local_time();
		pt::time_duration diff = time - start_time;
		times.push_back(tag + ": " + strEx::s::xtos(diff.total_milliseconds()));
	}
	void start(const std::string tag) {
		check(">>> " + tag);
		current = tag;
	}
	void end() {
		check("<<< " + current);
		current = "";
	}

	std::list<std::string> get() {
		check("DONE");
		return times; 
	}
};
	

