#pragma once

#include <boost/shared_ptr.hpp>
#include <str/utils_no_boost.hpp>
#include <str/xtos.hpp>
#include <string>
#include <vector>

namespace parsers {
namespace perfdata {

struct builder {
  virtual ~builder() = default;
  virtual void add(std::string alias) = 0;
  virtual void set_value(double value) = 0;
  virtual void set_warning(double value) = 0;
  virtual void set_critical(double value) = 0;
  virtual void set_minimum(double value) = 0;
  virtual void set_maximum(double value) = 0;
  virtual void set_unit(const std::string &value) = 0;

  virtual void next() = 0;

  virtual void add_string(std::string alias, std::string value) = 0;
};

double trim_to_double(std::string s) {
  std::string::size_type pend = s.find_first_not_of("0123456789,.-");
  if (pend != std::string::npos) s = s.substr(0, pend);
  str::utils::replace(s, ",", ".");
  if (s.empty()) {
    return 0.0;
  }
  try {
    return str::stox<double>(s);
  } catch (...) {
    return 0.0;
  }
}

void parse(boost::shared_ptr<builder> builder, const std::string &perff) {
  std::string perf = perff;
  // TODO: make this work with const!

  const std::string perf_separator = " ";
  const std::string perf_lable_enclosure = "'";
  const std::string perf_equal_sign = "=";
  const std::string perf_item_splitter = ";";
  const std::string perf_valid_number = "0123456789,.-";

  while (true) {
    if (perf.size() == 0) return;
    std::string::size_type p = 0;
    p = perf.find_first_not_of(perf_separator, p);
    if (p != 0) perf = perf.substr(p);
    if (perf[0] == perf_lable_enclosure[0]) {
      p = perf.find(perf_lable_enclosure[0], 1) + 1;
      if (p == std::string::npos) return;
    }
    p = perf.find(perf_separator, p);
    if (p == 0) return;
    std::string chunk;
    if (p == std::string::npos) {
      chunk = perf;
      perf = std::string();
    } else {
      chunk = perf.substr(0, p);
      p = perf.find_first_not_of(perf_separator, p);
      if (p == std::string::npos)
        perf = std::string();
      else
        perf = perf.substr(p);
    }
    std::vector<std::string> items;
    str::utils::split(items, chunk, perf_item_splitter);
    if (items.size() < 1) {
      builder->add_string("invalid", "invalid performance data");
      builder->next();
      break;
    }

    std::pair<std::string, std::string> fitem = str::utils::split2(items[0], perf_equal_sign);
    std::string alias = fitem.first;
    if (alias.size() > 0 && alias[0] == perf_lable_enclosure[0] && alias[alias.size() - 1] == perf_lable_enclosure[0])
      alias = alias.substr(1, alias.size() - 2);

    if (alias.empty()) continue;
    builder->add(alias);

    std::string::size_type pstart = fitem.second.find_first_of(perf_valid_number);
    if (pstart == std::string::npos) {
      builder->set_value(0);
      builder->next();
      continue;
    }
    if (pstart != 0) fitem.second = fitem.second.substr(pstart);
    std::string::size_type pend = fitem.second.find_first_not_of(perf_valid_number);
    if (pend == std::string::npos) {
      builder->set_value(trim_to_double(fitem.second));
    } else {
      builder->set_value(trim_to_double(fitem.second.substr(0, pend)));
      builder->set_unit(fitem.second.substr(pend));
    }
    if (items.size() >= 2 && items[1].size() > 0) builder->set_warning(trim_to_double(items[1]));
    if (items.size() >= 3 && items[2].size() > 0) builder->set_critical(trim_to_double(items[2]));
    if (items.size() >= 4 && items[3].size() > 0) builder->set_minimum(trim_to_double(items[3]));
    if (items.size() >= 5 && items[4].size() > 0) builder->set_maximum(trim_to_double(items[4]));
    builder->next();
  }
}
}  // namespace perfdata
};  // namespace parsers