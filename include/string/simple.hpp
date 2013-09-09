#pragma once

#include <boost/lexical_cast.hpp>
#include <string>
#include <sstream>


namespace ss {


		template<class T>
		inline T stox(std::string s) {
			return boost::lexical_cast<T>(s.c_str());
		}

		template<typename T>
		inline std::string xtos(T i) {
			std::stringstream ss;
			ss << i;
			return ss.str();
		}
}