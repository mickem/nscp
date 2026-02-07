#ifndef NSCP_CHECK_UPTIME_H
#define NSCP_CHECK_UPTIME_H
#include <boost/date_time/posix_time/ptime.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace checks {
namespace check_uptime_filter {

struct filter_obj {
  long long uptime;
  long long now;
  boost::posix_time::ptime boot;

  filter_obj(long long uptime, long long now, boost::posix_time::ptime boot) : uptime(uptime), now(now), boot(boot) {}

  std::string show() const { return "uptime: " + str::format::itos_as_time(uptime * 1000) + " boot: " + str::format::format_date(boot); }

  long long get_uptime() const { return uptime; }
  long long get_boot() const { return now - uptime; }
  std::string get_boot_s() const { return str::format::format_date(boot); }
  std::string get_uptime_s() const { return str::format::itos_as_time(get_uptime() * 1000); }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_uptime_filter

void check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace checks

#endif  // NSCP_CHECK_UPTIME_H
