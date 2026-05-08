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
#include <nscapi/settings/helper.hpp>

namespace smtp_handler {
namespace sh = nscapi::settings_helper;

struct smtp_target_object : nscapi::targets::target_object {
  typedef target_object parent;

  smtp_target_object(const std::string& alias, const std::string& path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_string("address", "smtp://smtp.example.com:587");
    set_property_string("security", "starttls");
    set_property_string("sender", "nscp@localhost");
    set_property_string("recipient", "nscp@localhost");
    set_property_string("subject", "[NSClient++] %source%");
    set_property_string("template", "Hello, this is %source% reporting %message%!");
  }
  smtp_target_object(const nscapi::settings_objects::object_instance& other, const std::string& alias, const std::string& path) : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path
        .add_key()

        .add_string("sender", sh::string_fun_key([this](auto value) { this->set_property_string("sender", value); }, "nscp@localhost"), "SENDER",
                    "Envelope sender / From: header. Use a real address (e.g. alerts@example.com) - public providers like Gmail and Microsoft 365 reject "
                    "submissions where From: differs from the authenticated mailbox.")

        .add_string("recipient", sh::string_fun_key([this](auto value) { this->set_property_string("recipient", value); }, "nscp@localhost"), "RECIPIENT",
                    "Recipient (RCPT TO and To: header). Single recipient per submission.")

        .add_string("subject", sh::string_fun_key([this](auto value) { this->set_property_string("subject", value); }, "[NSClient++] %source%"), "SUBJECT",
                    "Subject template. %source% is replaced with the originating check name, %message% with the plugin output.")

        .add_string("template",
                    sh::string_fun_key([this](auto value) { this->set_property_string("template", value); }, "Hello, this is %source% reporting %message%!"),
                    "TEMPLATE", "Body template. Same %source% / %message% substitution as `subject`.")

        .add_string("username", sh::string_fun_key([this](auto value) { this->set_property_string("username", value); }, ""), "AUTH USERNAME",
                    "SMTP AUTH username. Empty disables auth (only valid for unauthenticated relays). For Gmail use the full Google account address; for "
                    "Microsoft 365 use the UPN. App passwords work for both.")

        .add_password("password", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }, ""), "AUTH PASSWORD",
                      "SMTP AUTH password. Stored sensitive. Credentials are only sent over a TLS / STARTTLS-secured connection; configuring a password "
                      "with security=none refuses to start.")

        .add_string("security", sh::string_fun_key([this](auto value) { this->set_property_string("security", value); }, "starttls"), "TRANSPORT SECURITY",
                    "Connection security. `starttls` (default, port 587) connects in clear and upgrades to TLS before AUTH; `tls` connects with TLS "
                    "immediately (port 465); `none` is plain SMTP and is only safe for trusted internal relays.")

        .add_string("ehlo-hostname", sh::string_fun_key([this](auto value) { this->set_property_string("ehlo-hostname", value); }, ""), "EHLO HOSTNAME",
                    "Hostname to advertise in EHLO. Defaults to the agent's hostname; some submission services require a real FQDN here.")

        .add_string("insecure-skip-verify", sh::bool_fun_key([this](auto value) { this->set_property_bool("insecure-skip-verify", value); }, false),
                    "SKIP TLS CERT VERIFY",
                    "When true, skip certificate validation on the server. Only safe for self-signed test environments; never set this on a production "
                    "submission service.");
  }
};

struct options_reader_impl : client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override { return boost::make_shared<smtp_target_object>(alias, path); }
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) override {
    return boost::make_shared<smtp_target_object>(parent, alias, path);
  }

  void process(boost::program_options::options_description& desc, client::destination_container& source, client::destination_container& data) override {
    // clang-format off
    desc.add_options()
      ("sender", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("sender", value); }),
       "Envelope sender / From: header.")
      ("recipient", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("recipient", value); }),
       "Recipient address (one per submission).")
      ("subject", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("subject", value); }),
       "Subject template; %source% / %message% are substituted.")
      ("template", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("template", value); }),
       "Body template; %source% / %message% are substituted.")
      ("username", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("username", value); }),
       "SMTP AUTH username.")
      ("password", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("password", value); }),
       "SMTP AUTH password.")
      ("security", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("security", value); }),
       "Transport security: none | starttls (default) | tls.")
      ("ehlo-hostname", po::value<std::string>()->notifier([&data] (auto value) { data.set_string_data("ehlo-hostname", value); }),
       "Hostname to send in EHLO.")
      ("insecure-skip-verify", po::bool_switch()->notifier([&data] (auto value) { data.set_bool_data("insecure-skip-verify", value); }),
       "Skip TLS certificate validation (test environments only).")
      ("source-host", po::value<std::string>()->notifier([&source] (auto value) { source.set_string_data("host", value); }),
       "Source/sender host name (default is auto which means use the name of the actual host).")
      ("sender-host", po::value<std::string>()->notifier([&source] (auto value) { source.set_string_data("host", value); }),
       "Source/sender host name (alias for --source-host).")
      ;
    // clang-format on
  }
};
}  // namespace smtp_handler
