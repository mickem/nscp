#include <map>
#include <list>

#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/make_shared.hpp>

#include <parsers/where.hpp>

#include <simple_timer.hpp>
#include <strEx.h>
#include "filter.hpp"

using namespace boost::assign;
using namespace parsers::where;

//////////////////////////////////////////////////////////////////////////

parsers::where::node_type get_percentage(boost::shared_ptr<ping_filter::filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
	boost::tuple<long long, std::string> value = parsers::where::helpers::read_arguments(context, subject, "%");
	long long number = value.get<0>();
	std::string unit = value.get<1>();

	if (unit != "%")
		context->error("Invalid unit: " + unit);
	return parsers::where::factory::create_int(number);
}

ping_filter::filter_obj_handler::filter_obj_handler() {
	static const parsers::where::value_type type_custom_pct = parsers::where::type_custom_int_1;

	registry_.add_string()
		("host", &filter_obj::get_host, "The host name or ip address (as given on command line)")
		("ip", &filter_obj::get_ip, "The ip address name")
		// 		("version", boost::bind(&filter_obj::get_version, _1), "Windows exe/dll file version")
		// 		("filename", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		// 		("file", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		// 		("name", boost::bind(&filter_obj::get_filename, _1), "The name of the file")
		;
	registry_.add_int()
		("loss", type_custom_pct, &filter_obj::get_loss, "Packet loss")
		("time", type_int, &filter_obj::get_time, "Round trip time in ms")
		("sent", type_int, &filter_obj::get_sent, "Number of packets sent to the host")
		("recv", type_int, &filter_obj::get_recv, "Number of packets received from the host")
		("timeout", type_int, &filter_obj::get_timeout, "Number of packets which timed out from the host")
		;
	/*
		registry_.add_human_string()
			("access", boost::bind(&filter_obj::get_access_s, _1), "")
			("creation", boost::bind(&filter_obj::get_creation_s, _1), "")
			("written", boost::bind(&filter_obj::get_written_s, _1), "")
			;
			*/

	registry_.add_converter()
		(type_custom_pct, &get_percentage)
		;
}

void ping_filter::filter_obj::add(boost::shared_ptr<ping_filter::filter_obj> other) {
	if (!other)
		return;
	result.num_send_ += other->result.num_send_;
	result.num_replies_ += other->result.num_replies_;
	result.num_timeouts_ += other->result.num_timeouts_;
	result.time_ += other->result.time_;
}

//////////////////////////////////////////////////////////////////////////

boost::shared_ptr<ping_filter::filter_obj> ping_filter::filter_obj::get_total() {
	return boost::make_shared<ping_filter::filter_obj>();
}