/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <boost/make_shared.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <parsers/perfdata.hpp>
#include <str/xtos.hpp>

namespace nscapi {
namespace protobuf {

namespace {

long long get_multiplier(const std::string &unit) {
  if (unit.empty()) return 1;
  if (unit[0] == 'K') return 1024ll;
  if (unit[0] == 'M') return 1024ll * 1024ll;
  if (unit[0] == 'G') return 1024ll * 1024ll * 1024ll;
  if (unit[0] == 'T') return 1024ll * 1024ll * 1024ll * 1024ll;
  return 1;
}

struct perf_builder : parsers::perfdata::builder {
  PB::Commands::QueryResponseMessage::Response::Line *payload;
  PB::Common::PerformanceData *lastPerf = nullptr;

  perf_builder(PB::Commands::QueryResponseMessage::Response::Line *payload) : payload(payload) {}

  void add_string(std::string alias, std::string value) override {
    auto *perfData = payload->add_perf();
    perfData->set_alias(alias);
    perfData->mutable_string_value()->set_value(value);
  }

  void add(std::string alias) override {
    lastPerf = payload->add_perf();
    lastPerf->set_alias(alias);
  }
  void set_value(double value) override {
    if (lastPerf) lastPerf->mutable_float_value()->set_value(value);
  }
  void set_warning(double value) override {
    if (lastPerf) lastPerf->mutable_float_value()->mutable_warning()->set_value(value);
  }
  void set_critical(double value) override {
    if (lastPerf) lastPerf->mutable_float_value()->mutable_critical()->set_value(value);
  }
  void set_minimum(double value) override {
    if (lastPerf) lastPerf->mutable_float_value()->mutable_minimum()->set_value(value);
  }
  void set_maximum(double value) override {
    if (lastPerf) lastPerf->mutable_float_value()->mutable_maximum()->set_value(value);
  }
  void set_unit(const std::string &value) override {
    if (lastPerf) lastPerf->mutable_float_value()->set_unit(value);
  }
  void next() override {}
};

void parse_float_perf_value(std::stringstream &ss, const PB::Common::PerformanceData_FloatValue &val) {
  ss << str::xtos_non_sci(val.value());
  if (!val.unit().empty()) ss << val.unit();
  if (!val.has_warning() && !val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
    return;
  }
  ss << ";";
  if (val.has_warning()) ss << str::xtos_non_sci(val.warning().value());
  if (!val.has_critical() && !val.has_minimum() && !val.has_maximum()) {
    return;
  }
  ss << ";";
  if (val.has_critical()) ss << str::xtos_non_sci(val.critical().value());
  if (!val.has_minimum() && !val.has_maximum()) {
    return;
  }
  ss << ";";
  if (val.has_minimum()) ss << str::xtos_non_sci(val.minimum().value());
  if (!val.has_maximum()) {
    return;
  }
  ss << ";";
  if (val.has_maximum()) ss << str::xtos_non_sci(val.maximum().value());
}

}  // namespace

std::string functions::extract_perf_value_as_string(const PB::Common::PerformanceData &perf) {
  if (perf.has_float_value()) {
    const auto &val = perf.float_value();
    return str::xtos_non_sci(val.value() * static_cast<double>(get_multiplier(val.unit())));
  }
  if (perf.has_string_value()) {
    const auto &val = perf.string_value();
    return val.value();
  }
  return "unknown";
}

long long functions::extract_perf_value_as_int(const PB::Common::PerformanceData &perf) {
  if (perf.has_float_value()) {
    const auto &val = perf.float_value();
    if (!val.unit().empty()) return static_cast<long long>(val.value() * static_cast<double>(get_multiplier(val.unit())));
    return static_cast<long long>(val.value());
  }
  if (perf.has_string_value()) {
    return 0;
  }
  return 0;
}

std::string functions::extract_perf_maximum_as_string(const PB::Common::PerformanceData &perf) {
  if (perf.has_float_value()) {
    const auto &val = perf.float_value();
    if (!val.unit().empty()) return str::xtos_non_sci(val.maximum().value() * static_cast<double>(get_multiplier(val.unit())));
    return str::xtos_non_sci(val.maximum().value());
  }
  return "unknown";
}

void functions::parse_performance_data(PB::Commands::QueryResponseMessage::Response::Line *payload, const std::string &perf) {
  parsers::perfdata::parse(std::make_shared<perf_builder>(payload), perf);
}

std::string functions::build_performance_data(PB::Commands::QueryResponseMessage::Response::Line const &payload, const std::size_t max_length) {
  std::string ret;

  bool first = true;
  for (int i = 0; i < payload.perf_size(); i++) {
    std::stringstream ss;
    ss.precision(5);
    const auto &perfData = payload.perf(i);
    if (!first) ss << " ";
    first = false;
    ss << '\'' << perfData.alias() << "'=";
    if (perfData.has_float_value()) {
      parse_float_perf_value(ss, perfData.float_value());
    } else if (perfData.has_string_value()) {
      ss << perfData.string_value().value();
    }
    std::string tmp = ss.str();
    if (max_length == no_truncation || ret.length() + tmp.length() <= max_length) {
      ret += tmp;
    }
  }
  return ret;
}

}  // namespace protobuf
}  // namespace nscapi
