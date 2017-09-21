#pragma once

#include "error_handler_interface.hpp"

#include <boost/thread.hpp>

#include <string>
#include <vector>

struct error_handler : error_handler_interface {
	error_handler() : error_count_(0) {}
	void add_message(bool is_error, const log_entry &message);
	void reset();
	status get_status();
	log_list get_errors(std::size_t &position);
private:
	boost::timed_mutex mutex_;
	log_list log_entries;
	std::string last_error_;
	unsigned int error_count_;
};