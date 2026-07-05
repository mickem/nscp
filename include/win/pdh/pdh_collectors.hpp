// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/circular_buffer.hpp>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <numeric>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_interface.hpp>

namespace PDH {
namespace instance_providers {
struct container : pdh_instance_interface {
  std::string alias_;
  std::list<std::shared_ptr<pdh_instance_interface> > children_;

  bool has_instances() override { return true; }
  std::list<std::shared_ptr<pdh_instance_interface> > get_instances() override { return children_; }

  container(const pdh_object &parent, const std::list<pdh_object> &sub_counters) : alias_(parent.alias) {
    for (const pdh_object &o : sub_counters) {
      children_.push_back(factory::create(o));
    }
  }
  std::string get_name() const override { return alias_; }
  std::string get_counter() const override { return ""; }

  DWORD get_format() override { return 0; }

  double get_average(long seconds) override {
    double sum = 0;
    for (pdh_instance o : children_) {
      sum += o->get_average(seconds);
    }
    return sum;
  }
  double get_value() override {
    double sum = 0;
    for (pdh_instance o : children_) {
      sum += o->get_value();
    }
    return sum;
  }
  long long get_int_value() override {
    long long sum = 0;
    for (pdh_instance o : children_) {
      sum += o->get_int_value();
    }
    return sum;
  }
  double get_float_value() override {
    double sum = 0;
    for (pdh_instance o : children_) {
      sum += o->get_float_value();
    }
    return sum;
  }
  void collect(const PDH_FMT_COUNTERVALUE &value) override {}
};

class base_counter : public pdh_instance_interface {
  std::string alias_;
  std::string path_;
  unsigned long format_;

 public:
  explicit base_counter(const pdh_object &config) : alias_(config.alias), path_(config.path), format_(config.get_flags()) {}
  std::string get_name() const override { return alias_; }
  std::string get_counter() const override { return path_; }
  DWORD get_format() override { return format_; }

  bool has_instances() override { return false; }
  std::list<std::shared_ptr<pdh_instance_interface> > get_instances() override {
    std::list<std::shared_ptr<pdh_instance_interface> > ret;
    return ret;
  }
};

template <class T>
struct base_collector : base_counter {
  explicit base_collector(const pdh_object &config) : base_counter(config) {}

  void collect(const PDH_FMT_COUNTERVALUE &value) override = 0;
  virtual void update(T value) = 0;
};

template <>
struct base_collector<double> : base_counter {
  explicit base_collector(const pdh_object &config) : base_counter(config) {}
  void collect(const PDH_FMT_COUNTERVALUE &value) override { update(value.doubleValue); }
  virtual void update(double value) = 0;
};
template <>
struct base_collector<long long> : base_counter {
  explicit base_collector(const pdh_object &config) : base_counter(config) {}
  void collect(const PDH_FMT_COUNTERVALUE &value) override { update(value.largeValue); }
  virtual void update(long long value) = 0;
};
template <>
struct base_collector<long> : base_counter {
  explicit base_collector(const pdh_object &config) : base_counter(config) {}
  void collect(const PDH_FMT_COUNTERVALUE &value) override { update(value.longValue); }
  virtual void update(long value) = 0;
};

template <class T>
class value_collector : public base_collector<T> {
  boost::shared_mutex mutex_;
  T value;

 public:
  explicit value_collector(pdh_object config) : base_collector<T>(config), value(0) {}
  double get_average(long) override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<double>(value);
  }
  double get_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<double>(value);
  }
  long long get_int_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<long long>(value);
  }
  double get_float_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<double>(value);
  }
  void update(T newValue) override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    value = newValue;
  }
};

template <class T>
class rrd_collector : public base_collector<T> {
  boost::shared_mutex mutex_;
  boost::circular_buffer<T> values;

 public:
  explicit rrd_collector(pdh_object config) : base_collector<T>(config) {
    values.resize(config.buffer_size);
    for (int i = 0; i < config.buffer_size; i++) {
      values[i] = 0;
    }
  }
  explicit rrd_collector(int size) : base_collector<T>(pdh_object()), values(size) {
    values.resize(size);
    for (int i = 0; i < size; i++) {
      values[i] = 0;
    }
  }
  double get_average(long seconds) override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    if (seconds > values.size()) throw pdh_exception(get_name(), "Buffer to small");
    if (seconds == 0) throw pdh_exception(get_name(), "INvalid size");

    const double sum = std::accumulate(values.end() - seconds, values.end(), 0.0);
    return sum / seconds;
  }
  double get_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<double>(values.back());
  }
  long long get_int_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<long long>(values.back());
  }
  double get_float_value() override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    return static_cast<double>(values.back());
  }

  void update(T value) override {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (!lock.owns_lock()) throw pdh_exception(get_name(), "Could not get mutex");
    values.push_back(value);
  }
};
}  // namespace instance_providers
}  // namespace PDH