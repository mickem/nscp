// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <win/pdh/pdh_object_gather.hpp>

#include <list>
#include <win/pdh/pdh_counters.hpp>
#include <win/pdh/pdh_query.hpp>

namespace PDH {

namespace {

// The aggregate pseudo-instance is not localized and always spelled _Total.
bool is_total_instance(const std::string &name) { return name == "_Total"; }

std::list<pdh_instance> add_counters(PDHQuery &query, const std::string &object, const std::vector<std::string> &counters, const bool with_instances) {
  std::list<pdh_instance> instances;
  std::string first_error;
  for (const std::string &counter : counters) {
    try {
      pdh_object obj;
      obj.set_counter("\\" + object + (with_instances ? "(*)" : "") + "\\" + counter);
      obj.set_alias(counter);
      if (with_instances) obj.set_instances("true");
      obj.set_strategy_static();
      obj.set_type("double");
      obj.set_resolution("auto");
      pdh_instance instance = factory::create(obj);
      query.addCounter(instance);
      instances.push_back(instance);
    } catch (const pdh_exception &e) {
      // Individual counters vary between Windows versions and SKUs (e.g. the
      // Terminal Services Session protocol counters only exist on session
      // hosts): skip what this host does not have and let the values default,
      // as long as at least one counter of the object resolves.
      if (first_error.empty()) first_error = e.reason();
    }
  }
  if (instances.empty()) throw pdh_exception(first_error.empty() ? "No counters given for " + object : first_error);
  return instances;
}

void collect(PDHQuery &query, const bool double_sample) {
  query.open();
  if (double_sample) {
    query.collect();
    Sleep(1000);
  }
  // ignore_errors=true: with a wildcard, individual instances can come and go
  // between expansion and collection (and rate counters lack a second sample
  // when double_sample is off) — neither should fail the whole gather.
  query.gatherData(true);
  query.close();
}

}  // namespace

object_instance_values gather_object_instances(const std::string &object, const std::vector<std::string> &counters, const bool double_sample) {
  PDHQuery query;
  std::list<pdh_instance> roots = add_counters(query, object, counters, true);
  collect(query, double_sample);

  object_instance_values out;
  for (pdh_instance &root : roots) {
    // The root's alias is the plain counter name; each expanded child is
    // named "<alias>_<instance>", so strip the prefix to get the instance.
    const std::string prefix = root->get_name() + "_";
    for (const pdh_instance &child : root->get_instances()) {
      std::string name = child->get_name();
      if (name.compare(0, prefix.size(), prefix) == 0) name = name.substr(prefix.size());
      if (is_total_instance(name)) continue;
      try {
        out[name][root->get_name()] = child->get_float_value();
      } catch (const pdh_exception &) {
        // A rate counter without its second sample (double_sample=false) has
        // no formatted value yet; report 0 rather than failing the record.
        out[name][root->get_name()] = 0.0;
      }
    }
  }
  return out;
}

std::map<std::string, double> gather_object_values(const std::string &object, const std::vector<std::string> &counters, const bool double_sample) {
  PDHQuery query;
  std::list<pdh_instance> roots = add_counters(query, object, counters, false);
  collect(query, double_sample);

  std::map<std::string, double> out;
  for (pdh_instance &root : roots) {
    try {
      out[root->get_name()] = root->get_float_value();
    } catch (const pdh_exception &) {
      out[root->get_name()] = 0.0;
    }
  }
  return out;
}

}  // namespace PDH
