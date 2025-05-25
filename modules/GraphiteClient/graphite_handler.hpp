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

#include <nscapi/nscapi_settings_helper.hpp>
#include <boost/make_shared.hpp>

namespace graphite_handler {
namespace sh = nscapi::settings_helper;

struct graphite_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  graphite_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_bool("send perfdata", true);
    set_property_bool("send status", true);
    set_property_int("timeout", 30);
    set_property_string("perf path", "nsclient.${hostname}.${check_alias}.${perf_alias}");
    set_property_string("status path", "nsclient.${hostname}.${check_alias}.status");
    set_property_string("metric path", "nsclient.${hostname}.${metric}");
  }
  graphite_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    if (is_default()) {
      root_path
          .add_key()

          .add_string(
              "path",
              sh::string_fun_key([this](auto value) { this->set_property_string("perf path", value); }, "nsclient.${hostname}.${check_alias}.${perf_alias}"),
              "PATH FOR METRICS", "Path mapping for metrics")

          .add_string("status path",
                      sh::string_fun_key([this](auto value) { this->set_property_string("status path", value); }, "nsclient.${hostname}.${check_alias}.status"),
                      "PATH FOR STATUS", "Path mapping for status")

          .add_bool("send perfdata", sh::bool_fun_key([this](auto value) { this->set_property_bool("send perfdata", value); }, true), "SEND PERF DATA",
                    "Send performance data to this server")

          .add_bool("send status", sh::bool_fun_key([this](auto value) { this->set_property_bool("send status", value); }, true), "SEND STATUS",
                    "Send status data to this server")

          .add_string("metric path",
                      sh::string_fun_key([this](auto value) { this->set_property_string("metric path", value); }, "nsclient.${hostname}.${metric}"),
                      "PATH FOR METRICS", "Path mapping for metrics");
    } else {
      root_path
          .add_key()

          .add_string("path", sh::string_fun_key([this](auto value) { this->set_property_string("perf path", value); }), "PATH FOR METRICS",
                      "Path mapping for metrics")

          .add_string("status path", sh::string_fun_key([this](auto value) { this->set_property_string("status path", value); }), "PATH FOR STATUS",
                      "Path mapping for status")

          .add_bool("send perfdata", sh::bool_fun_key([this](auto value) { this->set_property_bool("send perfdata", value); }), "SEND PERF DATA",
                    "Send performance data to this server")

          .add_bool("send status", sh::bool_fun_key([this](auto value) { this->set_property_bool("send status", value); }), "SEND STATUS",
                    "Send status data to this server");
    }
    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return boost::make_shared<graphite_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return boost::make_shared<graphite_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    desc.add_options()("path", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("path", value); }), "");
  }
};
}  // namespace graphite_handler