// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/function.hpp>
#include <client/simple_client.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nscapi/protobuf/settings.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <set>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <vector>

static void create_registry_query(const nscapi::core_wrapper *core, const std::string &command, const PB::Registry::ItemType &type,
                                  PB::Registry::RegistryResponseMessage &response_message, const bool fetch_all = false) {
  PB::Registry::RegistryRequestMessage rrm;
  PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
  if (!command.empty()) {
    payload->mutable_inventory()->set_name(command);
    payload->mutable_inventory()->set_fetch_all(true);
  } else if (fetch_all) {
    // Without a name, fetch_all is what makes the core look past the plugins
    // it has already instantiated and scan the module directory. That scan
    // loads every module found, so only ask when the answer is worth it.
    payload->mutable_inventory()->set_fetch_all(true);
  }
  payload->mutable_inventory()->add_type(type);
  std::string pb_response;
  core->registry_query(rrm.SerializeAsString(), pb_response);
  response_message.ParseFromString(pb_response);
}

// --- table rendering ----------------------------------------------------------
//
// Everything the prompt lists used to be tab separated, which lines up only
// while every name is shorter than a tab stop and falls apart the moment a
// description spans lines. A table pads each column to its widest cell and
// keeps the first line of every cell, so a long description does not push the
// rest of the row (or the next row) around.

typedef std::vector<std::string> table_row;

// The first line of a possibly multi-line text, trimmed. Lists get one line
// per entry; `desc` shows the full text where there is room for it.
static std::string first_line(const std::string &text) {
  std::string line = text.substr(0, text.find('\n'));
  boost::algorithm::trim(line);
  return line;
}

// Render rows as aligned columns, two spaces apart, `indent` spaces in front.
// The last column is not padded, so trailing whitespace never ends up in the
// output.
static std::string render_table(const std::vector<table_row> &rows, const std::size_t indent = 0) {
  std::vector<std::size_t> widths;
  for (const table_row &row : rows) {
    if (row.size() > widths.size()) widths.resize(row.size(), 0);
    for (std::size_t i = 0; i < row.size(); ++i) widths[i] = std::max(widths[i], first_line(row[i]).size());
  }
  std::string out;
  for (const table_row &row : rows) {
    if (!out.empty()) out += "\n";
    std::string line(indent, ' ');
    for (std::size_t i = 0; i < row.size(); ++i) {
      const std::string cell = first_line(row[i]);
      line += cell;
      if (i + 1 < row.size()) line += std::string(widths[i] - cell.size() + 2, ' ');
    }
    boost::algorithm::trim_right(line);
    out += line;
  }
  return out;
}

typedef ::PB::Registry::RegistryResponseMessage::Response::Inventory inventory_entry;

static bool is_loaded(const inventory_entry &inv) {
  for (int i = 0; i < inv.info().metadata_size(); i++) {
    if (inv.info().metadata(i).key() == "loaded" && inv.info().metadata(i).value() == "true") return true;
  }
  return false;
}

// Every inventory entry of a response, or the error the core answered with.
static bool collect_entries(const PB::Registry::RegistryResponseMessage &response_message, std::vector<inventory_entry> &entries, std::string &error) {
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const inventory_entry &i : pl.inventory()) entries.push_back(i);
    if (pl.result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
      error = "Error: " + pl.result().message();
      return false;
    }
  }
  return true;
}

// `name  description` for queries and aliases; `[X] name  description` for
// modules. Nothing found is reported as such rather than as an empty screen.
static std::string render_inventory(const std::vector<PB::Registry::RegistryResponseMessage> &responses, const bool with_loaded_marker) {
  std::vector<table_row> rows;
  for (const PB::Registry::RegistryResponseMessage &response : responses) {
    std::vector<inventory_entry> entries;
    std::string error;
    if (!collect_entries(response, entries, error)) return error;
    for (const inventory_entry &i : entries) {
      table_row row;
      if (with_loaded_marker) row.push_back(is_loaded(i) ? "[X]" : "[ ]");
      row.push_back(i.name());
      row.push_back(i.info().description());
      rows.push_back(row);
    }
  }
  return rows.empty() ? "Nothing found" : render_table(rows);
}

// An alias is registered with a description of the form "Alias for: <command
// line>" (or "Alternative name for: <command>"); that is the only place the
// target lives, so the description is parsed rather than the registry
// extended. Returns the command line after the prefix, or an empty string.
static std::string alias_target(const std::string &description) {
  static const char *prefixes[] = {"Alias for: ", "Alternative name for: "};
  for (const char *prefix : prefixes) {
    const std::size_t len = std::string(prefix).size();
    if (description.compare(0, len, prefix) == 0) return boost::algorithm::trim_copy(description.substr(len));
  }
  return "";
}

// What `<command> show-default` answers: the command's arguments with their
// default values, quoted and ready to paste. Empty when the command has no
// defaults, does not understand show-default, or fails.
static std::string show_default(const client::cli_handler_ptr &handler, const std::string &command) {
  try {
    nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
    std::list<std::string> args;
    args.push_back("show-default");
    std::string response;
    if (!helper.simple_query(command, args, response) || response.empty()) return "";
    PB::Commands::QueryResponseMessage message;
    if (!message.ParseFromString(response)) return "";
    for (const PB::Commands::QueryResponseMessage::Response &payload : message.payload()) {
      if (payload.result() != PB::Common::ResultCode::OK) continue;
      for (const PB::Commands::QueryResponseMessage::Response::Line &l : payload.lines()) {
        const std::string line = boost::algorithm::trim_copy(l.message());
        if (!line.empty()) return line;
      }
    }
  } catch (...) {
  }
  return "";
}

// `keywords <query> [args]`: the filter keywords a check offers, each with the
// value it has right now. The names and descriptions come from the help
// payload; the values come from running the check with a detail syntax that
// renders every keyword, thresholds off and every record shown, so what is
// listed is exactly what `filter=`, `warning=` and `critical=` expressions see.
// Arguments the user adds go to the check as they are (and win over the
// generated ones), so `keywords check_drivesize drive=c:` narrows the records.
static std::string render_keywords(const client::cli_handler_ptr &handler, const std::string &query, const std::list<std::string> &user_args) {
  const nscapi::core_wrapper *core = handler->get_core();
  PB::Registry::RegistryResponseMessage response;
  create_registry_query(core, query, PB::Registry::ItemType::QUERY, response);
  std::vector<inventory_entry> entries;
  std::string error;
  if (!collect_entries(response, entries, error)) return error;
  if (entries.empty()) return "Command not found: " + query;

  // An alias carries no keywords of its own; the target's are the ones its
  // filter expressions use.
  std::string command = query;
  const inventory_entry *source = &entries.front();
  std::vector<inventory_entry> target_entries;
  const std::string target = alias_target(source->info().description());
  if (!target.empty()) {
    command = target.substr(0, target.find(' '));
    PB::Registry::RegistryResponseMessage target_response;
    create_registry_query(core, command, PB::Registry::ItemType::QUERY, target_response);
    std::string ignored;
    collect_entries(target_response, target_entries, ignored);
    if (!target_entries.empty()) source = &target_entries.front();
  }
  // The generic summary keywords (count, list, status, ...) describe the
  // whole result, not one record: rendered inside a record they expand to
  // the list of all records - separators included - and `sep` is the list
  // separator itself. They get a line of their own instead of a value.
  static const char *summary_names[] = {"count",   "total",     "ok_count",  "warn_count",   "crit_count",  "problem_count", "list",
                                        "ok_list", "warn_list", "crit_list", "problem_list", "detail_list", "sep",           "status"};
  const std::set<std::string> summary(std::begin(summary_names), std::end(summary_names));
  std::vector<std::string> names, descriptions, summaries;
  for (int i = 0; i < source->parameters().fields_size(); i++) {
    const std::string &name = source->parameters().fields(i).name();
    if (summary.find(name) != summary.end()) {
      summaries.push_back(name);
      continue;
    }
    names.push_back(name);
    descriptions.push_back(source->parameters().fields(i).long_description());
  }
  if (names.empty() && summaries.empty()) return command + " has no filter keywords (it is not a filter based check)";
  if (names.empty()) return command + " has no record keywords, only the summary ones: " + boost::algorithm::join(summaries, ", ");

  // Two control characters that never appear in a rendered value; the check
  // separates keywords with one and records with the other.
  const std::string unit(1, '\x1f');
  const std::string record(1, '\x1e');
  std::string detail;
  for (const std::string &name : names) detail += (detail.empty() ? "" : unit) + "%(" + name + ")";

  std::list<std::string> args = user_args;
  const auto given = [&user_args](const std::string &key) {
    for (const std::string &arg : user_args) {
      if (arg == key || arg.compare(0, key.size() + 1, key + "=") == 0) return true;
    }
    return false;
  };
  if (!given("detail-syntax")) args.push_back("detail-syntax=" + detail);
  if (!given("top-syntax")) args.push_back("top-syntax=${list}");
  if (!given("list-separator")) args.push_back("list-separator=" + record);
  if (!given("warning")) args.push_back("warning=none");
  if (!given("critical")) args.push_back("critical=none");
  if (!given("show-all")) args.push_back("show-all=true");
  if (!given("empty-state")) args.push_back("empty-state=ok");

  std::string raw;
  nscapi::core_helper helper(core, handler->get_plugin_id());
  if (!helper.simple_query(command, args, raw) || raw.empty()) return "Failed to run " + command;
  PB::Commands::QueryResponseMessage message;
  if (!message.ParseFromString(raw) || message.payload_size() == 0) return "Failed to run " + command + ": no response";
  const PB::Commands::QueryResponseMessage::Response &payload = message.payload(0);
  std::string rendered;
  if (payload.lines_size() > 0) rendered = payload.lines(0).message();
  if (payload.result() != PB::Common::ResultCode::OK) {
    return command + " returned " + nscapi::plugin_helper::translateReturn(payload.result()) + ": " + first_line(rendered);
  }
  std::vector<std::vector<std::string> > records;
  std::string::size_type start = 0;
  while (start <= rendered.size()) {
    const std::string::size_type end = rendered.find(record, start);
    const std::string entry = rendered.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!entry.empty() && entry.find(unit) != std::string::npos) {
      std::vector<std::string> values;
      boost::algorithm::split(values, entry, boost::algorithm::is_any_of(unit));
      records.push_back(values);
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }

  // One value column per record. Past a handful the table is unreadable and
  // the user is better served by narrowing, so say so instead of widening.
  const std::size_t max_records = 8;
  const std::size_t shown = std::min(records.size(), max_records);
  std::vector<table_row> rows;
  table_row heading{"KEYWORD"};
  for (std::size_t r = 0; r < shown; ++r) heading.push_back(shown == 1 ? "VALUE" : "#" + std::to_string(r + 1));
  heading.push_back("DESCRIPTION");
  rows.push_back(heading);
  for (std::size_t i = 0; i < names.size(); ++i) {
    table_row row{names[i]};
    for (std::size_t r = 0; r < shown; ++r) row.push_back(i < records[r].size() ? records[r][i] : "");
    row.push_back(descriptions[i]);
    rows.push_back(row);
  }
  std::string out = "Filter keywords of " + command;
  if (command != query) out += " (via " + query + ")";
  if (records.empty()) {
    out += " - no records matched, so there are no values to show:";
  } else {
    out += ", " + std::to_string(records.size()) + (records.size() == 1 ? " record" : " records");
    if (records.size() > shown) out += " (showing " + std::to_string(shown) + "; add an argument such as filter=... to narrow)";
    out += ":";
  }
  out += "\n" + render_table(rows, 2);
  if (!summaries.empty()) {
    out += "\nSummary keywords, for top-syntax (they describe the whole result, not a record): " + boost::algorithm::join(summaries, ", ");
  }
  return out;
}

// `desc <query>`: what it is, and what it takes. For an alias the command it
// expands to is shown, and the parameters listed are the target's - the alias
// itself takes none - so the reader sees what can still be passed and what
// the alias has already fixed. The "Default:" line is the command as it would
// run with every default spelled out (what `<command> show-default` prints),
// which is the quickest way to see what a check does when called bare.
static std::string render_description(const client::cli_handler_ptr &handler, const inventory_entry &inv) {
  const nscapi::core_wrapper *core = handler->get_core();
  std::vector<table_row> header;
  header.push_back({"Command:", inv.name()});
  const std::string target = alias_target(inv.info().description());
  std::string parameters_of = inv.name();
  const inventory_entry *parameters_from = &inv;
  PB::Registry::RegistryResponseMessage target_response;
  std::vector<inventory_entry> target_entries;
  if (!target.empty()) {
    header.push_back({"Runs:", target});
    // The target command is the first word of the command line; the rest are
    // the arguments the alias fixes.
    const std::string target_command = target.substr(0, target.find(' '));
    create_registry_query(core, target_command, PB::Registry::ItemType::QUERY, target_response);
    std::string ignored;
    collect_entries(target_response, target_entries, ignored);
    if (!target_entries.empty()) {
      parameters_from = &target_entries.front();
      parameters_of = target_command;
      header.push_back({"Description:", target_entries.front().info().description()});
    }
  } else {
    header.push_back({"Description:", inv.info().description()});
  }
  const std::string defaults = show_default(handler, parameters_of);
  if (!defaults.empty()) header.push_back({"Default:", parameters_of + " " + defaults});
  std::string out = render_table(header);

  // The rest of a multi-line description, below the header, as written.
  const std::string &description = parameters_from->info().description();
  const std::size_t newline = description.find('\n');
  if (newline != std::string::npos) {
    std::string rest = description.substr(newline + 1);
    boost::algorithm::trim(rest);
    if (!rest.empty()) out += "\n\n" + rest;
  }

  std::vector<table_row> rows;
  bool any_default = false;
  for (int i = 0; i < parameters_from->parameters().parameter_size(); i++) {
    const ::PB::Registry::ParameterDetail &p = parameters_from->parameters().parameter(i);
    if (!p.default_value().empty()) any_default = true;
  }
  for (int i = 0; i < parameters_from->parameters().parameter_size(); i++) {
    const ::PB::Registry::ParameterDetail &p = parameters_from->parameters().parameter(i);
    table_row row;
    row.push_back(p.name());
    if (any_default) row.push_back(p.default_value());
    row.push_back(p.long_description());
    rows.push_back(row);
  }
  out += "\n\nParameters";
  if (parameters_of != inv.name()) out += " (of " + parameters_of + ")";
  out += ":";
  if (rows.empty()) {
    out += "\n  (none)";
  } else {
    if (any_default) rows.insert(rows.begin(), table_row{"NAME", "DEFAULT", "DESCRIPTION"});
    out += "\n" + render_table(rows, 2);
  }
  return out;
}

namespace client {

const std::vector<command_info> &builtin_commands() {
  // Ordered the way the help text should read: what to look at first, then the
  // things that change state.
  static const std::vector<command_info> commands = {
      {"help", "", "show this help"},
      {"exit", "", "leave the interactive prompt"},
      {"queries", "", "list all available queries"},
      {"commands", "", "list all available queries (alias for queries)"},
      {"aliases", "", "list all available query aliases"},
      {"alias", "", "list all available query aliases (alias for aliases)"},
      {"list", "", "list queries and aliases"},
      {"plugins", "", "list all plugins and whether they are loaded"},
      {"desc", "<query>", "describe a query and its parameters"},
      {"keywords", "<query> [args]", "show the filter keywords of a query with their current values (arguments narrow the records, e.g. drive=c:)"},
      {"metrics", "[prefix]", "show the metrics collected so far"},
      {"settings", "", "show the configured settings (keys set in the configuration, not every registered default)"},
      {"exec", "<target> <command> [args]", "run a command on one module"},
      {"load", "<module>", "load a module now"},
      {"unload", "<module>", "unload a module now"},
      {"enable", "<module>", "enable a module in the configuration and save"},
      {"disable", "<module>", "disable a module in the configuration and save"},
      {"reload", "", "reload all modules"},
  };
  return commands;
}

static std::string render_help() {
  const std::string catch_all = "<any other command>";
  std::string::size_type width = catch_all.size();
  for (const command_info &c : builtin_commands()) {
    const std::string::size_type len = c.name.size() + (c.args.empty() ? 0 : c.args.size() + 1);
    if (len > width) width = len;
  }
  std::string help = "Commands:";
  for (const command_info &c : builtin_commands()) {
    const std::string usage = c.args.empty() ? c.name : c.name + " " + c.args;
    help += "\n\t" + usage + std::string(width - usage.size() + 2, ' ') + "- " + c.description;
  }
  help += "\n\t" + catch_all + std::string(width - catch_all.size() + 2, ' ') + "- run as a query";
  return help;
}

static std::vector<command_info> collect(const PB::Registry::RegistryResponseMessage &response_message) {
  std::vector<command_info> ret;
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      command_info info;
      info.name = i.name();
      info.description = i.info().description();
      ret.push_back(info);
    }
  }
  return ret;
}

std::vector<command_info> cli_client::list_queries() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, response_message);
  std::vector<command_info> ret = collect(response_message);
  // Aliases arrive under a separate inventory type but behave exactly like
  // queries at the prompt, so the caller gets one merged list.
  PB::Registry::RegistryResponseMessage alias_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, alias_message);
  const std::vector<command_info> aliases = collect(alias_message);
  ret.insert(ret.end(), aliases.begin(), aliases.end());
  return ret;
}

static bool metadata_flag(const ::PB::Registry::RegistryResponseMessage::Response::Inventory &inv, const std::string &key) {
  for (int i = 0; i < inv.info().metadata_size(); i++) {
    if (inv.info().metadata(i).key() == key) return inv.info().metadata(i).value() == "true";
  }
  return false;
}

static std::vector<module_info> collect_modules(const PB::Registry::RegistryResponseMessage &response_message) {
  std::vector<module_info> ret;
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      module_info info;
      info.name = i.name();
      info.description = i.info().description();
      info.loaded = metadata_flag(i, "loaded");
      info.enabled = metadata_flag(i, "enabled");
      ret.push_back(info);
    }
  }
  return ret;
}

std::vector<module_info> cli_client::list_modules() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message);
  return collect_modules(response_message);
}

std::vector<module_info> cli_client::list_all_modules() const {
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message, true);
  return collect_modules(response_message);
}

std::vector<std::string> cli_client::list_parameters(const std::string &query) const {
  std::vector<std::string> ret;
  if (query.empty()) return ret;
  PB::Registry::RegistryResponseMessage response_message;
  create_registry_query(handler->get_core(), query, PB::Registry::ItemType::QUERY, response_message);
  for (const ::PB::Registry::RegistryResponseMessage::Response &pl : response_message.payload()) {
    for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : pl.inventory()) {
      if (i.name() != query) continue;
      for (int p = 0; p < i.parameters().parameter_size(); p++) {
        ret.push_back(i.parameters().parameter(p).name());
      }
    }
  }
  return ret;
}

void cli_client::handle_command(const std::string &command) {
  if (command == "plugins") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::MODULE, response_message);
    handler->output_message(render_inventory({response_message}, true));
  } else if (command == "help") {
    handler->output_message(render_help());
  } else if (command == "reload") {
    if (!handler->get_core()->reload("delayed,service")) {
      NSC_LOG_ERROR("Failed to reload modules");
    }
  } else if (command.size() > 6 && command.substr(0, 6) == "enable") {
    std::string name = command.substr(7);
    bool has_errors = false;
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_update()->mutable_node()->set_path("/modules");
      r->mutable_update()->mutable_node()->set_key(name);
      r->mutable_update()->mutable_node()->set_value("enabled");
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_control()->set_command(PB::Settings::Command::SAVE);
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    if (!has_errors) handler->output_message(name + " enabled successfully...");
  } else if (command.size() > 7 && command.substr(0, 7) == "disable") {
    std::string name = command.substr(8);
    bool has_errors = false;
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_update()->mutable_node()->set_path("/modules");
      r->mutable_update()->mutable_node()->set_key(name);
      r->mutable_update()->mutable_node()->set_value("disabled");
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    {
      PB::Settings::SettingsRequestMessage srm;
      PB::Settings::SettingsRequestMessage::Request *r = srm.add_payload();
      r->mutable_control()->set_command(PB::Settings::Command::SAVE);
      r->set_plugin_id(handler->get_plugin_id());
      std::string response;
      handler->get_core()->settings_query(srm.SerializeAsString(), response);
      PB::Settings::SettingsResponseMessage response_message;
      response_message.ParseFromString(response);
      for (int i = 0; i < response_message.payload_size(); i++) {
        if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
          handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
          has_errors = true;
        }
      }
    }
    if (!has_errors) handler->output_message(name + " disabled successfully...");
  } else if (command.size() > 4 && command.substr(0, 4) == "load") {
    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    std::string name = command.substr(5);
    payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
    payload->mutable_control()->set_command(PB::Registry::Command::LOAD);
    payload->mutable_control()->set_name(name);
    std::string pb_response, json_response;
    handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
    PB::Registry::RegistryResponseMessage response_message;
    response_message.ParseFromString(pb_response);
    bool has_errors = false;
    for (int i = 0; i < response_message.payload_size(); i++) {
      if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
        handler->output_message("Failed to load module: " + response_message.payload(i).result().message());
        has_errors = true;
      }
    }
    if (!has_errors) handler->output_message(name + " loaded successfully...");
  } else if (command.size() > 6 && command.substr(0, 6) == "unload") {
    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    std::string name = command.substr(7);
    payload->mutable_control()->set_type(PB::Registry::ItemType::MODULE);
    payload->mutable_control()->set_command(PB::Registry::Command::UNLOAD);
    payload->mutable_control()->set_name(name);
    std::string pb_response, json_response;
    handler->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
    PB::Registry::RegistryResponseMessage response_message;
    response_message.ParseFromString(pb_response);
    bool has_errors = false;
    for (int i = 0; i < response_message.payload_size(); i++) {
      if (response_message.payload(i).result().code() != PB::Common::Result_StatusCodeType_STATUS_OK) {
        handler->output_message("Failed to unload module: " + response_message.payload(i).result().message());
        has_errors = true;
      }
    }
    if (!has_errors) handler->output_message(name + " unloaded successfully...");
  } else if (command == "queries" || command == "commands") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, response_message);
    handler->output_message(render_inventory({response_message}, false));
  } else if (command == "aliases" || command == "alias") {
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, response_message);
    handler->output_message(render_inventory({response_message}, false));
  } else if (command.size() > 5 && command.substr(0, 4) == "desc") {
    const std::string name = boost::algorithm::trim_copy(command.substr(5));
    PB::Registry::RegistryResponseMessage response_message;
    create_registry_query(handler->get_core(), name, PB::Registry::ItemType::QUERY, response_message);
    std::vector<inventory_entry> entries;
    std::string error;
    if (!collect_entries(response_message, entries, error)) {
      handler->output_message(error);
    } else if (entries.empty()) {
      handler->output_message("Command not found: " + name);
    } else {
      handler->output_message(render_description(handler, entries.front()));
    }
  } else if (command == "keywords" || (command.size() > 9 && command.substr(0, 9) == "keywords ")) {
    std::list<std::string> words;
    if (command.size() > 9) str::utils::parse_command(command.substr(9), words);
    if (words.empty()) {
      handler->output_message("Usage: keywords <query> [args]");
    } else {
      const std::string query = words.front();
      words.pop_front();
      handler->output_message(render_keywords(handler, query, words));
    }
  } else if (command == "list") {
    // Both, in one table, so the columns line up across the two kinds.
    PB::Registry::RegistryResponseMessage queries, aliases;
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY, queries);
    create_registry_query(handler->get_core(), "", PB::Registry::ItemType::QUERY_ALIAS, aliases);
    handler->output_message(render_inventory({queries, aliases}, false));
  } else if (command.size() >= 7 && command.substr(0, 7) == "metrics") {
    for (const metrics::metrics_store::values_map::value_type &v : metrics_store.get(command.substr(7))) {
      handler->output_message(v.first + "=" + v.second);
    }
  } else if (command.size() > 4 && command.substr(0, 4) == "exec") {
    try {
      std::list<std::string> args;
      str::utils::parse_command(command, args);
      if (args.size() < 3) {
        handler->output_message("Usage: exec <target> <command> [args]");
        return;
      }
      args.pop_front();
      std::string target = args.front();
      args.pop_front();
      std::string cmd = args.front();
      args.pop_front();
      std::list<std::string> result;
      nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
      helper.exec_simple_command(target, cmd, args, result);
      for (const std::string &s : result) handler->output_message(s);
    } catch (const std::exception &e) {
      handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      handler->output_message("Unknown exception");
    }
  } else if (command.size() >= 8 && command.substr(0, 8) == "settings") {
    namespace pf = nscapi::protobuf::functions;

    pf::settings_query q(handler->get_plugin_id());
    // Walk the settings store rather than the registry: the registry lists
    // every key any loaded module has declared, almost all of them unset,
    // which buried the handful of lines that are actually configured.
    // Sensitive keys come back as "***": this dump ends up in tickets and
    // chat windows, and the file is right there for anyone who needs the
    // real value.
    q.list_configured("/", true, true);

    handler->get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      handler->output_message("ERROR: " + q.get_response_error());
    } else {
      std::string out;
      for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
        // Section entries come back too; only keys are worth a line.
        if (val.key().empty()) continue;
        if (!out.empty()) out += "\n";
        out += val.path() + "/" + val.key() + "=" + val.get_string();
      }
      handler->output_message(out.empty() ? "Nothing configured" : out);
    }
  } else if (!command.empty()) {
    try {
      std::list<std::string> args;
      str::utils::parse_command(command, args);
      if (args.empty()) {
        handler->output_message("Empty command");
        return;
      }
      std::string cmd = args.front();
      args.pop_front();
      nscapi::core_helper helper(handler->get_core(), handler->get_plugin_id());
      std::string response;
      if (!helper.simple_query(cmd, args, response)) {
        NSC_LOG_ERROR("Failed to execute command: " + cmd);
      }
      if (!response.empty()) {
        try {
          PB::Commands::QueryResponseMessage message;
          message.ParseFromString(response);

          for (const PB::Commands::QueryResponseMessage::Response &payload : message.payload()) {
            for (const PB::Commands::QueryResponseMessage::Response::Line &l : payload.lines()) {
              handler->output_message(nscapi::plugin_helper::translateReturn(payload.result()) + ": " + l.message());
              const std::string perf = nscapi::protobuf::functions::build_performance_data(l, nscapi::protobuf::functions::no_truncation);
              // Most checks return none, and a bare "Performance data:" line
              // after every result is just noise at the prompt.
              if (!perf.empty()) handler->output_message(" Performance data: " + perf);
            }
          }
        } catch (std::exception &e) {
          handler->output_message("Failed to extract return message: " + utf8::utf8_from_native(e.what()));
        }
      }
    } catch (const std::exception &e) {
      handler->output_message("Exception: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      handler->output_message("Unknown exception");
    }
  }
}

void cli_client::push_metrics(const PB::Metrics::MetricsMessage &response) { metrics_store.set(response); }

}  // namespace client