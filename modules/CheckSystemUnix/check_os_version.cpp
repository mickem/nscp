#include "check_os_version.h"

#include <sys/utsname.h>

#include <parsers/filter/cli_helper.hpp>

namespace os_version {

namespace os_version_filter {
filter_obj_handler::filter_obj_handler() {
  registry_.add_string("kernel_name", &filter_obj::get_kernel_name, "Kernel name")
      .add_string("nodename", &filter_obj::get_nodename, "Network node hostname")
      .add_string("kernel_release", &filter_obj::get_kernel_release, "Kernel release")
      .add_string("kernel_version", &filter_obj::get_kernel_version, "Kernel version")
      .add_string("machine", &filter_obj::get_machine, "Machine hardware name")
      .add_string("processor", &filter_obj::get_processor, "Processor type or unknown")
      .add_string("os", &filter_obj::get_processor, "Operating system");
}
}  // namespace os_version_filter
}  // namespace os_version

void os_version::check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef os_version_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);

  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${status}: ${list}", "${kernel_name} (${kernel_release})", "kernel_release", "", "");

  if (!filter_helper.parse_options()) return;

  if (!filter_helper.build_filter(filter)) return;

  utsname name{};
  if (uname(&name) == -1) return nscapi::protobuf::functions::set_response_bad(*response, "Cannot get system name");

  boost::shared_ptr<os_version_filter::filter_obj> record(new os_version_filter::filter_obj());
  record->kernel_name = name.sysname;
  record->nodename = name.nodename;
  record->kernel_version = name.version;
  record->kernel_release = name.release;
  record->machine = name.machine;

  filter.match(record);

  filter_helper.post_process(filter);
}
