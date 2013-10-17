#pragma once
#include <list>

#include <boost/filesystem/path.hpp>


#include "filter.hpp"

struct runtime_data {

	typedef logfile_filter::filter filter_type;
	typedef int transient_data_type;

	struct file_container {
		boost::filesystem::path file;
		boost::uintmax_t size;
	};

	std::list<file_container> files;
	std::string column_split;
	std::string line_split;


	void boot() {}
	void touch(boost::posix_time::ptime now);
	bool has_changed(transient_data_type) const;
	bool process_item(filter_type &filter, transient_data_type);
	void set_split(std::string line, std::string column);

	void add_file(const boost::filesystem::path &path);
};
