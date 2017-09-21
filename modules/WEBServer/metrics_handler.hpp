#pragma once


#include <boost/thread.hpp>

#include <string>

struct metrics_handler {
	void set(const std::string &metrics);
	std::string get();
private:
	std::string metrics_;
	boost::timed_mutex mutex_;
};
