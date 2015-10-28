#include "realtime_data.hpp"

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <strEx.h>

#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/macros.hpp>

void runtime_data::touch(boost::posix_time::ptime now) {
	BOOST_FOREACH(file_container &fc, files) {
		fc.size = boost::filesystem::file_size(fc.file);
	}
}

bool runtime_data::has_changed(transient_data_type) const {
	BOOST_FOREACH(const file_container &fc, files) {
		if (fc.size != boost::filesystem::file_size(fc.file))
			return true;
	}
	return false;
}

void runtime_data::add_file(const boost::filesystem::path &path) {
	try {
		file_container fc;
		fc.file = path;
		fc.size = boost::filesystem::file_size(fc.file);
		files.push_back(fc);
	} catch (std::exception &e) {
		NSC_LOG_ERROR("Failed to add " + path.string() + ": " + utf8::utf8_from_native(e.what()));
	}
}

bool runtime_data::process_item(filter_type &filter, transient_data_type) {
	bool matched = false;
	BOOST_FOREACH(file_container &c, files) {
		boost::uintmax_t sz = boost::filesystem::file_size(c.file);
		if (sz == c.size)
			continue;
		std::ifstream file(c.file.string().c_str());
		if (file.is_open()) {
			std::string line;
			if (sz > c.size)
				file.seekg(c.size);
			while (file.good()) {
				std::getline(file, line, '\n');
				if (!line.empty()) {
					std::list<std::string> chunks = strEx::s::splitEx(line, utf8::cvt<std::string>(column_split));
					boost::shared_ptr<logfile_filter::filter_obj> record(new logfile_filter::filter_obj(c.file.string(), line, chunks));
					modern_filter::match_result ret = filter.match(record);
					if (ret.matched_bound) {
						matched = true;
						if (ret.is_done) {
							break;
						}
					}
				}
			}
			file.close();
		} else {
			NSC_LOG_ERROR("Failed to open file: " + c.file.string());
		}
	}
	return matched;
}

void runtime_data::set_split(std::string line, std::string column) {
	if (column.empty())
		column_split = "\t";
	else
		column_split = column;
	strEx::replace(column_split, "\\t", "\t");
	strEx::replace(column_split, "\\n", "\n");
	if (line.empty())
		line = "\n";
	else
		line_split = line;
	strEx::replace(line_split, "\\t", "\t");
	strEx::replace(line_split, "\\n", "\n");
}