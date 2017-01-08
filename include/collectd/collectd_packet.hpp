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

#pragma once

#include <boost/optional.hpp>

#include <types.hpp>
#include <swap_bytes.hpp>
#include <stdint.h>

#include <map>
#include <list>

namespace collectd {
	class data {
	public:
		struct string_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			char      data[];

			char* get_data() {
				return &data[0];
			}
		};
		struct int64_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			int64_t data;
		};
		struct value_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			int16_t   count;
		};
		struct derive_value_part : public boost::noncopyable {
			int8_t   type;
			int64_t   value;
		};
	};


	class collectd_exception : public std::exception {
		std::string msg_;
	public:
		collectd_exception() {}
		~collectd_exception() throw () {}
		collectd_exception(std::string msg) : msg_(msg) {}
		const char* what() const throw () { return msg_.c_str(); }
	};

	class packet {
	public:

		std::string buffer;
	public:
		packet() {}
		packet(const packet &other) : buffer(other.buffer) {}
		packet operator =(const packet &other) {
			buffer = other.buffer;
			return *this;
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "TODO";
			return ss.str();
		}

		void add_host(std::string value) {
			append_string(0x0000, value);
		}
		void add_plugin(std::string value) {
			append_string(0x0002, value);
		}
		void add_plugin_instance(std::string value) {
			append_string(0x0003, value);
		}
		void add_type(std::string value) {
			append_string(0x0004, value);
		}
		void add_type_instance(std::string value) {
			append_string(0x0005, value);
		}
		void add_gauge_value(std::list<double> values) {
			append_values(0x00006, 0x01, values);
		}
		void add_derive_value(std::list<long long> values) {
			append_values(0x00006, 0x02, values);
		}
		void add_time_hr(unsigned long long time) {
			append_int(0x0008, time);
		}
		void add_interval_hr(unsigned long long time) {
			append_int(0x0009, time);
		}

		bool is_full() const {
			return buffer.size() > 300;
		}


		template<class T>
		inline void set_byte(std::string &buffer, const std::string::size_type pos, const T value) {
			T *b_value = reinterpret_cast<T*>(&buffer[pos]);
			*b_value = swap_bytes::hton<T>(value);
		}

		void append_string(int type, std::string &string_data) {
			std::string::size_type len = string_data.length() + 5;
			std::string::size_type pos = buffer.length();
			buffer.append(sizeof(collectd::data::string_part), '\0');
			collectd::data::string_part *data = reinterpret_cast<collectd::data::string_part*>(&buffer[pos]);
			data->type = swap_bytes::hton<int16_t>(type);
			data->length = swap_bytes::hton<int16_t>(len);
			buffer.append(string_data.c_str(), string_data.length() + 1);
		}
		void append_int(int type, unsigned long long int_data) {
			std::string::size_type pos = buffer.length();
			buffer.append(sizeof(int16_t) + sizeof(int16_t) + sizeof(int64_t), '\0');
			std::string::size_type len = buffer.length() - pos;
			set_byte<uint16_t>(buffer, pos, type);
			set_byte<uint16_t>(buffer, pos + sizeof(int16_t), len);
			set_byte<uint64_t>(buffer, pos + sizeof(int16_t) + sizeof(int16_t), int_data);
		}
		void append_values(int base_type, int value_type, const std::list<double> &value_data) {
			std::string::size_type pos = buffer.length();
			buffer.append(sizeof(collectd::data::value_part), '\0');
			for (std::size_t i = 0;  i < value_data.size(); i++) {
				append_value_type(value_type);
			}
			BOOST_FOREACH(const double &v, value_data) {
				append_value_value(v);
			}
			std::string::size_type len = buffer.length() - pos;
			collectd::data::value_part *data = reinterpret_cast<collectd::data::value_part*>(&buffer[pos]);
			data->type = swap_bytes::hton<int16_t>(base_type);
			data->count = swap_bytes::hton<int16_t>(value_data.size());
			data->length = swap_bytes::hton<int16_t>(len);
		}
		void append_values(int base_type, int value_type, const std::list<long long> &value_data) {
			std::string::size_type pos = buffer.length();
			buffer.append(sizeof(collectd::data::value_part), '\0');
			for (std::size_t i = 0; i < value_data.size(); i++) {
				append_value_type(value_type);
			}
			BOOST_FOREACH(const long long &v, value_data) {
				append_value_value(v);
			}
			std::string::size_type len = buffer.length() - pos;
			collectd::data::value_part *data = reinterpret_cast<collectd::data::value_part*>(&buffer[pos]);
			data->type = swap_bytes::hton<int16_t>(base_type);
			data->count = swap_bytes::hton<int16_t>(value_data.size());
			data->length = swap_bytes::hton<int16_t>(len);
		}

		void append_value_type(int type) {
			std::string::size_type pos = buffer.length();
			int sz = sizeof(int8_t);
			buffer.append(sz, '\0');
			int8_t *b_type = reinterpret_cast<int8_t*>(&buffer[pos]);
			*b_type = type;
		}
		void append_value_value(const double double_data) {
			std::string::size_type pos = buffer.length();
			int sz = sizeof(int64_t);
			buffer.append(sz, '\0');
			// 				int64_t *b_value = reinterpret_cast<int64_t*>(&buffer[pos + sizeof(int8_t)]);
			// 				*b_value = swap_bytes::hton<int64_t>(*int_data);
			double *b_dvalue = reinterpret_cast<double*>(&buffer[pos]);
			*b_dvalue = double_data;
		}
		void append_value_value(const long long int_data) {
			std::string::size_type pos = buffer.length();
			int sz = sizeof(int64_t);
			buffer.append(sz, '\0');
			int64_t *b_value = reinterpret_cast<int64_t*>(&buffer[pos]);
			*b_value = swap_bytes::hton<int64_t>(int_data);
		}


		std::string get_buffer() const {
			return buffer;
		}
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
			void set_type(const std::string &type_name_) {
				type_name = type_name_;
			}
			void set_plugin(const std::string &plugin_name_, const boost::optional<std::string> &plugin_instance_) {
				plugin_name = plugin_name_;
				plugin_instance = plugin_instance_;
			}
			void set_plugin(const std::string &plugin_name_) {
				plugin_name = plugin_name_;
			}
			std::string to_string() const {
				std::stringstream ss;
				ss << plugin_name << "-";
				if (plugin_instance)
					ss << *plugin_instance;
				ss << "/" << type_name << "-";
				if (type_instance)
					ss << *type_instance;
				ss << "=";
				if (!gauges.empty()) {
					ss << " gagues: ";
					BOOST_FOREACH(const double &d, gauges) {
						ss << d << ", ";
					}
				}
				if (!derives.empty()) {
					ss << " derives: ";
					BOOST_FOREACH(const long long &d, derives) {
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
			boost::tuple<std::string, std::string> svalue = strEx::s::split2(value, ":");
			if (svalue.get<0>() == "gauge") {
				BOOST_FOREACH(const std::string &vkey, strEx::s::splitEx(svalue.get<1>(), ",")) {
					if (vkey.size() > 0 && vkey[0] >= '0' && vkey[0] <= '9')
						metric.gauges.push_back(strEx::s::stox<double>(vkey, 0));
					else
						metric.gauges.push_back(strEx::s::stox<double>(metrics[vkey], 0));
				}
			}
			if (svalue.get<0>() == "derive") {
				std::string vkey = svalue.get<1>();
				if (vkey.size() > 0 && vkey[0] >= '0' && vkey[0] <= '9')
					metric.derives.push_back(strEx::s::stox<double>(svalue.get<1>(), 0));
				else
					metric.derives.push_back(strEx::s::stox<unsigned long long>(metrics[svalue.get<1>()], 0));
			}
		}


		void add_type(std::string value, std::string plugin, boost::optional<std::string> p_instance, std::string tpe, boost::optional<std::string> t_instance) {
			BOOST_FOREACH(const expanded_keys &et, expand_keyword(tpe, value)) {
				if (t_instance) {
					BOOST_FOREACH(const expanded_keys &ei, expand_keyword(*t_instance, et.value)) {
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
			boost::tuple<std::string, std::string> tag = strEx::s::split2(key, "/");
			std::string plugin = tag.get<0>();
			boost::optional<std::string> p_instance;
			std::string::size_type pos = plugin.find("-");
			if (pos != std::string::npos) {
				p_instance = plugin.substr(pos + 1);
				plugin = plugin.substr(0, pos);
			}
			std::string tpe = tag.get<1>();
			boost::optional<std::string> t_instance;
			pos = tpe.find("-");
			if (pos != std::string::npos) {
				t_instance = tpe.substr(pos + 1);
				tpe = tpe.substr(0, pos);
			}



			BOOST_FOREACH(const expanded_keys &ep, expand_keyword(plugin, value)) {
				if (p_instance) {
					BOOST_FOREACH(const expanded_keys &ei, expand_keyword(*p_instance, ep.value)) {
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
		void set_host(const std::string&host_) {
			host = host_;
		}



		std::string to_string() const {
			std::stringstream ss;
			BOOST_FOREACH(const metric_container &m, rendererd_metrics) {
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
			BOOST_FOREACH(const metric_container &m, rendererd_metrics) {
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
			packets.push_back(packet);

		}
		void set_metric(const ::std::string& key, const std::string &value);
	};


}