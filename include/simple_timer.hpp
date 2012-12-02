#pragma once

#include <boost/date_time.hpp>

class simple_timer {
	boost::posix_time::ptime start_time;
	std::wstring text;
	bool log;
public:
	simple_timer() {
		start();
	}
	simple_timer(std::wstring text, bool log) : text(text), log(log) {
		start();
	}
	~simple_timer() {
		if (log)
			std::wcout << text << stop() << std::endl;;
	}

	void start() {
		start_time = get();
	}
	unsigned long long stop() {
		boost::posix_time::time_duration diff = get() - start_time;
		start();
		return diff.total_seconds();
	}

private:
	boost::posix_time::ptime get() {
		return boost::posix_time::microsec_clock::local_time();
	}

};