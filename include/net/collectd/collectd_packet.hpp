/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#include <boost/endian/conversion.hpp>
#include <boost/optional.hpp>
#include <cstring>
#include <list>
#include <map>
#include <sstream>
#include <str/utils.hpp>
#include <str/xtos.hpp>

namespace collectd {
// collectd "part" type codes (see https://collectd.org/wiki/index.php/Binary_protocol).
enum part_type : int16_t {
  part_host = 0x0000,
  part_time_hr = 0x0008,
  part_plugin = 0x0002,
  part_plugin_instance = 0x0003,
  part_type = 0x0004,
  part_type_instance = 0x0005,
  part_values = 0x0006,
  part_interval_hr = 0x0009,
};

// Value type codes inside a values part.
enum value_type : uint8_t {
  value_gauge = 0x01,
  value_derive = 0x02,
};

// The collectd network plugin reads at most this many bytes per datagram, so
// we must not emit a packet larger than this (see Network plugin
// `MaxPacketSize`, default 1452).
static const std::size_t max_packet_size = 1452;

// Longest string a single part may carry: it must fit in one datagram alongside
// the 4-byte part header and the trailing NUL.
static const std::size_t max_string_length = max_packet_size - 5;

class collectd_exception : public std::exception {
  std::string msg_;

 public:
  collectd_exception() {}
  ~collectd_exception() throw() {}
  collectd_exception(std::string msg) : msg_(msg) {}
  const char *what() const throw() { return msg_.c_str(); }
};

class packet {
 public:
  std::string buffer;

 public:
  packet() {}
  packet(const packet &other) : buffer(other.buffer) {}
  packet operator=(const packet &other) {
    buffer = other.buffer;
    return *this;
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "TODO";
    return ss.str();
  }

  void add_host(const std::string &value) { append_string(part_host, value); }
  void add_plugin(const std::string &value) { append_string(part_plugin, value); }
  void add_plugin_instance(const std::string &value) { append_string(part_plugin_instance, value); }
  void add_type(const std::string &value) { append_string(part_type, value); }
  void add_type_instance(const std::string &value) { append_string(part_type_instance, value); }
  void add_gauge_value(const std::list<double> &values) { append_values(value_gauge, values); }
  void add_derive_value(const std::list<long long> &values) { append_values(value_derive, values); }
  void add_time_hr(unsigned long long time) { append_int(part_time_hr, time); }
  void add_interval_hr(unsigned long long time) { append_int(part_interval_hr, time); }

  // A packet is "full" once adding another value-list could overflow the
  // network buffer. Leave headroom for one more value-list (a handful of
  // string parts plus the values) so render() never emits an oversized packet.
  bool is_full() const { return buffer.size() > max_packet_size - 128; }

  // All multi-byte integers in the collectd protocol are big-endian. Build the
  // big-endian value in a properly aligned local and append its bytes, rather
  // than reinterpret_cast-ing into the std::string buffer (which is unaligned
  // and aliasing UB, and trips the sanitizers).
  template <class T>
  static void append_be(std::string &buf, T value) {
    const T be = boost::endian::native_to_big(value);
    buf.append(reinterpret_cast<const char *>(&be), sizeof(be));
  }

  // A string part: type(2) + length(2) + NUL-terminated string. The length
  // field covers the 4-byte header and the trailing NUL.
  //
  // The length field is an (unsigned) 16-bit value and the whole part must fit
  // inside one datagram, so clamp pathologically long strings — e.g. a
  // misconfigured hostname/plugin/type — instead of letting the cast to int16_t
  // silently wrap and emit a malformed part.
  void append_string(int16_t type, const std::string &string_data) {
    const std::size_t data_len = string_data.size() > max_string_length ? max_string_length : string_data.size();
    const int16_t len = static_cast<int16_t>(data_len + 5);
    append_be<int16_t>(buffer, type);
    append_be<int16_t>(buffer, len);
    buffer.append(string_data, 0, data_len);
    buffer.push_back('\0');
  }
  // A number part: type(2) + length(2, always 12) + uint64(8).
  void append_int(int16_t type, unsigned long long int_data) {
    append_be<int16_t>(buffer, type);
    append_be<int16_t>(buffer, static_cast<int16_t>(12));
    append_be<uint64_t>(buffer, static_cast<uint64_t>(int_data));
  }
  // A values part: type(2) + length(2) + count(2) + count value-type bytes +
  // count 8-byte values. Gauge values are little-endian IEEE-754 doubles;
  // derive values are big-endian int64. Build the body first, then prefix the
  // header with the now-known length.
  template <class T>
  void append_values(uint8_t value_type, const std::list<T> &value_data) {
    std::string body;
    body.reserve(value_data.size() * 9);
    for (std::size_t i = 0; i < value_data.size(); i++) {
      body.push_back(static_cast<char>(value_type));
    }
    for (const T &v : value_data) {
      append_value(body, v);
    }
    const int16_t len = static_cast<int16_t>(6 + body.size());
    append_be<int16_t>(buffer, part_values);
    append_be<int16_t>(buffer, len);
    append_be<int16_t>(buffer, static_cast<int16_t>(value_data.size()));
    buffer.append(body);
  }

  // Gauge: little-endian double (collectd stores gauges in x86 byte order).
  static void append_value(std::string &buf, double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint64_t le = boost::endian::native_to_little(bits);
    buf.append(reinterpret_cast<const char *>(&le), sizeof(le));
  }
  // Derive/counter: big-endian int64.
  static void append_value(std::string &buf, long long value) { append_be<int64_t>(buf, static_cast<int64_t>(value)); }

  std::string get_buffer() const { return buffer; }

  std::size_t get_size() const { return buffer.size(); }
};

struct collectd_builder {
  struct metric_container {
    long long time_hr;
    long long interval_hr;
    std::string plugin_name;
    boost::optional<std::string> plugin_instance;
    std::string type_name;
    boost::optional<std::string> type_instance;
    std::list<double> gauges;
    std::list<long long> derives;

    metric_container(long long time_hr, long long interval_hr) : time_hr(time_hr), interval_hr(interval_hr) {}

    void set_type(const std::string &type_name_, const std::string &type_instance_) {
      type_name = type_name_;
      type_instance = type_instance_;
    }
    void set_type(const std::string &type_name_) { type_name = type_name_; }
    void set_plugin(const std::string &plugin_name_, const boost::optional<std::string> &plugin_instance_) {
      plugin_name = plugin_name_;
      plugin_instance = plugin_instance_;
    }
    void set_plugin(const std::string &plugin_name_) { plugin_name = plugin_name_; }
    std::string to_string() const {
      std::stringstream ss;
      ss << plugin_name << "-";
      if (plugin_instance) ss << *plugin_instance;
      ss << "/" << type_name << "-";
      if (type_instance) ss << *type_instance;
      ss << "=";
      if (!gauges.empty()) {
        ss << " gagues: ";
        for (const double &d : gauges) {
          ss << d << ", ";
        }
      }
      if (!derives.empty()) {
        ss << " derives: ";
        for (const long long &d : derives) {
          ss << d << ", ";
        }
      }
      return ss.str();
    }
  };

  typedef std::map<std::string, std::string> metrics_map;
  typedef std::multimap<std::string, std::string> variables_map;
  typedef std::list<metric_container> metrics_list;
  typedef std::list<collectd::packet> packet_list;

  variables_map variables;
  metrics_map metrics;
  metrics_list rendererd_metrics;
  unsigned long long time_hr;
  unsigned long long interval_hr;
  std::string host;

  struct expanded_keys {
    std::string key;
    std::string value;
    expanded_keys(std::string key, std::string value) : key(key), value(value) {}
  };

 private:
  std::list<expanded_keys> expand_keyword(const std::string &keyword, const std::string &value);

  void add_value(metric_container &metric, std::string value) {
    str::utils::token svalue = str::utils::split2(value, ":");
    if (svalue.first == "gauge") {
      for (const std::string &vkey : str::utils::split_lst(svalue.second, ",")) {
        if (vkey.size() > 0 && vkey[0] >= '0' && vkey[0] <= '9')
          metric.gauges.push_back(str::stox<double>(vkey, 0));
        else
          metric.gauges.push_back(str::stox<double>(metrics[vkey], 0));
      }
    }
    if (svalue.first == "derive") {
      std::string vkey = svalue.second;
      if (vkey.size() > 0 && vkey[0] >= '0' && vkey[0] <= '9')
        metric.derives.push_back(static_cast<long long>(str::stox<double>(svalue.second, 0)));
      else
        metric.derives.push_back(str::stox<unsigned long long>(metrics[svalue.second], 0));
    }
  }

  void add_type(std::string value, std::string plugin, boost::optional<std::string> p_instance, std::string tpe, boost::optional<std::string> t_instance) {
    for (const expanded_keys &et : expand_keyword(tpe, value)) {
      if (t_instance) {
        for (const expanded_keys &ei : expand_keyword(*t_instance, et.value)) {
          metric_container m = metric_container(time_hr, interval_hr);
          m.set_plugin(plugin, p_instance);
          m.set_type(et.key, ei.key);
          add_value(m, ei.value);
          rendererd_metrics.push_back(m);
        }
      } else {
        metric_container m = metric_container(time_hr, interval_hr);
        m.set_plugin(plugin, p_instance);
        m.set_type(et.key);
        add_value(m, et.value);
        rendererd_metrics.push_back(m);
      }
    }
  }

 public:
  void add_metric(std::string key, std::string value) {
    str::utils::token tag = str::utils::split2(key, "/");
    std::string plugin = tag.first;
    boost::optional<std::string> p_instance;
    std::string::size_type pos = plugin.find("-");
    if (pos != std::string::npos) {
      p_instance = plugin.substr(pos + 1);
      plugin = plugin.substr(0, pos);
    }
    std::string tpe = tag.second;
    boost::optional<std::string> t_instance;
    pos = tpe.find("-");
    if (pos != std::string::npos) {
      t_instance = tpe.substr(pos + 1);
      tpe = tpe.substr(0, pos);
    }

    for (const expanded_keys &ep : expand_keyword(plugin, value)) {
      if (p_instance) {
        for (const expanded_keys &ei : expand_keyword(*p_instance, ep.value)) {
          add_type(ei.value, ep.key, ei.key, tpe, t_instance);
        }
      } else {
        add_type(ep.value, ep.key, boost::optional<std::string>(), tpe, t_instance);
      }
    }
  }

  void add_variable(std::string key, std::string value);
  void set_time(unsigned long long time_hr_, unsigned long long interval_hr_) {
    time_hr = time_hr_;
    interval_hr = interval_hr_;
  }
  void set_host(const std::string &host_) { host = host_; }

  std::string to_string() const {
    std::stringstream ss;
    for (const metric_container &m : rendererd_metrics) {
      ss << m.to_string() << "\n";
    }
    return ss.str();
  }

  void render(packet_list &packets) {
    bool is_new = true;
    collectd::packet packet;

    std::string last_plugin = "";
    std::string last_plugin_instance = "";
    std::string last_type = "";
    std::string last_type_instance = "";
    for (const metric_container &m : rendererd_metrics) {
      if (is_new) {
        last_plugin = "";
        last_plugin_instance = "";
        last_type = "";
        last_type_instance = "";
        packet.add_host(host);
        packet.add_time_hr(time_hr);
        packet.add_interval_hr(interval_hr);
        is_new = false;
      }
      if (m.plugin_name != last_plugin) {
        packet.add_plugin(m.plugin_name);
        last_plugin = m.plugin_name;
      }
      if (m.plugin_instance && last_plugin_instance != m.plugin_instance.get()) {
        packet.add_plugin_instance(m.plugin_instance.get());
        last_plugin_instance = m.plugin_instance.get();
      } else if (!m.plugin_instance && last_plugin_instance != "") {
        packet.add_plugin_instance("");
        last_plugin_instance = "";
      }

      if (m.type_name != last_type) {
        packet.add_type(m.type_name);
        last_type = m.type_name;
      }
      if (m.type_instance && last_type_instance != m.type_instance.get()) {
        packet.add_type_instance(m.type_instance.get());
        last_type_instance = m.type_instance.get();
      } else if (!m.type_instance && last_type_instance != "") {
        packet.add_type_instance("");
        last_type_instance = "";
      }

      if (!m.gauges.empty()) {
        packet.add_gauge_value(m.gauges);
      }
      if (!m.derives.empty()) {
        packet.add_derive_value(m.derives);
      }
      if (packet.is_full()) {
        packets.push_back(packet);
        packet = collectd::packet();
        is_new = true;
      }
    }
    // Only emit the trailing packet if it actually holds data. When nothing
    // rendered (e.g. an unmatched variable) or the last metric exactly filled
    // and flushed a packet, `packet` is an empty default-constructed buffer and
    // must not be sent as a zero-length datagram.
    if (packet.get_size() > 0) packets.push_back(packet);
  }
  void set_metric(const ::std::string &key, const std::string &value);
};

}  // namespace collectd