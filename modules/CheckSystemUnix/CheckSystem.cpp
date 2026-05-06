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

#include "CheckSystem.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <locale>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/format.hpp>

#include "check_cpu.h"
#include "check_memory.h"
#include "check_os_updates.h"
#include "check_os_version.h"
#include "check_pagefile.h"
#include "check_process.h"
#include "check_service.h"
#include "check_uptime.h"

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

/**
 * New version of the load call.
 * Start the background collector thread and let it run until unloadModule() is called.
 * @return true
 */
bool CheckSystem::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("system", alias, "unix");

  // Start the CPU collector thread
  collector_ = boost::shared_ptr<pdh_thread>(new pdh_thread());
  collector_->set_core(get_core(), get_id());
  collector_->set_path(settings.alias().get_settings_path("real-time/cpu"), settings.alias().get_settings_path("real-time/memory"));

  // clang-format off
  settings.alias().add_path_to_settings()
    ("Unix system", "Section for system checks and system settings")

    ("real-time/cpu", sh::fun_values_path([this] (auto key, auto value) { collector_->add_realtime_cpu_filter(nscapi::settings_proxy::create(get_id(), get_core()), key, value); }),
        "Realtime cpu filters", "A set of filters to use in real-time mode",
        "FILTER", "For more configuration options add a dedicated section")

    ("real-time/memory", sh::fun_values_path([this] (auto key, auto value) { collector_->add_realtime_mem_filter(nscapi::settings_proxy::create(get_id(), get_core()), key, value); }),
        "Realtime memory filters", "A set of filters to use in real-time mode",
        "FILTER", "For more configuration options add a dedicated section")
    ;

  settings.alias().add_key_to_settings()
    .add_string("default buffer length", sh::string_key(&collector_->default_buffer_size, "1h"),
        "Default buffer time", "Used to define the default size of range buffer checks (ie. CPU).")
    ;
  // clang-format on

  settings.register_all();
  settings.notify();

  collector_->add_samples(nscapi::settings_proxy::create(get_id(), get_core()));

  if (mode == NSCAPI::normalStart) {
    collector_->start();
  }

  return true;
}

/**
 * Unload (terminate) module.
 * Attempt to stop the background processing thread.
 * @return true if successfully, false if not (if not things might be bad)
 */
bool CheckSystem::unloadModule() {
  if (collector_) {
    collector_->stop();
    collector_.reset();
  }
  return true;
}

void CheckSystem::check_service(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_service(request, response);
}
void CheckSystem::check_memory(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_memory::check_memory(collector_, request, response);
}
void CheckSystem::check_process(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_proc::check_process(request, response);
}
void CheckSystem::check_cpu(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_cpu(collector_, request, response);
}
void CheckSystem::check_uptime(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  checks::check_uptime(request, response);
}
void CheckSystem::check_pagefile(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  check_page::check_pagefile(collector_, request, response);
}
void CheckSystem::check_os_version(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  os_version::check_os_version(request, response);
}
void CheckSystem::check_os_updates(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  os_updates::check_os_updates(request, response);
}

namespace {
bool read_uptime_seconds(double &uptime_secs) {
  try {
    std::locale c_locale("C");
    std::ifstream f;
    f.imbue(c_locale);
    f.open("/proc/uptime");
    if (!f.is_open()) return false;
    double idle = 0;
    f >> uptime_secs >> idle;
    return f.good() || f.eof();
  } catch (...) {
    return false;
  }
}
}  // namespace

void CheckSystem::fetchMetrics(PB::Metrics::MetricsMessage::Response *response) {
  using namespace nscapi::metrics;

  PB::Metrics::MetricsBundle *bundle = response->add_bundles();
  bundle->set_key("system");
  add_metric(bundle, "refresh_interval", 1ll);

  if (!collector_) return;

  // CPU metrics: system.cpu.<core>.{idle,user,kernel,total}
  try {
    PB::Metrics::MetricsBundle *cpu = bundle->add_children();
    cpu->set_key("cpu");
    if (collector_->has_cpu_data()) {
      const auto vals = collector_->get_cpu_load(5);
      for (const auto &v : vals) {
        const load_entry &load = v.second;
        std::string name = v.first;
        // Normalize "core 0" -> "core_0" (web UI prefers underscore-separated keys)
        std::replace(name.begin(), name.end(), ' ', '_');
        add_metric(cpu, name + ".idle", load.idle);
        add_metric(cpu, name + ".user", load.user);
        add_metric(cpu, name + ".kernel", load.kernel);
        add_metric(cpu, name + ".total", load.user + load.kernel);
      }
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR(std::string("Failed to fetch cpu metrics: ") + e.what());
  } catch (...) {
    NSC_LOG_ERROR("Failed to fetch cpu metrics");
  }

  // Memory metrics: system.mem.{physical,cached,swap}.{total,avail,used,%}
  try {
    PB::Metrics::MetricsBundle *mem = bundle->add_children();
    mem->set_key("mem");
    if (collector_->has_memory_data()) {
      const memory_info m = collector_->get_memory(1);
      auto add_mem_section = [&](const std::string &prefix, unsigned long long total, unsigned long long avail) {
        const unsigned long long used = total > avail ? total - avail : 0;
        add_metric(mem, prefix + ".total", total);
        add_metric(mem, prefix + ".avail", avail);
        add_metric(mem, prefix + ".used", used);
        add_metric(mem, prefix + ".%", str::format::calc_pct_round(used, total));
      };
      add_mem_section("physical", m.physical.total, m.physical.free);
      add_mem_section("cached", m.cached.total, m.cached.free);
      add_mem_section("swap", m.swap.total, m.swap.free);
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR(std::string("Failed to fetch memory metrics: ") + e.what());
  } catch (...) {
    NSC_LOG_ERROR("Failed to fetch memory metrics");
  }

  // Uptime metrics: system.uptime.{uptime,boot,ticks.raw,boot.raw}
  try {
    PB::Metrics::MetricsBundle *up = bundle->add_children();
    up->set_key("uptime");
    double uptime_secs = 0;
    if (read_uptime_seconds(uptime_secs)) {
      const auto value = static_cast<unsigned long long>(uptime_secs);
      const boost::posix_time::ptime now = boost::posix_time::second_clock::universal_time();
      const boost::posix_time::ptime boot = now - boost::posix_time::time_duration(0, 0, static_cast<long>(value));
      add_metric(up, "ticks.raw", static_cast<long long>(value));
      add_metric(up, "boot.raw", static_cast<long long>(value));
      add_metric(up, "uptime", str::format::itos_as_time(value * 1000));
      add_metric(up, "boot", str::format::format_date(boot));
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR(std::string("Failed to fetch uptime metrics: ") + e.what());
  } catch (...) {
    NSC_LOG_ERROR("Failed to fetch uptime metrics");
  }
}
