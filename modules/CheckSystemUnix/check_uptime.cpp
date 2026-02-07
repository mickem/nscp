#include "check_uptime.h"

#include <iosfwd>
#include <locale>

#define UPTIME_FILE "/proc/uptime"

#include <boost/assign.hpp>
#include <map>
#include <parsers/filter/cli_helper.hpp>
#include <simple_timer.hpp>

using namespace parsers::where;
namespace checks {
namespace check_uptime_filter {

parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(subject->get_string_value(context), "s"));
}

static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int()(
      "boot", parsers::where::type_date, [](auto obj, auto context) { return obj->get_boot(); }, "System boot time")(
      "uptime", type_custom_uptime, [](auto obj, auto context) { return obj->get_uptime(); }, "Time since last boot");
  registry_.add_converter()(type_custom_uptime, &parse_time);
  registry_.add_human_string("boot", &filter_obj::get_boot_s, "The system boot time")
      .add_human_string("uptime", &filter_obj::get_uptime_s, "Time sine last boot");
}
}  // namespace check_uptime_filter

}  // namespace checks

bool get_uptime(double &uptime_secs, double &idle_secs) {
  try {
    std::locale mylocale("C");
    std::ifstream uptime_file;
    uptime_file.imbue(mylocale);
    uptime_file.open(UPTIME_FILE);
    uptime_file >> uptime_secs >> idle_secs;
    uptime_file.close();
  } catch (const std::exception &e) {
    return false;
  }
  return true;
}

void checks::check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef check_uptime_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> times;

  filter_type filter;
  filter_helper.add_options("uptime < 2d", "uptime < 1d", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "uptime: ${uptime}h, boot: ${boot} (UTC)", "uptime", "", "");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  double uptime_secs = 0, idle_secs = 0;
  get_uptime(uptime_secs, idle_secs);
  unsigned long long value = uptime_secs;

  const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
  const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  const boost::posix_time::ptime boot = now - boost::posix_time::time_duration(0, 0, value);

  long long now_delta = (now - epoch).total_seconds();
  const auto uptime = static_cast<long long>(value);
  boost::shared_ptr<check_uptime_filter::filter_obj> record(new check_uptime_filter::filter_obj(uptime, now_delta, boot));
  filter.match(record);

  filter_helper.post_process(filter);
}
