#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include <file_helpers.hpp>
#include <strEx.h>
#include <checkHelpers.hpp>
#include <filter_framework.hpp>

#include "file_info.hpp"
#include "filter.hpp"

namespace file_finder {
	void recursive_scan(file_filter::filter_result result, file_filter::filter_argument args, file_filter::filter_engine engine, boost::filesystem::path dir, bool recursive = false, int current_level = 0);
}