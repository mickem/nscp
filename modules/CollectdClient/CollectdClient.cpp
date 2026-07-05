// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CollectdClient.h"

#include <boost/regex.hpp>
#include <memory>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/utils.hpp>

#include "collectd_client.hpp"
#include "collectd_handler.hpp"

/**
 * Default c-tor
 * @return
 */
CollectdClient::CollectdClient()
    : handler_(std::make_shared<collectd_client::collectd_client_handler>()),
      client_("collectd", handler_, std::make_shared<collectd_handler::options_reader_impl>()) {}

/**
 * Default d-tor
 * @return
 */
CollectdClient::~CollectdClient() {}

bool CollectdClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("collectd", alias, "client");
    std::string target_path = settings.alias().get_settings_path("targets");

    client_.set_path(target_path);

    unsigned int interval = 10;

    // clang-format off
    settings.alias().add_path_to_settings()
      ("COLLECTD CLIENT SECTION", "Section for the collectd client; forwards NSClient++ metrics to a collectd server.")

      ("targets", sh::fun_values_path([this] (auto key, auto value) { this->add_target(key, value); }),
	      "REMOTE TARGET DEFINITIONS", "",
	      "TARGET", "For more configuration options add a dedicated section")

      ("variables", sh::fun_values_path([this] (auto key, auto value) { this->handler_->add_variable(key, value); }),
	      "VARIABLE DEFINITIONS",
	      "Variables used to expand ${...} placeholders in metric keys. Each value is a regular expression matched against metric names; "
	      "the captured groups become the variable's values. When empty a built-in default set is used.")

      ("metrics", sh::fun_values_path([this] (auto key, auto value) { this->handler_->add_metric(key, value); }),
	      "METRIC MAPPINGS",
	      "Mapping of collectd keys (e.g. cpu-total/cpu-user) to value expressions (e.g. derive:system.cpu.total.user). "
	      "When empty a built-in default set is used.")
      ;

    settings.alias().add_key_to_settings()
      .add_string("hostname", sh::string_key(&hostname_, "auto"), "HOSTNAME",
        "The host name reported to collectd.\nSet this to auto (default) to use the name of this computer.\n\n"
        "auto\tHostname\n"
        "${host}\tHostname\n"
        "${host_lc}\tHostname in lowercase\n"
        "${host_uc}\tHostname in uppercase\n"
        "${domain}\tDomainname\n"
        "${domain_lc}\tDomainname in lowercase\n"
        "${domain_uc}\tDomainname in uppercase\n")

      .add_int("interval", sh::uint_key(&interval, 10), "METRICS INTERVAL",
        "The interval (in seconds) reported to collectd. Should match the core 'metrics interval' so collectd computes rates correctly.")
      ;
    // clang-format on

    settings.register_all();
    settings.notify();

    handler_->set_interval(interval);

    client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

    nscapi::core_helper core(get_core(), get_id());

    hostname_ = socket_helpers::expand_hostname(hostname_);
    client_.set_sender(hostname_);
  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("NSClient API exception: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("loading", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading");
    return false;
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
// Settings helpers
//

void CollectdClient::add_target(std::string key, std::string arg) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CollectdClient::unloadModule() {
  client_.clear();
  return true;
}

void CollectdClient::submitMetrics(const PB::Metrics::MetricsMessage &response) { client_.do_metrics(response); }
