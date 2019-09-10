#pragma once


#include <boost/thread/mutex.hpp>

#include <list>
#include <string>

struct metrics_handler {
	void set(const std::string &metrics);
	void set_list(const std::string &metrics);
	void set_openmetrics(std::list<std::string>& metrics);
	std::list<std::string> get_openmetrics();
	std::string get();
	std::string get_list();
private:
	std::string metrics_;
	std::string metrics_list_;
	std::list<std::string> open_metrics_;
	boost::timed_mutex mutex_;
};
