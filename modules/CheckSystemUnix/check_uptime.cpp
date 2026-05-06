#include "check_uptime.h"

#include <iosfwd>
#include <locale>

#define UPTIME_FILE "/proc/uptime"

#include <boost/assign.hpp>
#include <list>
#include <map>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscp_time.hpp>
#include <parsers/filter/cli_helper.hpp>
#include <simple_timer.hpp>
#include <str/xtos.hpp>

using namespace parsers::where;
namespace checks {
namespace check_uptime_filter {

parsers::where::node_type parse_time(boost::shared_ptr<filter_obj> object, parsers::where::evaluation_context context, parsers::where::node_type subject) {
  // The where-parser may hand us either a single string literal ("30m") or a
  // two-element list [number, unit] for tokenized inputs like "2d". For the
  // list form, list_node::get_value joins with ", " and produces "2, d",
  // which fails validate_time_spec under #589. Reassemble the list manually
  // (no separator) so both forms round-trip cleanly through stox_as_time_sec
  // (issue #452 + #589 follow-up).
  std::list<parsers::where::node_type> tokens = subject->get_list_value(context);
  std::string expr;
  if (tokens.size() == 2) {
    auto cit = tokens.begin();
    const long long n = (*cit)->get_int_value(context);
    ++cit;
    const std::string unit = (*cit)->get_value(context, parsers::where::type_string).get_string("");
    expr = str::xtos(n) + unit;
  } else {
    expr = subject->get_string_value(context);
  }
  return parsers::where::factory::create_int(str::format::stox_as_time_sec<long long>(expr, "s"));
}

static const parsers::where::value_type type_custom_uptime = parsers::where::type_custom_int_1;
filter_obj_handler::filter_obj_handler() {
  registry_.add_int()(
      "boot", parsers::where::type_date, [](auto obj, auto context) { return obj->get_boot(); }, "System boot time")(
      "uptime", type_custom_uptime, [](auto obj, auto context) { return obj->get_uptime(); }, "Time since last boot");
  registry_.add_converter()(type_custom_uptime, &parse_time);
  registry_.add_human_string("boot", &filter_obj::get_boot_s, "The system boot time")
      .add_human_string("uptime", &filter_obj::get_uptime_s, "Time since last boot (granularity controlled by --max-unit)")
      .add_human_string("tz", &filter_obj::get_tz, "The timezone label used to render boot time");
}
}  // namespace check_uptime_filter

}  // namespace checks

bool get_uptime(double &uptime_secs, double &idle_secs) {
  try {
    std::locale mylocale("C");
    std::ifstream uptime_file;
    uptime_file.imbue(mylocale);
    uptime_file.open(UPTIME_FILE);
    if (!uptime_file.is_open()) return false;
    uptime_file >> uptime_secs >> idle_secs;
    uptime_file.close();
  } catch (const std::exception &e) {
    return false;
  }
  return true;
}

void checks::check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                          const std::string &timezone) {
  typedef check_uptime_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::vector<std::string> times;

  filter_type filter;
  std::string max_unit_str = "w";
  filter_helper.add_options("uptime < 2d", "uptime < 1d", "", filter.get_filter_syntax(), "ignored");
  // The timezone label is rendered via the ${tz} placeholder so the configured
  // value (default "local") is reflected in the output (issue #365).
  filter_helper.add_syntax("${status}: ${list}", "uptime: ${uptime}h, boot: ${boot} (${tz})", "uptime", "", "");
  filter_helper.get_desc().add_options()(
      "max-unit", boost::program_options::value<std::string>(&max_unit_str)->default_value("w"),
      "Largest time unit used to render ${uptime}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'.");

  if (!filter_helper.parse_options()) return;

  str::format::itos_as_time_unit max_unit;
  try {
    max_unit = str::format::parse_itos_as_time_unit(max_unit_str);
  } catch (const std::invalid_argument &e) {
    nscapi::protobuf::functions::set_response_bad(*response, e.what());
    return;
  }

  if (!filter_helper.build_filter(filter)) return;

  double uptime_secs = 0, idle_secs = 0;
  // Honor read failure so we don't silently report an uptime of 0.
  if (!get_uptime(uptime_secs, idle_secs)) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read " UPTIME_FILE);
    return;
  }
  unsigned long long value = uptime_secs;

  // `now` is rendered in the configured zone; `boot` is derived as
  // `now - uptime` so it lands in the same wall-clock as `now`.
  const boost::posix_time::ptime now = nscp_time::now(timezone);
  const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  const boost::posix_time::ptime boot = now - boost::posix_time::time_duration(0, 0, value);

  long long now_delta = (boost::posix_time::second_clock::universal_time() - epoch).total_seconds();
  const auto uptime = static_cast<long long>(value);
  boost::shared_ptr<check_uptime_filter::filter_obj> record(new check_uptime_filter::filter_obj(uptime, now_delta, boot, timezone, max_unit));
  filter.match(record);

  filter_helper.post_process(filter);
}
