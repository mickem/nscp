#include "realtime_data.hpp"

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

bool runtime_data::has_changed(transient_data_type record) const {
	if (files.empty())
		return true;
	BOOST_FOREACH(const std::string &s, files) {
		if (s == "any" || s == "all" || s == record.get_log())
			return true;
	}
	return false;
}

void runtime_data::add_file(const std::string &file) {
	files.push_back(file);
}

bool runtime_data::process_item(filter_type &filter, transient_data_type record) {
	filter.match(filter_type::object_type(new eventlog_filter::old_filter_obj(record, 0)));
	return filter.summary.get_count_match() > 0;
}
