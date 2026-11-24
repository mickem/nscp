// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_translate.hpp"

#include <gtest/gtest.h>
#include <map>
#include <vector>

#include "icinga_agent_execute.hpp"

namespace json = boost::json;
using namespace icinga_agent;

namespace {
typedef std::list<std::string> args;

translated_command builtin(const std::string &command, const char *macros) {
  const boost::optional<translated_command> t = translate_command(translation_config(), command, json::parse(macros).as_object());
  EXPECT_TRUE(static_cast<bool>(t)) << command;
  return t ? t.value() : translated_command();
}
}  // namespace

// The macros are the ones a master sends: what the service set, on top of the
// defaults the ITL command carries (disk_win_unit = "mb", 20% / 10%).
TEST(IcingaAgentTranslate, DiskWindows) {
  translated_command t = builtin("disk-windows", R"({"disk_win_unit":"mb","disk_win_warn":"20%","disk_win_crit":"10%"})");
  EXPECT_EQ(t.query, "check_drivesize");
  EXPECT_EQ(t.arguments, (args{"filter=mounted = 1 and type in ('fixed', 'remote')", "w=@~:20%", "c=@~:10%"}));
  EXPECT_FALSE(t.fixed_arguments);

  // Paths, excludes, and a bare number in the command's unit.
  t = builtin("disk-windows", R"({"disk_win_unit":"gb","disk_win_warn":"50","disk_win_crit":"![5-1000]",
                                  "disk_win_path":["C:","D:"],"disk_win_exclude":"E:"})");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "drive=D:", "exclude=E:", "w=@~:50g", "c=@5g:1000g"}));

  // -U: the same thresholds against the space in use, the plain way round.
  t = builtin("disk-windows", R"({"disk_win_unit":"mb","disk_win_warn":"80%","disk_win_crit":"90%","disk_win_show_used":true,"disk_win_path":"C:"})");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "warning=used > 80%", "critical=used > 90%"}));

  // A side without a threshold never alerted, and must not start to.
  t = builtin("disk-windows", R"({"disk_win_unit":"mb","disk_win_crit":"10%","disk_win_path":"C:"})");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "w=none", "c=@~:10%"}));

  // Without -u the plugin counts bytes.
  t = builtin("disk-windows", R"({"disk_win_warn":"1000","disk_win_path":"C:"})");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "w=@~:1000b", "c=none"}));
}

TEST(IcingaAgentTranslate, MemoryAndSwapWindows) {
  translated_command t = builtin("memory-windows", R"({"memory_win_unit":"mb","memory_win_warn":"10%","memory_win_crit":"512"})");
  EXPECT_EQ(t.query, "check_memory");
  EXPECT_EQ(t.arguments, (args{"type=physical", "w=@~:10%", "c=@~:512m"}));

  // With -U check_memory.exe forces the plain reading: the `!` is ignored.
  t = builtin("memory-windows", R"({"memory_win_unit":"mb","memory_win_warn":"!90%","memory_win_show_used":true})");
  EXPECT_EQ(t.arguments, (args{"type=physical", "warning=used > 90%", "c=none"}));

  t = builtin("swap-windows", R"({"swap_win_unit":"mb","swap_win_warn":"10%","swap_win_crit":"5%"})");
  EXPECT_EQ(t.query, "check_pagefile");
  EXPECT_EQ(t.arguments, (args{"filter=name = 'total'", "w=@~:10%", "c=@~:5%"}));
}

TEST(IcingaAgentTranslate, LoadWindows) {
  translated_command t = builtin("load-windows", R"({"load_win_warn":80,"load_win_crit":"95%"})");
  EXPECT_EQ(t.query, "check_cpu");
  EXPECT_EQ(t.arguments, (args{"w=~:80", "c=~:95"}));
  // check_load.exe reads the first of a list; no thresholds, no alert.
  t = builtin("load-windows", R"({"load_win_warn":"80,90,95"})");
  EXPECT_EQ(t.arguments, (args{"w=~:80", "c=none"}));
}

TEST(IcingaAgentTranslate, CountsAndTimes) {
  translated_command t = builtin("procs-windows", R"({"procs_win_warn":300,"procs_win_crit":"!1","procs_win_user":"svc_sql"})");
  EXPECT_EQ(t.query, "check_process");
  EXPECT_EQ(t.arguments, (args{"process=*", "resolve-owner=true", "filter=username = 'svc_sql'", "warning=count > 300", "critical=count <= 1",
                            "top-syntax=${status}: ${count} processes"}));

  t = builtin("users-windows", R"({"users_win_warn":5})");
  EXPECT_EQ(t.query, "check_users");
  EXPECT_EQ(t.arguments, (args{"w=~:5", "c=none"}));

  t = builtin("uptime-windows", R"({"uptime_win_unit":"h","uptime_win_warn":720,"uptime_win_crit":"!1"})");
  EXPECT_EQ(t.query, "check_uptime");
  EXPECT_EQ(t.arguments, (args{"w=~:720h", "c=@~:1h"}));

  t = builtin("network-windows", R"({"network_no_isatap":true,"network_win_warn":1000000})");
  EXPECT_EQ(t.query, "check_network");
  EXPECT_EQ(t.arguments, (args{"w=~:1000000", "c=none"}));

  t = builtin("perfmon-windows", R"({"perfmon_win_counter":"\\Processor(_Total)\\% Idle Time","perfmon_win_type":"double","perfmon_win_crit":"!10"})");
  EXPECT_EQ(t.query, "check_pdh");
  EXPECT_EQ(t.arguments, (args{"counter=\\Processor(_Total)\\% Idle Time", "type=double", "w=none", "c=@~:10"}));
}

TEST(IcingaAgentTranslate, PingWindows) {
  translated_command t = builtin("ping4-windows", R"({"ping_win_address":"192.0.2.1","ping_win_packets":"5","ping_win_timeout":"1000",
                                                     "ping_win_wrta":100,"ping_win_wpl":5,"ping_win_crta":200,"ping_win_cpl":15})");
  EXPECT_EQ(t.query, "check_ping");
  EXPECT_EQ(t.arguments, (args{"host=192.0.2.1", "count=5", "timeout=1000", "w=~:100,~:5%", "c=~:200,~:15%"}));
  // Icinga drops "-w" unless both of its macros are set.
  t = builtin("ping-windows", R"({"ping_win_address":"192.0.2.1","ping_win_wrta":100})");
  EXPECT_EQ(t.arguments, (args{"host=192.0.2.1", "w=none", "c=none"}));
}

TEST(IcingaAgentTranslate, ServiceWindows) {
  translated_command t = builtin("service-windows", R"({"service_win_service":"Spooler"})");
  EXPECT_EQ(t.query, "check_service");
  EXPECT_EQ(t.arguments, (args{"service=Spooler", "warning=none", "critical=state != 'running'"}));
  t = builtin("service-windows", R"({"service_win_service":"Spooler","service_win_warn":true})");
  EXPECT_EQ(t.arguments, (args{"service=Spooler", "warning=state != 'running'", "critical=none"}));
}

TEST(IcingaAgentTranslate, UpdateWindows) {
  // A reboot-requiring update is critical on its own; nothing else is bound.
  translated_command t = builtin("update-windows", "{}");
  EXPECT_EQ(t.query, "check_os_updates");
  EXPECT_EQ(t.arguments, (args{"warning=none", "critical=reboot_required > 0"}));
  // The critical bound defaults to one more than the warning.
  t = builtin("update-windows", R"({"update_win_warn":5})");
  EXPECT_EQ(t.arguments, (args{"warning=updates >= 5", "critical=reboot_required > 0 or updates >= 6"}));
  t = builtin("update-windows", R"({"update_win_warn":5,"update_win_crit":20,"ignore_reboot":true})");
  EXPECT_EQ(t.arguments, (args{"warning=updates >= 5", "critical=updates >= 20"}));
  t = builtin("update-windows", R"({"ignore_reboot":true})");
  EXPECT_EQ(t.arguments, (args{"warning=none", "critical=none"}));
}

TEST(IcingaAgentTranslate, NscpLocal) {
  // The generic command: the query and its arguments as they are.
  translated_command t = builtin("nscp-local", R"({"nscp_query":"check_drivesize","nscp_arguments":["drive=C:","warning=free < 20%"],
                                                  "nscp_showall":false,"nscp_boot":true,"nscp_modules":["CheckDisk"],"nscp_log_level":"critical"})");
  EXPECT_EQ(t.query, "check_drivesize");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "warning=free < 20%"}));

  // The ITL's own defaults for nscp-local-cpu.
  t = builtin("nscp-local-cpu", R"({"nscp_query":"check_cpu","nscp_showall":true,"nscp_cpu_time":["1m","5m","15m"],
                                    "nscp_cpu_warning":80,"nscp_cpu_critical":90})");
  EXPECT_EQ(t.query, "check_cpu");
  EXPECT_EQ(t.arguments, (args{"critical=load>90", "show-all", "time=1m", "time=5m", "time=15m", "warning=load>80"}));

  // nscp_memory_op is a function on the master; it arrives resolved.
  t = builtin("nscp-local-memory", R"({"nscp_memory_physical":true,"nscp_memory_committed":false,"nscp_memory_op":"free <",
                                       "nscp_memory_warning":20,"nscp_memory_critical":10,"nscp_showall":false})");
  EXPECT_EQ(t.query, "check_memory");
  EXPECT_EQ(t.arguments, (args{"critical=free < 10", "type=physical", "warning=free < 20"}));

  t = builtin("nscp-local-uptime", "{}");
  EXPECT_EQ(t.query, "check_uptime");
  EXPECT_TRUE(t.arguments.empty());
}

TEST(IcingaAgentTranslate, RefusesWhatItCannotTranslate) {
  const translation_config config;
  const auto run = [&config](const std::string &command, const char *macros) { return translate_command(config, command, json::parse(macros).as_object()); };
  EXPECT_THROW(run("disk-windows", R"({"disk_win_warn":"lots"})"), translation_error);
  EXPECT_THROW(run("disk-windows", R"({"disk_win_warn":"20%","disk_win_unit":"parsecs"})"), translation_error);
  EXPECT_THROW(run("service-windows", "{}"), translation_error);
  EXPECT_THROW(run("service-windows", R"({"service_win_service":"Print Spooler","service_win_description":true})"), translation_error);
  EXPECT_THROW(run("uptime-windows", R"({"uptime_win_unit":"ms","uptime_win_warn":5})"), translation_error);
  EXPECT_THROW(run("procs-windows", R"({"procs_win_user":"o'brien"})"), translation_error);
  EXPECT_THROW(run("update-windows", R"({"update_win_warn":"many"})"), translation_error);
  EXPECT_THROW(run("nscp-local", "{}"), translation_error);
}

TEST(IcingaAgentTranslate, Aliases) {
  translation_config config;
  config.aliases["disk-windows"] = command_alias{"check_drivesize", {"drive=$disk_win_path$", "warning=free < $disk_win_warn$", "$extra$", "show-all"}};
  config.aliases["my-check"] = command_alias{"check_ok", {"message=fixed"}};
  config.aliases["by-macro"] = command_alias{"$query$", {}};

  // An alias wins over the built-in of the same name. A macro without a value
  // drops its argument; a lone list macro is one argument per element.
  boost::optional<translated_command> t =
      translate_command(config, "disk-windows", json::parse(R"({"disk_win_path":["C:","D:"],"extra":["a=1","b=2"]})").as_object());
  ASSERT_TRUE(static_cast<bool>(t));
  EXPECT_EQ(t.value().query, "check_drivesize");
  EXPECT_EQ(t.value().arguments, (args{"drive=C:,D:", "a=1", "b=2", "show-all"}));
  EXPECT_FALSE(t.value().fixed_arguments);

  t = translate_command(config, "my-check", json::object());
  ASSERT_TRUE(static_cast<bool>(t));
  EXPECT_TRUE(t.value().fixed_arguments);

  EXPECT_THROW(translate_command(config, "by-macro", json::object()), translation_error);
  EXPECT_EQ(translate_command(config, "by-macro", json::object{{"query", "check_cpu"}}).value().query, "check_cpu");
}

TEST(IcingaAgentTranslate, OnlyKnownCommands) {
  EXPECT_FALSE(translate_command(translation_config(), "check_cpu", json::object()));
  translation_config off;
  off.builtin = false;
  EXPECT_FALSE(translate_command(off, "disk-windows", json::object()));
  EXPECT_EQ(builtin_translations().size(), 26u);
}

// Through parse_execute_command, as the agent calls it.
TEST(IcingaAgentTranslate, InAnExecuteCommand) {
  const translation_config config;
  query_request req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"load-windows","host":"h","service":"load",
      "macros":{"load_win_warn":80,"load_win_crit":90}})"), config);
  EXPECT_TRUE(req.error.empty()) << req.error;
  EXPECT_EQ(req.query, "check_cpu");
  EXPECT_EQ(req.arguments, (args{"w=~:80", "c=~:90"}));

  // An ifw-api command names its query itself, whatever it is called.
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"load-windows","host":"h",
      "macros":{"ifw_api_command":"check_uptime"}})"), config);
  EXPECT_EQ(req.query, "check_uptime");
  EXPECT_TRUE(req.arguments.empty());

  // Anything else still runs under its own name.
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"check_uptime","host":"h","macros":{}})"), config);
  EXPECT_EQ(req.query, "check_uptime");

  // A use that cannot be translated is an error on the check, not a guess.
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"disk-windows","host":"h","macros":{"disk_win_warn":"lots"}})"),
                              config);
  EXPECT_NE(req.error.find("Cannot translate the Icinga command 'disk-windows'"), std::string::npos) << req.error;

  // Without a translation table the name is the query, as before.
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"disk-windows","host":"h","macros":{}})"));
  EXPECT_EQ(req.query, "disk-windows");
}

TEST(IcingaAgentTranslate, UnitsAreReadAsThePluginsReadThem) {
  // parseBUnit() upper-cases; so B, kB, MB, GB and TB come in any case.
  for (const auto &unit : std::vector<std::pair<std::string, std::string>>{{"B", "b"}, {"kB", "k"}, {"KB", "k"}, {"MB", "m"}, {"Gb", "g"}, {"tb", "t"}}) {
    json::object macros{{"disk_win_path", "C:"}, {"disk_win_warn", "5"}, {"disk_win_unit", unit.first}};
    const boost::optional<translated_command> t = translate_command(translation_config(), "disk-windows", macros);
    ASSERT_TRUE(static_cast<bool>(t));
    EXPECT_EQ(t.value().arguments, (args{"drive=C:", "w=@~:5" + unit.second, "c=none"})) << unit.first;
  }
  // The ITL always sends a unit for memory and swap; without one the plugins
  // count megabytes.
  translated_command t = builtin("memory-windows", R"({"memory_win_warn":"512"})");
  EXPECT_EQ(t.arguments, (args{"type=physical", "w=@~:512m", "c=none"}));
  t = builtin("swap-windows", R"({"swap_win_crit":"![100-200]"})");
  EXPECT_EQ(t.arguments, (args{"filter=name = 'total'", "w=none", "c=@100m:200m"}));
  t = builtin("uptime-windows", R"({"uptime_win_unit":"M","uptime_win_warn":90})");
  EXPECT_EQ(t.arguments, (args{"w=~:90m", "c=none"}));
  // No unit: check_uptime.exe counts seconds.
  t = builtin("uptime-windows", R"({"uptime_win_crit":"[0-600]"})");
  EXPECT_EQ(t.arguments, (args{"w=none", "c=@0s:600s"}));
}

TEST(IcingaAgentTranslate, ShowUsedOnDiskMemoryAndSwap) {
  // check_disk.exe keeps the `!` with -U...
  translated_command t = builtin("disk-windows", R"({"disk_win_unit":"gb","disk_win_path":"C:","disk_win_show_used":true,
                                                    "disk_win_warn":"!10","disk_win_crit":"[100-200]"})");
  EXPECT_EQ(t.arguments, (args{"drive=C:", "warning=used <= 10g", "critical=used >= 100g and used <= 200g"}));
  // ...check_memory.exe and check_swap.exe drop it.
  t = builtin("swap-windows", R"({"swap_win_unit":"mb","swap_win_show_used":true,"swap_win_warn":"!80%","swap_win_crit":"![10-20]"})");
  EXPECT_EQ(t.arguments, (args{"filter=name = 'total'", "warning=used > 80%", "critical=used >= 10m and used <= 20m"}));
  // "0", false and "false" are not -U.
  for (const char *off : {R"("0")", "false", R"("false")", "0"}) {
    const std::string macros = std::string(R"({"memory_win_unit":"mb","memory_win_warn":"10%","memory_win_show_used":)") + off + "}";
    t = builtin("memory-windows", macros.c_str());
    EXPECT_EQ(t.arguments, (args{"type=physical", "w=@~:10%", "c=none"})) << off;
  }
}

TEST(IcingaAgentTranslate, PingThresholdForms) {
  // Each half is a threshold of its own, so it can be negated or a range.
  translated_command t = builtin("ping-windows", R"({"ping_win_address":"h","ping_win_wrta":"[50-100]","ping_win_wpl":"!10",
                                                    "ping_win_crta":"200.5","ping_win_cpl":"50"})");
  EXPECT_EQ(t.arguments, (args{"host=h", "w=@50:100,@~:10%", "c=~:200.5,~:50%"}));
  // The ITL appends the % itself, so a value that carries one is not a number.
  EXPECT_THROW(translate_command(translation_config(), "ping-windows",
                                 json::parse(R"({"ping_win_address":"h","ping_win_wrta":100,"ping_win_wpl":"5%"})").as_object()),
               translation_error);
  EXPECT_THROW(translate_command(translation_config(), "ping-windows", json::parse(R"({"ping_win_wrta":100,"ping_win_wpl":5})").as_object()),
               translation_error);
}

TEST(IcingaAgentTranslate, PerfmonWindows) {
  translated_command t = builtin("perfmon-windows", R"({"perfmon_win_counter":"\\Memory\\Pages/sec","perfmon_win_type":"int64",
                                                       "perfmon_win_warn":"[100-200]","perfmon_win_wait":1000,"perfmon_win_syntax":"pages"})");
  // int64 is the query's `large`; the wait and the label have no counterpart.
  EXPECT_EQ(t.arguments, (args{"counter=\\Memory\\Pages/sec", "type=large", "w=@100:200", "c=none"}));
  EXPECT_THROW(translate_command(translation_config(), "perfmon-windows", json::parse(R"({"perfmon_win_warn":5})").as_object()), translation_error);
}

TEST(IcingaAgentTranslate, UpdateWindowsZeroMeansNoBound) {
  // check_update.exe tests "count >= warn && warn": a zero is no threshold.
  translated_command t = builtin("update-windows", R"({"update_win_warn":0,"update_win_crit":0,"ignore_reboot":true})");
  EXPECT_EQ(t.arguments, (args{"warning=none", "critical=none"}));
  // A critical bound alone.
  t = builtin("update-windows", R"({"update_win_crit":"10"})");
  EXPECT_EQ(t.arguments, (args{"warning=none", "critical=reboot_required > 0 or updates >= 10"}));
  // update_win_reboot has no counterpart and changes nothing.
  t = builtin("update-windows", R"({"update_win_warn":3,"update_win_reboot":true})");
  EXPECT_EQ(t.arguments, (args{"warning=updates >= 3", "critical=reboot_required > 0 or updates >= 4"}));
}

TEST(IcingaAgentTranslate, NscpLocalFamily) {
  translated_command t = builtin("nscp-local-service", R"({"nscp_query":"check_service","nscp_showall":true,"nscp_service_name":["Spooler","W32Time"],
      "nscp_service_otype":"state","nscp_service_wtype":"state","nscp_service_ctype":"state","nscp_service_critical":"stopped"})");
  EXPECT_EQ(t.query, "check_service");
  // No ok/warning value: Icinga drops those arguments, and so does this.
  EXPECT_EQ(t.arguments, (args{"critical=state='stopped'", "service=Spooler", "service=W32Time", "show-all"}));

  t = builtin("nscp-local-disk", R"({"nscp_query":"check_drivesize","nscp_showall":true,"nscp_disk_drive":"C:","nscp_disk_exclude":["D:"],
      "nscp_disk_op":"used >","nscp_disk_warning":80,"nscp_disk_critical":90,"nscp_disk_arguments":["filter=type = 'fixed'"]})");
  EXPECT_EQ(t.query, "check_drivesize");
  EXPECT_EQ(t.arguments, (args{"filter=type = 'fixed'", "critical=used > 90", "drive=C:", "exclude=D:", "show-all", "warning=used > 80"}));

  t = builtin("nscp-local-counter", R"({"nscp_query":"check_pdh","nscp_counter_name":"\\System\\Processes","nscp_counter_op":">",
      "nscp_counter_warning":500,"nscp_counter_perfsyntax":"procs","nscp_showall":false})");
  EXPECT_EQ(t.arguments, (args{"counter=\\System\\Processes", "perf-syntax=procs", "warning=value > 500"}));

  t = builtin("nscp-local-tasksched", R"({"nscp_query":"check_tasksched","nscp_tasksched_name":"Backup","nscp_tasksched_recursive":true,
      "nscp_tasksched_warning":"exit_code != 0","nscp_tasksched_critical":"exit_code < 0","nscp_tasksched_hidden":false})");
  EXPECT_EQ(t.arguments, (args{"critical=exit_code < 0", "filter=title='Backup'", "recursive=true", "warning=exit_code != 0"}));
  // Without a task name there is no filter.
  t = builtin("nscp-local-tasksched", R"({"nscp_tasksched_name":"","nscp_tasksched_warning":"exit_code != 0"})");
  EXPECT_EQ(t.query, "check_tasksched");
  EXPECT_EQ(t.arguments, (args{"warning=exit_code != 0"}));

  // The one-shot client's own options mean nothing to a running agent.
  t = builtin("nscp-local-os-version", R"({"nscp_modules":["CheckSystem"],"nscp_boot":true,"nscp_log_level":"critical","nscp_load_all":false})");
  EXPECT_EQ(t.query, "check_os_version");
  EXPECT_TRUE(t.arguments.empty());
}

TEST(IcingaAgentTranslate, EveryBuiltinTranslates) {
  // Each command with nothing set but what it requires: none may throw for
  // want of an optional macro, and each names a query.
  const std::map<std::string, std::string> required{{"perfmon-windows", R"({"perfmon_win_counter":"\\x"})"},
                                                    {"ping-windows", R"({"ping_win_address":"h"})"},
                                                    {"ping4-windows", R"({"ping_win_address":"h"})"},
                                                    {"ping6-windows", R"({"ping_win_address":"h"})"},
                                                    {"service-windows", R"({"service_win_service":"s"})"},
                                                    {"nscp-local", R"({"nscp_query":"check_ok"})"}};
  for (const std::string &name : builtin_translations()) {
    const auto it = required.find(name);
    const translated_command t = builtin(name, it == required.end() ? "{}" : it->second.c_str());
    EXPECT_FALSE(t.query.empty()) << name;
    EXPECT_EQ(t.query.find('$'), std::string::npos) << name;
    for (const std::string &argument : t.arguments) {
      // check_process is given a syntax of its own; nothing else carries a `$`.
      if (argument.rfind("top-syntax=", 0) != 0) EXPECT_EQ(argument.find('$'), std::string::npos) << name << ": " << argument;
    }
  }
}

TEST(IcingaAgentTranslate, AliasDetails) {
  translation_config config;
  config.builtin = false;
  config.aliases["price"] = command_alias{"check_ok", {"message=costs $$5", "n=$n$", "$empty$", "$blank$"}};
  // `$$` is a dollar; numbers render as on a command line; an empty macro
  // drops its argument as a missing one does.
  const boost::optional<translated_command> t =
      translate_command(config, "price", json::parse(R"({"n":5.0,"empty":[],"blank":""})").as_object());
  ASSERT_TRUE(static_cast<bool>(t));
  EXPECT_EQ(t.value().arguments, (args{"message=costs $5", "n=5"}));
  // A `$` counts as the master's hand in it, escaped or not.
  EXPECT_FALSE(t.value().fixed_arguments);

  // Aliases work with the built-in table off, and only they do.
  EXPECT_FALSE(translate_command(config, "disk-windows", json::object()));

  // A template that is not valid Icinga is an error on the check, by name.
  config.aliases["broken"] = command_alias{"check_ok", {"message=$oops"}};
  const query_request req =
      parse_execute_command(json::parse(R"({"command_type":"check_command","command":"broken","host":"h","macros":{}})"), config);
  EXPECT_NE(req.error.find("Cannot translate the Icinga command 'broken'"), std::string::npos) << req.error;
  EXPECT_NE(req.error.find("unterminated macro"), std::string::npos) << req.error;
}

TEST(IcingaAgentTranslate, IfwApiWinsOverAnAlias) {
  translation_config config;
  config.aliases["nscp-cpu"] = command_alias{"check_ok", {}};
  query_request req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"nscp-cpu","host":"h",
      "macros":{"ifw_api_command":"check_cpu","ifw_api_arguments":{"warning":{"value":"load > 80"}}}})"), config);
  EXPECT_EQ(req.query, "check_cpu");
  EXPECT_EQ(req.arguments, (args{"warning=load > 80"}));
  EXPECT_FALSE(req.fixed_arguments);
  // Arguments alone are enough to say "this is an ifw-api command".
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"nscp-cpu","host":"h",
      "macros":{"ifw_api_arguments":{"warning":{"value":"load > 80"}}}})"), config);
  EXPECT_EQ(req.query, "nscp-cpu");
  EXPECT_EQ(req.arguments, (args{"warning=load > 80"}));
  // Without either the alias answers, and its arguments are its own.
  req = parse_execute_command(json::parse(R"({"command_type":"check_command","command":"nscp-cpu","host":"h","macros":{}})"), config);
  EXPECT_EQ(req.query, "check_ok");
  EXPECT_TRUE(req.fixed_arguments);
}

// A query the agent already has under an ITL name was put there on purpose:
// the built-in translation stands back for it. The module's own aliases are
// not asked about - they are the configuration saying what the name means.
TEST(IcingaAgentTranslate, ALocalQueryOfTheSameNameWins) {
  translation_config config;
  std::vector<std::string> asked;
  config.is_local_query = [&asked](const std::string &name) {
    asked.push_back(name);
    return name == "disk-windows";
  };
  EXPECT_FALSE(translate_command(config, "disk-windows", json::object()));
  EXPECT_TRUE(static_cast<bool>(translate_command(config, "load-windows", json::object())));
  // A name that is not an ITL command is never looked up.
  EXPECT_FALSE(translate_command(config, "check_cpu", json::object()));
  EXPECT_EQ(asked, (std::vector<std::string>{"disk-windows", "load-windows"}));

  // Through an ExecuteCommand it then runs under its own name, macros unused.
  const query_request req = parse_execute_command(
      json::parse(R"({"command_type":"check_command","command":"disk-windows","host":"h","macros":{"disk_win_warn":"20%"}})"), config);
  EXPECT_TRUE(req.error.empty()) << req.error;
  EXPECT_EQ(req.query, "disk-windows");
  EXPECT_TRUE(req.arguments.empty());

  config.aliases["disk-windows"] = command_alias{"check_drivesize", {"drive=C:"}};
  asked.clear();
  EXPECT_EQ(translate_command(config, "disk-windows", json::object()).value().query, "check_drivesize");
  EXPECT_TRUE(asked.empty());
}
