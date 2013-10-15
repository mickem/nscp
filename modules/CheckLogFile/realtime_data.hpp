#pragma once
#include <list>

#include "filter.hpp"

struct runtime_data {

	typedef logfile_filter::filter filter_type;

	struct file_container {
		std::string file;
		boost::uintmax_t size;
	};

	std::list<file_container> files;
	std::string column_split;
	std::string line_split;


	void boot() {}
	void touch(boost::posix_time::ptime now);
	bool has_changed() const;
	void set_files(std::string file_string);
	void set_file(std::string file_string);
	bool process_item(filter_type filter);
	void set_split(std::string line, std::string column);
};
