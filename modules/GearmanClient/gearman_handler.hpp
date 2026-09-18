// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_targets.hpp>
#include <nscapi/settings/helper.hpp>
#include <string>

#include "gearman_protocol.hpp"

/**
 * Target definitions and command line options for the passive submit channel.
 *
 * A target is one gearmand and the envelope to reach it with, shaped like an
 * NSCA target so an installation replacing NSCA with Mod-Gearman changes the
 * section name and little else.
 *
 * The shared secret is spelled `key` in the configuration, matching the core's
 * module.conf and the worker section of this same module, but is stored on the
 * container under `password`. That is not cosmetic: `client::is_sensitive_key`
 * decides both what is masked when a destination is logged and what counts as
 * a credential the request may not redirect to a host of its own choosing
 * (`configuration::check_host_override`), and it recognises a key by its name.
 * Stored as `key` the shared secret would be printed in the clear at trace
 * level and could be pointed at any gearmand a caller named.
 */
namespace gearman_handler {
namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

struct gearman_target_object : public nscapi::targets::target_object {
  typedef nscapi::targets::target_object parent;

  gearman_target_object(const std::string &alias, const std::string &path) : parent(alias, path) {
    set_property_int("timeout", 30);
    set_property_string("port", std::to_string(gearman::default_port));
    set_property_bool("encryption", true);
    set_property_bool("insecure", false);
    set_property_string("queue", gearman::default_result_queue);
  }
  gearman_target_object(const nscapi::settings_objects::object_instance &other, const std::string &alias, const std::string &path)
      : parent(other, alias, path) {}

  void read(const nscapi::settings_helper::settings_impl_interface_ptr proxy, const bool oneliner, const bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    // clang-format off
    root_path.add_key()

        .add_bool("encryption", sh::bool_fun_key([this](auto value) { this->set_property_bool("encryption", value); }, true), "ENCRYPT PAYLOADS",
                  "Whether the result travels inside the AES-256 envelope (mod_gearman's encryption=yes). Leave this on: with it off the payload is plain "
                  "base64 and anyone who can reach gearmand can read and forge the results this agent files. Turning it off also requires 'insecure = true'.")

        .add_bool("insecure", sh::bool_fun_key([this](auto value) { this->set_property_bool("insecure", value); }, false), "ALLOW UNENCRYPTED PAYLOADS",
                  "Acknowledge that 'encryption = false' submits results with no protection at all. Without this a submission to this target is refused "
                  "rather than sent in the clear.")

        .add_password("key", sh::string_fun_key([this](auto value) { this->set_property_string("password", value); }, ""), "SHARED KEY",
                      "The shared password from the core's module.conf (key=), the same value the worker section uses. At most 32 bytes are used, as in "
                      "mod_gearman.")

        .add_string("queue", sh::string_fun_key([this](auto value) { this->set_property_string("queue", value); }, gearman::default_result_queue),
                    "RESULT QUEUE",
                    "The queue the core's result thread reads. Only change this when the core's module.conf names a different result queue; a result on a "
                    "queue nobody reads is silently discarded by gearmand.");
    // clang-format on

    settings.register_all();
    settings.notify();
  }
};

struct options_reader_impl : public client::options_reader_interface {
  nscapi::settings_objects::object_instance create(std::string alias, std::string path) override {
    return std::make_shared<gearman_target_object>(alias, path);
  }
  nscapi::settings_objects::object_instance clone(nscapi::settings_objects::object_instance parent, const std::string alias, const std::string path) override {
    return std::make_shared<gearman_target_object>(parent, alias, path);
  }

  void process(po::options_description &desc, client::destination_container &source, client::destination_container &data) override {
    // clang-format off
    desc.add_options()
      ("key", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("password", value); }),
        "The shared key from the core's module.conf (key=)")
      ("password", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("password", value); }),
        "Same as key")
      ("queue", po::value<std::string>()->notifier([&data](auto value) { data.set_string_data("queue", value); }),
        "The queue the core reads results from (default check_results)")
      ("encryption", po::value<bool>()->implicit_value(true)->notifier([&data](auto value) { data.set_bool_data("encryption", value); }),
        "Whether to wrap the result in the AES-256 envelope (default true)")
      ("insecure", po::value<bool>()->implicit_value(true)->notifier([&data](auto value) { data.set_bool_data("insecure", value); }),
        "Acknowledge sending results unencrypted; required together with encryption=false")
      ;
    // clang-format on
  }
};
}  // namespace gearman_handler
