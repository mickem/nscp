#ifndef NSCP_CHECK_PAGEFILE_H
#define NSCP_CHECK_PAGEFILE_H
#include <boost/shared_ptr.hpp>
#include <nscapi/nscapi_protobuf_command.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <str/format.hpp>

class pdh_thread;

namespace check_page {

namespace check_page_filter {

struct filter_obj {
  std::string name;
  unsigned long long total;
  unsigned long long used;

  filter_obj(std::string name, unsigned long long total, unsigned long long used) : name(name), total(total), used(used) {}
  filter_obj(const filter_obj &other) : name(other.name), total(other.total), used(other.used) {}

  std::string show() const { return name; }

  long long get_total() const { return total; }
  long long get_used() const { return used; }
  long long get_free() const { return total > used ? total - used : 0; }
  long long get_used_pct() const { return total == 0 ? 0 : get_used() * 100 / total; }
  long long get_free_pct() const { return total == 0 ? 0 : get_free() * 100 / total; }
  std::string get_name() const { return name; }

  std::string get_total_human() const { return str::format::format_byte_units(get_total()); }
  std::string get_used_human() const { return str::format::format_byte_units(get_used()); }
  std::string get_free_human() const { return str::format::format_byte_units(get_free()); }
};

typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : public native_context {
  filter_obj_handler();
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;

}  // namespace check_page_filter

void check_pagefile(boost::shared_ptr<pdh_thread> collector, const PB::Commands::QueryRequestMessage::Request &request,
                    PB::Commands::QueryResponseMessage::Response *response);

}  // namespace check_page

#endif  // NSCP_CHECK_PAGEFILE_H
