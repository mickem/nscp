#pragma once
#include <list>

#include <boost/filesystem/path.hpp>

#include "filter.hpp"

struct runtime_data {
	typedef eventlog_filter::filter filter_type;
	typedef EventLogRecord& transient_data_type;

	std::list<std::string> files;

	void boot() {}
	void touch(boost::posix_time::ptime) {}
	bool has_changed(transient_data_type record) const;
	bool process_item(filter_type &filter, transient_data_type data);
	void add_file(const std::string &file);
};