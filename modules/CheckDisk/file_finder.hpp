#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include <file_helpers.hpp>

#include "file_info.hpp"
#include "filter.hpp"

namespace file_finder {

	struct scanner_context {
		bool debug;
		std::string pattern;
		DWORD now;
		int max_depth;
		bool is_valid_level(int current_level);
		void report_error(const std::string str);
		void report_debug(const std::string str);
		void report_warning(const std::string msg);
	};

	void recursive_scan(file_filter::filter &filter, scanner_context &context, boost::filesystem::path dir, bool recursive = false, int current_level = 0);
}