#pragma once
/*
#include <file_helpers.hpp>
#include <checkHelpers.hpp>
#include <filter_framework.hpp>


namespace file_filter {
	struct filter_obj;
}
namespace file_finder {

	typedef checkHolders::CheckContainer<checkHolders::MaxMinBoundsDiscSize> PathContainer;



	struct filter {

		static const int filter_plus = 1;
		static const int filter_minus = 2;
		static const int filter_normal = 3;


		filters::filter_all_numeric<unsigned long long, checkHolders::disk_size_handler<checkHolders::disk_size_type> > size;
		filters::filter_all_times creation;
		filters::filter_all_times accessed;
		filters::filter_all_times written;
		filters::filter_all_strings version;
		filters::filter_all_num_ul line_count;

		inline bool hasFilter() {
			return size.hasFilter() || creation.hasFilter() || 
				accessed.hasFilter() || written.hasFilter();
		}
		bool matchFilter(file_filter::filter_obj *value) const;

		std::wstring getValue() const {
			if (size.hasFilter())
				return _T("size: ") + size.getValue();
			if (creation.hasFilter())
				return _T("creation: ") + creation.getValue();
			if (accessed.hasFilter())
				return _T("accessed: ") + accessed.getValue();
			if (written.hasFilter())
				return _T("written: ") + written.getValue();
			if (version.hasFilter())
				return _T("written: ") + version.getValue();
			if (line_count.hasFilter())
				return _T("written: ") + line_count.getValue();
			return _T("UNknown...");
		}

	};

}
*/