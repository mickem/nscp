#pragma once


#include <boost/thread.hpp>

#include <string>

struct metrics_handler {
	void set(const std::string &metrics);
	void set_list(const std::string &metrics);
	std::string get();
	std::string get_list();
private:
	std::string metrics_;
	std::string metrics_list_;
	boost::timed_mutex mutex_;
};
