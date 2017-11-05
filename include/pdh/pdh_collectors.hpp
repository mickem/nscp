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

#include <numeric>

#include <pdh/pdh_interface.hpp>
#include <pdh/pdh_counters.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/foreach.hpp>

namespace PDH {
	namespace instance_providers {
		struct container : public PDH::pdh_instance_interface {
			std::string alias_;
			std::list<boost::shared_ptr<pdh_instance_interface> > children_;

			virtual bool has_instances() {
				return true;
			}
			virtual std::list<boost::shared_ptr<pdh_instance_interface> > get_instances() {
				return children_;
			}

			container(pdh_object parent, std::list<pdh_object> sub_counters) : alias_(parent.alias) {
				BOOST_FOREACH(const pdh_object &o, sub_counters) {
					children_.push_back(PDH::factory::create(o));
				}
			}
			virtual std::string get_name() const {
				return alias_;
			}
			virtual std::string get_counter() const {
				return "";
			}

			virtual DWORD get_format() { return 0; }

			virtual double get_average(long seconds) {
				double sum = 0;
				BOOST_FOREACH(pdh_instance o, children_) {
					sum += o->get_average(seconds);
				}
				return sum;
			}
			virtual double get_value() {
				double sum = 0;
				BOOST_FOREACH(pdh_instance o, children_) {
					sum += o->get_value();
				}
				return sum;
			}
			virtual long long get_int_value() {
				long long sum = 0;
				BOOST_FOREACH(pdh_instance o, children_) {
					sum += o->get_int_value();
				}
				return sum;
			}
			virtual double get_float_value() {
				double sum = 0;
				BOOST_FOREACH(pdh_instance o, children_) {
					sum += o->get_float_value();
				}
				return sum;
			}
			virtual void collect(const PDH_FMT_COUNTERVALUE &value) {}
		};

		class base_counter : public PDH::pdh_instance_interface {
			std::string alias_;
			std::string path_;
			unsigned long format_;
		public:
			base_counter(pdh_object config) : alias_(config.alias), path_(config.path), format_(config.get_flags()) {}
			virtual std::string get_name() const {
				return alias_;
			}
			virtual std::string get_counter() const {
				return path_;
			}
			virtual DWORD get_format() {
				return format_;
			}

			virtual bool has_instances() {
				return false;
			}
			virtual std::list<boost::shared_ptr<pdh_instance_interface> > get_instances() {
				std::list<boost::shared_ptr<pdh_instance_interface> > ret;
				return ret;
			}
		};

		template<class T>
		struct base_collector : public base_counter {
			base_collector(pdh_object config) : base_counter(config) {}

			virtual void collect(const PDH_FMT_COUNTERVALUE &value) = 0;
			virtual void update(T value) = 0;
		};

		template<>
		struct base_collector<double> : public base_counter {
			base_collector(pdh_object config) : base_counter(config) {}
			virtual void collect(const PDH_FMT_COUNTERVALUE &value) {
				update(value.doubleValue);
			}
			virtual void update(double value) = 0;
		};
		template<>
		struct base_collector<long long> : public base_counter {
			base_collector(pdh_object config) : base_counter(config) {}
			virtual void collect(const PDH_FMT_COUNTERVALUE &value) {
				update(value.largeValue);
			}
			virtual void update(long long value) = 0;
		};
		template<>
		struct base_collector<long> : public base_counter {
			base_collector(pdh_object config) : base_counter(config) {}
			virtual void collect(const PDH_FMT_COUNTERVALUE &value) {
				update(value.longValue);
			}
			virtual void update(long value) = 0;
		};

		template<class T>
		class value_collector : public base_collector<T> {
			boost::shared_mutex mutex_;
			T value;
		public:
			value_collector(pdh_object config) : base_collector<T>(config), value(0) {}
			virtual double get_average(long) {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return value;
			}
			virtual double get_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return static_cast<T>(value);
			}
			virtual long long get_int_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return value;
			}
			virtual double get_float_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return value;
			}
			virtual void update(T newValue) {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				value = newValue;
			}
		};

		template<class T>
		class rrd_collector : public base_collector<T> {
			boost::shared_mutex mutex_;
			boost::circular_buffer<T> values;

		public:
			rrd_collector(pdh_object config) : base_collector<T>(config) {
				values.resize(config.buffer_size);
				for (int i = 0; i < config.buffer_size; i++) {
					values[i] = 0;
				}
			}
			rrd_collector(int size) {
				values.resize(size);
				for (int i = 0; i < size; i++) {
					values[i] = 0;
				}
			}
			virtual double get_average(long seconds) {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				if (seconds > values.size())
					throw PDH::pdh_exception(get_name(), "Buffer to small");
				if (seconds == 0)
					throw PDH::pdh_exception(get_name(), "INvalid size");

				double sum = std::accumulate(values.end() - seconds, values.end(), 0.0);
				return sum / seconds;
			}
			virtual double get_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return values.back();
			}
			virtual long long get_int_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return values.back();
			}
			virtual double get_float_value() {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				return values.back();
			}

			virtual void update(T value) {
				boost::shared_lock<boost::shared_mutex> lock(mutex_);
				if (!lock.owns_lock())
					throw PDH::pdh_exception(get_name(), "Could not get mutex");
				values.push_back(value);
			}
		};
	}
}