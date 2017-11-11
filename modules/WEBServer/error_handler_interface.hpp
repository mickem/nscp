#pragma once

#include <string>
#include <vector>
#include <list>

struct error_handler_interface {
 
	struct log_entry {
		int line;
		std::string type;
		std::string file;
		std::string message;
		std::string date;
	};

	struct status {
		status() : error_count(0) {}
		std::string last_error;
		unsigned int error_count;
	};
  
	typedef std::vector<log_entry> log_list;
  
	virtual void add_message(bool is_error, const log_entry &message) = 0;
	virtual void reset() = 0;
	virtual log_list get_messages(std::list<std::string> levels, std::size_t &position, std::size_t &ipp, std::size_t &count) = 0;
	virtual status get_status() = 0;

};