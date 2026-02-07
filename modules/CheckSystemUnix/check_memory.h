#ifndef NSCP_CHECK_MEMORY_H
#define NSCP_CHECK_MEMORY_H
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

class pdh_thread;

namespace check_memory {

namespace check_mem_filter {

struct filter_obj {
  std::string type;
  unsigned long long free;
  unsigned long long total;

  filter_obj(std::string type, unsigned long long free, unsigned long long total) : type(type), free(free), total(total) {}
  filter_obj(const filter_obj &other) : type(other.type), free(other.free), total(other.total) {}

  std::string show() const {
    return "type: " + type + " total: " + str::format::format_byte_units(total) + " free: " + str::format::format_byte_units(free) +
           " used: " + str::format::format_byte_units(total - free);
  }

  long long get_total() const { return total; }
  long long get_used() const { return total - free; }
  long long get_free() const { return free; }
  std::string get_type() const { return type; }

  std::string get_total_human() const { return str::format::format_byte_units(get_total()); }
  std::string get_used_human() const { return str::format::format_byte_units(get_used()); }
  std::string get_free_human() const { return str::format::format_byte_units(get_free()); }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace check_mem_filter

void check_memory(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                  PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_memory

#endif  // NSCP_CHECK_MEMORY_H
