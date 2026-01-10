#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <socket/allowed_hosts.hpp>
#include <str/format.hpp>
#include <string>
#include <utf8.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;

std::size_t extract_mask(const std::string &mask, std::size_t mask_length) {
  if (!mask.empty()) {
    const std::string::size_type start_pos_number = mask.find_first_of("0123456789");
    if (start_pos_number != std::string::npos) {
      const std::string::size_type end_pos_number = mask.find_first_not_of("0123456789", start_pos_number);
      if (end_pos_number != std::string::npos) {
        // TODO: This seems wrong should likely be end_pos_number-start_pos_number.
        mask_length = str::stox<std::size_t>(mask.substr(start_pos_number, end_pos_number));
      } else {
        mask_length = str::stox<std::size_t>(mask.substr(start_pos_number));
      }
    }
  }
  return static_cast<unsigned int>(mask_length);
}

template <class addr>
addr calculate_mask(const std::string &mask_as_string) {
  addr ret{};
  constexpr std::size_t byte_size = 8;
  constexpr std::size_t largest_byte = 0xff;
  const std::size_t mask = extract_mask(mask_as_string, byte_size * ret.size());
  const std::size_t index = mask / byte_size;
  const std::size_t reminder = mask % byte_size;

  const std::size_t value = largest_byte - (largest_byte >> reminder);

  for (std::size_t i = 0; i < ret.size(); i++) {
    if (i < index)
      ret[i] = largest_byte;
    else if (i == index)
      ret[i] = static_cast<unsigned char>(value);
    else
      ret[i] = 0;
  }
  return ret;
}

void socket_helpers::allowed_hosts_manager::refresh(std::list<std::string> &errors) {
  io_service io_service;
  tcp::resolver resolver(io_service);
  entries_v4.clear();
  entries_v6.clear();
  for (const std::string &record : sources) {
    std::string::size_type pos = record.find('/');
    std::string addr, mask;
    if (pos == std::string::npos) {
      addr = record;
      mask = "";
    } else {
      addr = record.substr(0, pos);
      mask = record.substr(pos);
    }
    if (addr.empty()) continue;

    if (std::isdigit(addr[0])) {
      address a = address::from_string(addr);
      if (a.is_v4()) {
        entries_v4.emplace_back(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask));
      } else if (a.is_v6()) {
        entries_v6.emplace_back(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask));
      } else {
        errors.push_back("Invalid address: " + record);
      }
    } else {
      try {
        tcp::resolver::query dns_query(addr, "");
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(dns_query);
        for (tcp::resolver::iterator end; endpoint_iterator != end; ++endpoint_iterator) {
          address a = endpoint_iterator->endpoint().address();
          if (a.is_v4()) {
            entries_v4.emplace_back(record, a.to_v4().to_bytes(), calculate_mask<addr_v4>(mask));
          } else if (a.is_v6()) {
            entries_v6.emplace_back(record, a.to_v6().to_bytes(), calculate_mask<addr_v6>(mask));
          } else {
            errors.emplace_back("Invalid address: " + record);
          }
        }
      } catch (const std::exception &e) {
        errors.emplace_back("Failed to parse host " + record + ": " + utf8::utf8_from_native(e.what()));
      }
    }
  }
}

void socket_helpers::allowed_hosts_manager::set_source(const std::string &source) {
  sources.clear();
  for (std::string s : str::utils::split_lst(source, std::string(","))) {
    boost::trim(s);
    if (!s.empty()) sources.push_back(s);
  }
}

std::string socket_helpers::allowed_hosts_manager::to_string() const {
  std::string ret;
  for (const host_record_v4 &r : entries_v4) {
    ip::address_v4 a(r.addr);
    ip::address_v4 m(r.mask);
    std::string s = a.to_string() + "(" + m.to_string() + ")";
    str::format::append_list(ret, s);
  }
  for (const host_record_v6 &r : entries_v6) {
    ip::address_v6 a(r.addr);
    ip::address_v6 m(r.mask);
    std::string s = a.to_string() + "(" + m.to_string() + ")";
    str::format::append_list(ret, s);
  }
  return ret;
}
