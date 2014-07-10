#include <boost/bind.hpp>
#include <boost/assign.hpp>

#include <parsers/where.hpp>

#include "counter_filter.hpp"

using namespace boost::assign;
using namespace parsers::where;

counter_filter::filter_obj_handler::filter_obj_handler() {
	registry_.add_string()
		("counter", boost::bind(&filter_obj::get_counter, _1), "The name of the file")
		;

}
