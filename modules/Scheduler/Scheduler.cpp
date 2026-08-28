// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "Scheduler.h"

#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_convert.hpp>
#include <nscapi/protobuf/functions_status.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_submit.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool Scheduler::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  if (mode == NSCAPI::reloadStart) {
    scheduler_.prepare_shutdown();
    scheduler_.stop();
    // The worker threads are joined at this point, so the task list and the
    // pending queue can be dropped safely. Without this the tasks (and their
    // queued instances) from before the reload survive alongside the ones
    // added below, so every schedule ends up running twice.
    scheduler_.clear();
    schedules_.clear();
  }

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias(alias, "scheduler");
  schedules_.set_path(settings.alias().get_settings_path("schedules"));

  // clang-format off
  settings.alias().add_path_to_settings()
    ("Scheduler", "Section for the Scheduler module.")

    ;

  settings.alias().add_key_to_settings()
    .add_int("threads", sh::int_fun_key([this] (auto value) { scheduler_.set_threads(value); }, 5),
	    "Threads", "Number of threads to use.")
    .add_string("startup window", sh::string_fun_key([this] (const auto& value) { this->set_startup_window(value); }, "0s"),
        "Startup window",
        "Time over which schedules with 'run on startup' are spread out when the agent starts. The default (0s) runs them all immediately; raise it if you "
        "have many startup schedules and do not want to hit the monitoring server with all of them at once.")
    .add_string("timezone", sh::string_fun_key([this] (const auto& value) { scheduler_.set_timezone(value); }, "local"),
        "Timezone",
        "Reference clock for cron expressions. Accepts 'local' (default — standard cron semantics), 'utc'/'gmt' (restores the pre-0.13 "
        "behaviour), or any POSIX TZ string such as 'EST-05EDT,M3.2.0,M11.1.0'. Unparseable values fall back to UTC and are flagged in the "
        "tz label as 'UTC?'.")
    ;

  settings.alias().add_path_to_settings()
    ("schedules", sh::fun_values_path([this] (auto key, auto value) { this->add_schedule(key, value); }),
	    "Schedules", "Section for the Scheduler module.",
	    "SCHEDULE", "For more configuration options add a dedicated section")
    ;

  settings.alias().add_templates()
    ("schedules", "plus", "Add a simple schedule",
	    "Add a simple scheduled job for passive monitoring",
	    "{"
	    "\"fields\": [ "
	    " { \"id\": \"alias\",		\"title\" : \"Alias\",		\"type\" : \"input\",		\"desc\" : \"This will identify the command\"} , "
	    " { \"id\": \"command\",	\"title\" : \"Command\",	\"type\" : \"data-choice\",	\"desc\" : \"The name of the command to execute\",\"exec\" : \"CheckExternalScripts list --json --query\" } , "
	    " { \"id\": \"args\",		\"title\" : \"Arguments\",	\"type\" : \"input\",		\"desc\" : \"Command line arguments for the command\" } , "
	    " { \"id\": \"cmd\",		\"key\" : \"command\", \"title\" : \"A\",	\"type\" : \"hidden\",		\"desc\" : \"A\" } "
	    " ], "
	    "\"events\": { "
	    "\"onSave\": \"(function (node) { node.save_path = self.path; var f = node.get_field('cmd'); f.key = node.get_field('alias').value(); var val = node.get_field('command').value(); if (node.get_field('args').value()) { val += ' ' + node.get_field('args').value(); }; f.value(val)})\""
	    "}"
	    "}")
    ;
  // clang-format on
  settings.register_all();
  settings.notify();

  schedules_.ensure_default(nscapi::settings_proxy::create(get_id(), get_core()));
  schedules_.add_samples(nscapi::settings_proxy::create(get_id(), get_core()));

  for (const schedules::schedule_handler::object_list_type::value_type &o : schedules_.get_object_list()) {
    if (o->duration && o->duration->total_seconds() == 0) {
      NSC_LOG_ERROR("WE cant add schedules with 0 duration: " + o->to_string());
      continue;
    }
    if (o->duration && o->schedule) {
      NSC_LOG_ERROR("WE cant add schedules with both duration and schedule: " + o->to_string());
      continue;
    }
    if (!o->duration && !o->schedule) {
      NSC_LOG_ERROR("WE need wither duration or schedule: " + o->to_string());
      continue;
    }
    NSC_DEBUG_MSG("Adding scheduled item: " + o->to_string());
    scheduler_.add_task(o);
  }

  if (mode == NSCAPI::normalStart) {
    scheduler_.set_handler(this);
    scheduler_.start();
  }
  if (mode == NSCAPI::reloadStart) {
    scheduler_.set_handler(this);
    scheduler_.start();
  }
  // On a normal boot the startup runs wait for startModule, which the core
  // calls once every plugin is loaded - firing them here would query commands
  // that later modules have not registered yet. A reload (or a module loaded
  // into an already running agent) has no such problem and gets no second
  // startModule, so fire them right away instead.
  if (started_ && (mode == NSCAPI::normalStart || mode == NSCAPI::reloadStart)) {
    scheduler_.run_startup_tasks(startup_window_);
  }
  return true;
}

bool Scheduler::startModule() {
  started_ = true;
  scheduler_.run_startup_tasks(startup_window_);
  return true;
}

void Scheduler::set_startup_window(const std::string &value) {
  try {
    startup_window_ = boost::posix_time::seconds(str::format::stox_as_time_sec<long>(value, "s"));
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Invalid 'startup window' value '" + value + "', falling back to 0s: ", e);
    startup_window_ = boost::posix_time::seconds(0);
  }
}

void Scheduler::add_schedule(const std::string &key, const std::string &arg) {
  try {
    schedules_.add(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add target: " + key);
  }
}

void Scheduler::prepareShutdown() {
  // Drain the scheduler while every peer plugin is still loaded so in-flight
  // queries and submissions can complete normally. By the time unloadModule
  // runs the worker threads have already been joined.
  scheduler_.prepare_shutdown();
  scheduler_.stop();
}

bool Scheduler::unloadModule() {
  scheduler_.prepare_shutdown();
  scheduler_.stop();
  schedules_.clear();
  return true;
}

void Scheduler::on_error(const char *file, const int line, std::string msg) { GET_CORE()->log(NSCAPI::log_level::error, file, line, msg); }
void Scheduler::on_trace(const char *file, const int line, std::string msg) { GET_CORE()->log(NSCAPI::log_level::trace, file, line, msg); }

bool Scheduler::handle_schedule(const schedules::target_object item) {
  try {
    std::string error;
    // Errors are logged inside run_schedule; the timer path keeps rescheduling
    // regardless so a single failing check does not silently drop out of the
    // schedule for the lifetime of the process.
    run_schedule(item, forwarded_identity(), error);
    return true;
  } catch (nsclient::nsclient_exception &e) {
    NSC_LOG_ERROR_EXR("Failed to register command: ", e);
    return false;
  } catch (std::exception &e) {
    NSC_LOG_ERROR_EXR("Exception: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX(item->get_alias());
    return false;
  }
}

Scheduler::forwarded_identity Scheduler::extract_identity(const PB::Commands::QueryRequestMessage &request_message) {
  // The same two metadata keys core_helper stamps and plugin_manager reads
  // back when it evaluates the permission rules.
  forwarded_identity id;
  for (const auto &kv : request_message.header().metadata()) {
    if (kv.key() == "nscp.caller_plugin_id")
      id.caller_plugin_id = kv.value();
    else if (kv.key() == "nscp.principal")
      id.principal = kv.value();
  }
  return id;
}

bool Scheduler::run_schedule(const schedules::target_object &item, const forwarded_identity &id, std::string &error) {
  std::string response;
  nscapi::core_helper ch(get_core(), get_id());
  // Forward the caller identity (empty for our own timer runs) so an on-demand
  // run is checked against the permissions of whoever asked for it rather than
  // against the Scheduler module.
  if (!ch.simple_query_on_behalf_of(id.caller_plugin_id, id.principal, item->command, item->arguments, response)) {
    error = "Command was not found: " + item->command;
    NSC_LOG_ERROR("Failed to execute: " + item->command);
    if (item->channel.empty()) {
      NSC_LOG_ERROR_WA("No channel specified for ", item->get_alias());
      return false;
    }
    nscapi::protobuf::functions::create_simple_submit_request(item->channel, item->command, NSCAPI::query_return_codes::returnUNKNOWN, error, "", response);
    std::string result;
    get_core()->submit_message(item->channel, response, result);
    return false;
  }
  PB::Commands::QueryResponseMessage resp_msg;
  resp_msg.ParseFromString(response);
  PB::Commands::QueryResponseMessage resp_msg_send;
  resp_msg_send.mutable_header()->CopyFrom(resp_msg.header());
  for (const PB::Commands::QueryResponseMessage::Response &p : resp_msg.payload()) {
    if (nscapi::report::matches(item->report, nscapi::protobuf::functions::gbp_to_nagios_status(p.result()))) resp_msg_send.add_payload()->CopyFrom(p);
  }
  if (resp_msg_send.payload_size() == 0) {
    NSC_DEBUG_MSG("Filter not matched for: " + item->get_alias() + " so nothing is reported");
    return true;
  }
  if (item->channel == "drop") {
    return true;
  }
  if (item->channel.empty()) {
    error = "No channel specified for " + item->get_alias();
    NSC_LOG_ERROR_STD(error + " message will not be sent.");
    return false;
  }
  nscapi::protobuf::functions::make_submit_from_query(response, item->channel, item->get_alias(), item->target_id, item->source_id);
  std::string result;
  if (!get_core()->submit_message(item->channel, response, result)) {
    error = "Failed to submit: " + item->get_alias();
    NSC_LOG_ERROR_STD(error);
    return false;
  }
  if (!nscapi::protobuf::functions::parse_simple_submit_response(result, error)) {
    NSC_LOG_ERROR_STD("Failed to submit " + item->get_alias() + ": " + error);
    return false;
  }
  return true;
}

void Scheduler::run_schedules(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response,
                              const PB::Commands::QueryRequestMessage &request_message) {
  po::options_description desc = nscapi::program_options::create_desc(request);
  std::vector<std::string> wanted;
  // clang-format off
  desc.add_options()
    ("schedule", po::value<std::vector<std::string> >(&wanted),
      "Alias of a schedule to run, can be given more than once. Defaults to every configured schedule.")
    ;
  // clang-format on
  po::variables_map vm;
  // Deliberately no positional option: it would make the key=value token parser
  // break at the literal token "schedule", which in turn breaks the CLI's
  // `--argument schedule=<alias>` form (it passes an undashed key=value pair
  // alongside the dashed one). Aliases are given one per `schedule=` instead.
  if (!nscapi::program_options::process_arguments_from_request(vm, desc, request, *response)) return;

  std::list<schedules::target_object> all = scheduler_.get_all();
  // The scheduler keeps its schedules in an unordered map, so sort them to get
  // a stable order in the message (and to run them in a predictable one).
  all.sort([](const schedules::target_object &lhs, const schedules::target_object &rhs) { return lhs->get_alias() < rhs->get_alias(); });
  if (all.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "No schedules configured");
  }

  std::list<schedules::target_object> selected;
  if (wanted.empty()) {
    selected = all;
  } else {
    for (const std::string &alias : wanted) {
      bool found = false;
      for (const schedules::target_object &item : all) {
        if (item->get_alias() == alias) {
          selected.push_back(item);
          found = true;
        }
      }
      if (!found) {
        std::string available;
        for (const schedules::target_object &item : all) str::format::append_list(available, item->get_alias());
        return nscapi::protobuf::functions::set_response_bad(*response, "No such schedule: " + alias + " (available: " + available + ")");
      }
    }
  }

  std::string ran, failed;
  for (const schedules::target_object &item : selected) {
    std::string error;
    bool ok;
    try {
      ok = run_schedule(item, extract_identity(request_message), error);
    } catch (const std::exception &e) {
      ok = false;
      error = utf8::utf8_from_native(e.what());
    } catch (...) {
      ok = false;
      error = "Unknown exception";
    }
    if (ok) {
      str::format::append_list(ran, item->get_alias());
    } else {
      str::format::append_list(failed, item->get_alias() + " (" + error + ")");
    }
  }
  if (!failed.empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, "Failed to run: " + failed + (ran.empty() ? "" : ", ran: " + ran));
  }
  nscapi::protobuf::functions::set_response_good(*response, "Ran " + str::xtos(selected.size()) + " schedule(s): " + ran);
}

void Scheduler::fetchMetrics(PB::Metrics::MetricsMessage::Response *response) {
  PB::Metrics::MetricsBundle *bundle = response->add_bundles();
  bundle->set_key("scheduler");
  if (scheduler_.get_scheduler().has_metrics()) {
    const auto tasks = scheduler_.get_scheduler().get_metric_executed();
    const auto submitted = scheduler_.get_scheduler().get_metric_compleated();
    const auto errors = scheduler_.get_scheduler().get_metric_errors();
    const auto threads = scheduler_.get_scheduler().get_metric_threads();
    const auto queue = scheduler_.get_scheduler().get_metric_ql();
    const auto avg_time = scheduler_.get_scheduler().get_avg_time();
    const auto rate = scheduler_.get_scheduler().get_metric_rate();

    PB::Metrics::Metric *m = bundle->add_value();
    m->set_key("jobs");
    m->mutable_gauge_value()->set_value(static_cast<double>(tasks));
    m = bundle->add_value();
    m->set_key("submitted");
    m->mutable_gauge_value()->set_value(static_cast<double>(submitted));
    m = bundle->add_value();
    m->set_key("errors");
    m->mutable_gauge_value()->set_value(static_cast<double>(errors));
    m = bundle->add_value();
    m->set_key("threads");
    m->mutable_gauge_value()->set_value(static_cast<double>(threads));
    m = bundle->add_value();
    m->set_key("queue");
    m->mutable_gauge_value()->set_value(static_cast<double>(queue));
    m = bundle->add_value();
    m->set_key("avgtime");
    m->mutable_gauge_value()->set_value(static_cast<double>(avg_time));
    m = bundle->add_value();
    m->set_key("rate");
    m->mutable_gauge_value()->set_value(static_cast<double>(rate));
  } else {
    PB::Metrics::Metric *m = bundle->add_value();
    m->set_key("metrics.available");
    m->mutable_gauge_value()->set_value(0);
  }
}