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

#include <boost/make_shared.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

namespace syslog_handler {
namespace sh = nscapi::settings_helper;

struct syslog_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  syslog_target_object(std::string alias, std::string path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_string("path", "/nsclient++");
    set_property_string("severity", "error");
    set_property_string("facility", "kernel");
    set_property_string("tag syntax", "NSCA");
    set_property_string("message syntax", "%message%");
    set_property_string("ok severity", "informational");
    set_property_string("warning severity", "warning");
    set_property_string("critical severity", "critical");
    set_property_string("unknown severity", "emergency");
  }
  syslog_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path) : parent(other, alias, path) {}

  virtual void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_key()
        .add_string("severity", sh::string_fun_key([this](auto value) { this->set_property_string("severity", value); }, "error"), "TODO", "")
        .add_string("facility", sh::string_fun_key([this](auto value) { this->set_property_string("facility", value); }, "kernel"), "TODO", "")
        .add_string("tag_syntax", sh::string_fun_key([this](auto value) { this->set_property_string("tag syntax", value); }, "NSCA"), "TODO", "")
        .add_string("message_syntax", sh::string_fun_key([this](auto value) { this->set_property_string("message syntax", value); }, "%message%"), "TODO", "")
        .add_string("ok severity", sh::string_fun_key([this](auto value) { this->set_property_string("ok severity", value); }, "informational"), "TODO", "")
        .add_string("warning severity", sh::string_fun_key([this](auto value) { this->set_property_string("warning severity", value); }, "warning"), "TODO", "")
        .add_string("critical severity", sh::string_fun_key([this](auto value) { this->set_property_string("critical severity", value); }, "critical"), "TODO",
                    "")
        .add_string("unknown severity", sh::string_fun_key([this](auto value) { this->set_property_string("unknown severity", value); }, "emergency"), "TODO",
                    "");
  }
};

struct options_reader_impl : public client::options_reader_interface {
  virtual nscapi::settings_objects::object_instance create(std::string alias, std::string path) {
    return boost::make_shared<syslog_target_object>(alias, path);
  }
  virtual nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) {
    return boost::make_shared<syslog_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description &desc, client::destination_container &source, client::destination_container &data) {
    // clang-format off
  desc.add_options()
    ("path", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("path", value); }),
    "")
    ("severity,s", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("severity", value); }),
    "Severity of error message")
    ("unknown-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("unknown_severity", value); }),
    "Severity of error message")
    ("ok-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("ok_severity", value); }),
    "Severity of error message")
    ("warning-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("warning_severity", value); }),
    "Severity of error message")
    ("critical-severity", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("critical_severity", value); }),
    "Severity of error message")
    ("facility,f", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("facility", value); }),
    "Facility of error message")
    ("tag template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("tag template", value); }),
    "Tag template (TODO)")
    ("message template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("message template", value); }),
    "Message template (TODO)")
    ;
    // clang-format on
  }
};
}  // namespace syslog_handler