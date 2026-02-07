#ifndef NSCP_CHECK_OS_VERSION_H
#define NSCP_CHECK_OS_VERSION_H
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace os_version {

namespace os_version_filter {

struct filter_obj {
  std::string kernel_name;
  std::string nodename;
  std::string kernel_version;
  std::string kernel_release;
  std::string machine;
  std::string processor;
  std::string os;

  filter_obj() {}

  std::string show() const {
    return "kernel: " + kernel_name + " nodename: " + nodename + " version: " + kernel_version + " release: " + kernel_release + " machine: " + machine +
           " processor: " + processor + " os: " + os;
  }

  std::string get_kernel_name() const { return kernel_name; }
  std::string get_nodename() const { return nodename; }
  std::string get_kernel_version() const { return kernel_version; }
  std::string get_kernel_release() const { return kernel_release; }
  std::string get_machine() const { return machine; }
  std::string get_processor() const { return processor; }
  std::string get_os() const { return os; }
};
typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace os_version_filter

void check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace os_version

#endif  // NSCP_CHECK_OS_VERSION_H
