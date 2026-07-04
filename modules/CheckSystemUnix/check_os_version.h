#ifndef NSCP_CHECK_OS_VERSION_H
#define NSCP_CHECK_OS_VERSION_H
#include <nscapi/protobuf/command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace os_version {

// Distribution identity parsed from /etc/os-release.
struct os_release_info {
  std::string distribution;       // ID, e.g. "ubuntu"
  std::string distribution_name;  // NAME, e.g. "Ubuntu"
  std::string version;            // VERSION_ID, e.g. "22.04"
  std::string family;             // first token of ID_LIKE, else ID, e.g. "debian"
  std::string pretty;             // PRETTY_NAME, e.g. "Ubuntu 22.04.3 LTS"
};

// Parse the shell-style key=value contents of /etc/os-release. Strips optional
// surrounding single/double quotes from values.
os_release_info parse_os_release(const std::string &content);

// Read and parse /etc/os-release from an explicit path (defaults to the real
// location). Returns an empty struct if the file is missing.
os_release_info read_os_release_from(const std::string &path);

namespace os_version_filter {

struct filter_obj {
  std::string kernel_name;
  std::string nodename;
  std::string kernel_version;
  std::string kernel_release;
  std::string machine;
  std::string processor;
  std::string os;
  std::string distribution;
  std::string distribution_name;
  std::string version;
  std::string family;

  filter_obj() {}

  std::string show() const {
    return "os: " + os + " kernel: " + kernel_name + " release: " + kernel_release + " machine: " + machine;
  }

  std::string get_kernel_name() const { return kernel_name; }
  std::string get_nodename() const { return nodename; }
  std::string get_kernel_version() const { return kernel_version; }
  std::string get_kernel_release() const { return kernel_release; }
  std::string get_machine() const { return machine; }
  std::string get_processor() const { return processor; }
  std::string get_os() const { return os; }
  std::string get_distribution() const { return distribution; }
  std::string get_distribution_name() const { return distribution_name; }
  std::string get_version() const { return version; }
  std::string get_family() const { return family; }
};
typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;

struct filter_obj_handler : public native_context {
  filter_obj_handler();
};

typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace os_version_filter

void check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}  // namespace os_version

#endif  // NSCP_CHECK_OS_VERSION_H
