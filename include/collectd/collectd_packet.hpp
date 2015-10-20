#pragma once

#include <boost/optional.hpp>

#include <types.hpp>
#include <swap_bytes.hpp>
#include <unicode_char.hpp>

namespace collectd {
	class data {
	public:
		typedef struct string_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			char      data[];

			char* get_data() {
				return &data[0];
			}
		};
		typedef struct int64_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			int64_t data;
		};
		typedef struct value_part : public boost::noncopyable {
			int16_t   type;
			int16_t   length;
			int16_t   count;
		};
		typedef struct derive_value_part : public boost::noncopyable {
			int8_t   type;
			int64_t   value;
		};
	};

	template<class T>
	inline void set_byte(std::string &buffer, const std::string::size_type pos, const T value) {
		T *b_value = reinterpret_cast<T*>(&buffer[pos]);
		*b_value = swap_bytes::hton<T>(value);
	}

	class collectd_exception : public std::exception {
		std::string msg_;
	public:
		collectd_exception() {}
		~collectd_exception() throw () {}
		collectd_exception(std::string msg) : msg_(msg) {}
		const char* what() const throw () { return msg_.c_str(); }
	};

	struct collectd_value {
		int type;
		boost::optional<double> double_data;
		boost::optional<long long> int_data;


		static collectd_value mk_gague(const double  &v) {
			collectd_value ret;
			ret.type = 0x01;
			ret.double_data = v;
			return ret;
		}

		void append_to(std::string &buffer) const {
			std::string::size_type pos = buffer.length();
			int sz = sizeof(int8_t)+sizeof(int64_t);
			buffer.append(sz, '\0');
			int8_t *b_type = reinterpret_cast<int8_t*>(&buffer[pos]);
			*b_type = type;
			if (int_data) {
				int64_t *b_value = reinterpret_cast<int64_t*>(&buffer[pos + sizeof(int8_t)]);
				*b_value = swap_bytes::hton<int64_t>(*int_data);
			}
			if (double_data) {
				double *b_dvalue = reinterpret_cast<double*>(&buffer[pos + sizeof(int8_t)]);
				*b_dvalue = *double_data;
			}
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "{ type: " << type << ", value = ";
			if (double_data)
				ss << *double_data;
			if (int_data)
				ss << *int_data;
			return ss.str();
		}

	};

	typedef std::list<collectd_value> collectd_value_list;
	struct collectd_element {
		int type;
		boost::optional<std::string> string_data;
		boost::optional<double> double_data;
		boost::optional<unsigned long long> int_data;
		boost::optional<collectd_value_list> value_data;

		static collectd_element mk_string(int type, std::string s) {
			collectd_element ret;
			ret.string_data = s;
			ret.type = type;
			return ret;
		}
		static collectd_element mk_int(int type, unsigned long long s) {
			collectd_element ret;
			ret.int_data = s;
			ret.type = type;
			return ret;
		}
		static collectd_element mk_value(int type, const collectd_value_list &v) {
			collectd_element ret;
			ret.value_data = v;
			ret.type = type;
			return ret;
		}

		std::string to_string() const {
			std::stringstream ss;
			ss << "{ type: " << type << ", value = ";
			if (string_data)
				ss << *string_data;
			if (double_data)
				ss << *double_data;
			if (int_data)
				ss << *int_data;
			if (value_data) {
				BOOST_FOREACH(const collectd_value &v, *value_data) {
					ss << v.to_string() << ", ";
				}
			}
			return ss.str();
		}
		void append_to(std::string &buffer) const {
			if (string_data) {
				std::string::size_type len = string_data->length() + 5;
				std::string::size_type pos = buffer.length();
				buffer.append(sizeof(collectd::data::string_part), '\0');
				collectd::data::string_part *data = reinterpret_cast<collectd::data::string_part*>(&buffer[pos]);
				data->type = swap_bytes::hton<int16_t>(type);
				data->length = swap_bytes::hton<int16_t>(len);
				buffer.append(string_data->c_str(), string_data->length() + 1);
			}
			if (int_data) {
				std::string::size_type pos = buffer.length();
				buffer.append(sizeof(int16_t) + sizeof(int16_t) + sizeof(int64_t), '\0');
				std::string::size_type len = buffer.length() - pos;
				set_byte<uint16_t>(buffer, pos, type);
				set_byte<uint16_t>(buffer, pos + sizeof(int16_t), len);
				set_byte<uint64_t>(buffer, pos + sizeof(int16_t) + sizeof(int16_t), *int_data);
			}
			if (value_data) {
				std::string::size_type pos = buffer.length();
				buffer.append(sizeof(collectd::data::value_part), '\0');
				BOOST_FOREACH(const collectd_value &v, *value_data) {
					v.append_to(buffer);
				}
				std::string::size_type len = buffer.length() - pos;
				collectd::data::value_part *data = reinterpret_cast<collectd::data::value_part*>(&buffer[pos]);
				data->type = swap_bytes::hton<int16_t>(type);
				data->count = swap_bytes::hton<int16_t>(value_data->size());
				data->length = swap_bytes::hton<int16_t>(len);
			}
		}

	};
	typedef std::list<collectd_element> collectd_data;

	class packet {
	public:

		collectd_data data;
	public:
		packet() {}
		packet(const packet &other) :data(other.data) {}
		packet operator =(const packet &other) {
			data = other.data;
			return *this;
		}

		std::string to_string() const {
			std::stringstream ss;
			BOOST_FOREACH(const collectd_element &e, data) {
				ss << e.to_string() << ", ";

			}
			return ss.str();
		}

		void add_host(std::string value) {
			data.push_back(collectd_element::mk_string(0x0000, value));
		}
		void add_plugin(std::string value) {
			data.push_back(collectd_element::mk_string(0x0002, value));
		}
		void add_plugin_instance(std::string value) {
			data.push_back(collectd_element::mk_string(0x0003, value));
		}
		void add_type(std::string value) {
			data.push_back(collectd_element::mk_string(0x0004, value));
		}
		void add_type_instance(std::string value) {
			data.push_back(collectd_element::mk_string(0x0005, value));
		}
		void add_value(collectd_value_list values) {
			data.push_back(collectd_element::mk_value(0x00006, values));
		}
		void add_time_hr(unsigned long long time) {
			data.push_back(collectd_element::mk_int(0x0008, time));
		}
		void add_interval_hr(unsigned long long time) {
			data.push_back(collectd_element::mk_int(0x0009, time));
		}

		void parse_data(const char* buffer, unsigned int buffer_len) {
		}
/*
		static void copy_string(char* data, const std::string &value, std::string::size_type max_length) {
			memset(data, 0, max_length);
			value.copy(data, value.size()>max_length?max_length:value.size());
		}
´*/
		void get_buffer(std::string &buffer) const {
			BOOST_FOREACH(const collectd_element &e, data) {
				e.append_to(buffer);
			}

		}
		std::string get_buffer() const {
			std::string buffer;
			get_buffer(buffer);
			return buffer;
		}
	};
}
