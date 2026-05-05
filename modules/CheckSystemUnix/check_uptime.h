#ifndef NSCP_CHECK_UPTIME_H
#define NSCP_CHECK_UPTIME_H
#include <boost/date_time/posix_time/ptime.hpp>
#include <check/uptime/filter_obj.hpp>
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <string>

namespace checks {
namespace check_uptime_filter {

typedef check_uptime_filter_common::filter_obj filter_obj;

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_uptime_filter

void check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response, const std::string &timezone);
}  // namespace checks

#endif  // NSCP_CHECK_UPTIME_H
