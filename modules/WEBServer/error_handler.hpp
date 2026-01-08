#pragma once

#include <boost/thread.hpp>
#include <string>
#include <vector>

#include "error_handler_interface.hpp"

struct error_handler : error_handler_interface {
  error_handler() : error_count_(0) {}
  void add_message(bool is_error, const log_entry &message);
  void reset();
  status get_status();
  log_list get_messages(std::list<std::string> levels, std::size_t position, std::size_t ipp, std::size_t &count) override;
  log_list get_messages_since(std::size_t since, std::size_t position, std::size_t ipp, std::size_t &count) override;

 private:
  boost::timed_mutex mutex_;
  log_list log_entries;
  std::string last_error_;
  unsigned int error_count_;
};