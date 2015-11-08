#pragma once

#include <check_mk/data.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/noncopyable.hpp>

namespace check_mk {
	namespace server {
		class parser : public boost::noncopyable {
			std::vector<char> buffer_;
		public:
		};
	}// namespace server
} // namespace nscp