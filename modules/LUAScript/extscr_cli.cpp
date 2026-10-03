// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "extscr_cli.h"

#include <config.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <iterator>
#include <list>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <string>
#include <utility>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
namespace pf = nscapi::protobuf::functions;
namespace npo = nscapi::program_options;
namespace fs = boost::filesystem;

#define SCRIPT_PATH "/settings/lua/scripts"
#define MODULE_NAME "LuaScript"
// Relative to ${scripts}, matching what find_script searches: it tries
// root / <value>, so "lua/x.lua" resolves to ${scripts}/lua/x.lua - where
// add --import now writes it. Was "scripts\\lua\\", which belonged to the old
// ${base-path} root and only resolved on Windows.
#define REL_SCRIPT_PATH "lua/"

namespace json = boost::json;

extscr_cli::extscr_cli(std::shared_ptr<script_provider> provider) : provider_(std::move(provider)) {}

bool extscr_cli::run(std::string cmd, const PB::Commands::ExecuteRequestMessage_Request &request, PB::Commands::ExecuteResponseMessage_Response *response) {
  if (cmd == "add")
    add_script(request, response);
  else if (cmd == "install")
    configure(request, response);
  else if (cmd == "list")
    list(request, response);
  else if (cmd == "show")
    show(request, response);
  else if (cmd == "delete")
    delete_script(request, response);
  else
    return false;
  return true;
}

bool extscr_cli::validate_sandbox(fs::path pscript, PB::Commands::ExecuteResponseMessage::Response *response) const {
  fs::path path = provider_->get_root();
  if (!file_helpers::checks::path_contains_file(path, std::move(pscript))) {
    nscapi::protobuf::functions::set_response_bad(*response, "Not allowed outside: " + path.string());
    return false;
  }
  return true;
}

void extscr_cli::list(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  po::variables_map vm;
  po::options_description desc;
  bool json = false, query = false, lib = false;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("json", po::bool_switch(&json), "Return the list in json format.")
    ("query", po::bool_switch(&query), "List queries instead of scripts (for aliases).")
    ("include-lib", po::bool_switch(&lib), "Do not ignore any lib folders.")
  ;
  // clang-format on

  try {
    npo::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);
  } catch (const std::exception &e) {
    return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
  }

  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
    return;
  }
  std::string resp;
  json::array data;
  if (query) {
    PB::Registry::RegistryRequestMessage rrm;
    PB::Registry::RegistryResponseMessage r_response;
    PB::Registry::RegistryRequestMessage::Request *payload = rrm.add_payload();
    // Names only. For a QUERY inventory fetch_all makes the core run every
    // registered command with `help-pb` to collect its parameters, which this
    // listing never reads: GET /api/v2/scripts/<runtime> executed every check
    // on the box - each script handler included, with whatever side effects
    // it has - just to print their names.
    payload->mutable_inventory()->set_fetch_all(false);
    payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
    std::string pb_response;
    provider_->get_core()->registry_query(rrm.SerializeAsString(), pb_response);
    r_response.ParseFromString(pb_response);
    for (const ::PB::Registry::RegistryResponseMessage_Response &p : r_response.payload()) {
      for (const ::PB::Registry::RegistryResponseMessage_Response_Inventory &i : p.inventory()) {
        if (json) {
          data.push_back(json::value(i.name()));
        } else {
          resp += i.name() + "\n";
        }
      }
    }
  } else {
    fs::path dir = provider_->get_core()->expand_path("${scripts}/lua");
    // Relativised against ${scripts}, the folder find_file resolves against, so
    // that what `list` prints can be handed straight back to `add`. It used to
    // append the sub-folder to the root, which matched on no platform and left
    // the strip below relativising on its own; rooting at ${base-path} instead
    // made the prefix match on Windows, but printed `scripts\lua\x` - a
    // spelling find_file has no candidate for, so `add` then refused the very
    // value `list` had just produced from anywhere but the install directory.
    // ${scripts} is the prefix on both platforms and survives a [paths]
    // override moving the folder off ${base-path} entirely.
    fs::path rel = provider_->get_core()->expand_path("${scripts}");
    fs::recursive_directory_iterator iter(dir), eod;
    for (fs::path const &i : boost::make_iterator_range(iter, eod)) {
      std::string s = i.string();
      // Relative to ${scripts} when the file is under it, which is every file
      // this loop walks, giving `lua/x.lua` - the same spelling `add --import`
      // records and one find_file resolves from any working directory. A file
      // reached through a symlink out of the folder is left absolute rather
      // than mangled: the strip used to slice the leading separator off
      // regardless, leaving a rootless `usr/lib/nsclient/scripts/x` that named
      // no file at all.
      if (boost::algorithm::starts_with(s, rel.string())) {
        s = s.substr(rel.string().size());
        if (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
      }
      if (s.empty()) continue;
      fs::path clone = i.parent_path();
      if (fs::is_regular_file(i) && !boost::algorithm::contains(clone.string(), "lib")) {
        if (json) {
          data.push_back(json::value(s));
        } else {
          resp += s + "\n";
        }
      }
    }
  }
  if (json) {
    resp = json::serialize(data);
  }
  nscapi::protobuf::functions::set_response_good(*response, resp);
}

namespace {
enum class sandbox_use { read, remove };

bool exists_entry(const fs::path &p) {
  boost::system::error_code ec;
  return fs::exists(fs::symlink_status(p, ec));
}

// The entry a `--script` name refers to, resolved only inside `root`
// (`${scripts}/lua`). Unlike script_provider::find_file() this does not try
// the name as given - relative to the working directory, or absolute - so a
// name cannot reach a file outside the scripts folder. `lua/foo.lua` (the
// form `list` and the REST listing print) and `foo.lua` / `foo` (the form a
// script is configured by) both resolve. `outside` is set when a name only
// resolves to something outside `root`, so the caller can say why it refused.
//
// Containment is decided on real paths, symlinks resolved: a lexical test
// alone lets a symlink inside the folder (or a symlinked sub-folder) reach any
// file the service account can. A path that cannot be resolved counts as
// outside. This is PythonScript's resolver with the Lua extension.
//
// - read: a regular file whose real path - the link's target, for a link - is
//   inside the folder.
// - remove: a regular file or a symlink (dangling, or to a directory, too)
//   that itself sits in the folder once any symlinked folder above it is
//   resolved. What a link points at does not matter, since removing a link
//   never touches its target; a real directory is never a candidate.
//
// The path returned is the one inside the folder, so delete removes a link,
// never its target.
boost::optional<fs::path> resolve_in_sandbox(const fs::path &root, const std::string &script, const sandbox_use use, bool &outside) {
  outside = false;
  if (script.empty()) return boost::none;
  boost::system::error_code ec;
  const fs::path real_root = fs::weakly_canonical(root, ec);
  if (ec) return boost::none;
  const fs::path parent = root.parent_path();
  const std::list<fs::path> candidates = {root / script, root / (script + ".lua"), parent / script, parent / (script + ".lua")};
  for (const fs::path &c : candidates) {
    const fs::path candidate = c.lexically_normal();
    if (!exists_entry(candidate)) continue;
    if (!file_helpers::checks::path_contains_file(root, candidate)) {
      outside = true;
      continue;
    }
    const bool is_link = fs::is_symlink(fs::symlink_status(candidate, ec));
    const bool is_file = fs::is_regular_file(candidate, ec);
    if (use == sandbox_use::read ? !is_file : !(is_file || is_link)) continue;
    // Where the entry itself lives, any symlinked folder above it resolved.
    const fs::path real_parent = fs::weakly_canonical(candidate.parent_path(), ec);
    if (ec || !file_helpers::checks::path_contains_file(real_root, real_parent / candidate.filename())) {
      outside = true;
      continue;
    }
    if (use == sandbox_use::read) {
      const fs::path real = fs::weakly_canonical(candidate, ec);
      if (ec || !file_helpers::checks::path_contains_file(real_root, real)) {
        outside = true;
        continue;
      }
    }
    return candidate;
  }
  return boost::none;
}

// The file a configured entry loads, found the way the loader finds it
// (lua_script::find_script(): as written - absolute, or relative to the
// working directory - then under ${scripts}/lua and ${scripts}, each with and
// without `.lua`). Also tried with any ${...} expanded and with `\` read as a
// separator, so an entry written for another platform or with a path variable
// still matches the file it names.
boost::optional<fs::path> configured_file(nscapi::core_wrapper *core, const fs::path &scripts, const std::string &configured) {
  std::list<std::string> forms = {configured, core->expand_path(configured)};
  for (const std::string &f : std::list<std::string>(forms)) forms.push_back(boost::algorithm::replace_all_copy(f, "\\", "/"));
  for (const std::string &f : forms) {
    for (const fs::path &c : {fs::path(f), fs::path(f + ".lua"), scripts / "lua" / f, scripts / "lua" / (f + ".lua"), scripts / f, scripts / (f + ".lua")}) {
      if (exists_entry(c)) return c;
    }
  }
  return boost::none;
}

bool same_entry(const fs::path &a, const fs::path &b) {
  boost::system::error_code ec;
  if (fs::absolute(a).lexically_normal() == fs::absolute(b).lexically_normal()) return true;
  return fs::equivalent(a, b, ec) && !ec;
}

// The script's contents, or nothing when it cannot be read - an unreadable
// file must not pass for an empty one.
boost::optional<std::string> read_file(const fs::path &file) {
  std::ifstream in(file.string().c_str(), std::ios::in | std::ios::binary);
  if (!in.is_open()) return boost::none;
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) return boost::none;
  return data;
}

bool parse_script_option(const char *what, const PB::Commands::ExecuteRequestMessage::Request &request,
                         PB::Commands::ExecuteResponseMessage::Response *response, std::string &script) {
  po::variables_map vm;
  po::options_description desc;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("script", po::value<std::string>(&script), what)
  ;
  // clang-format on

  try {
    npo::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);
  } catch (const std::exception &e) {
    npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return false;
  }

  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
    return false;
  }
  if (script.empty()) {
    nscapi::protobuf::functions::set_response_bad(*response, "No script specified add --script");
    return false;
  }
  return true;
}
}  // namespace

// Both verbs used to parse their options and then do nothing, so `nscp lua
// show` printed nothing and `nscp lua delete` deleted nothing - and GET and
// DELETE on /api/v2/scripts/lua/<name>, which run them, answered an empty 200.
void extscr_cli::show(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  std::string script;
  if (!parse_script_option("Script to show.", request, response, script)) return;

  const fs::path root = provider_->get_root();
  bool outside = false;
  const boost::optional<fs::path> file = resolve_in_sandbox(root, script, sandbox_use::read, outside);
  if (!file) {
    nscapi::protobuf::functions::set_response_bad(*response, outside ? "Not allowed outside: " + root.string() : "Script not found: " + script);
    return;
  }
  const boost::optional<std::string> data = read_file(file.value());
  if (!data) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to read " + file.value().string());
    return;
  }
  nscapi::protobuf::functions::set_response_good(*response, data.value());
}

void extscr_cli::delete_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  std::string script;
  if (!parse_script_option("Script to delete.", request, response, script)) return;

  const fs::path root = provider_->get_root();
  bool outside = false;
  const boost::optional<fs::path> file = resolve_in_sandbox(root, script, sandbox_use::remove, outside);
  if (!file) {
    nscapi::protobuf::functions::set_response_bad(*response, outside ? "Not allowed outside: " + root.string() : "Script not found: " + script);
    return;
  }
  const fs::path target = file.value();

  // Drop every configured alias that loads this file, so the next reload does
  // not log a script it can no longer find. Resolved before the file goes, as
  // resolving needs it to exist. list_configured() walks the store itself:
  // list() answers from the registry, which reports a key with an empty value
  // - the bare `foo.lua =` form - without its key.
  pf::settings_query q(provider_->get_id());
  q.list_configured(SCRIPT_PATH, false, false);
  provider_->get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
    return;
  }
  const fs::path scripts = root.parent_path();
  std::list<std::string> aliases;
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (!val.matches(std::string(SCRIPT_PATH)) || val.key().empty()) continue;
    // `alias = file`, or a bare `file =` whose key is the script - the loader
    // reads them the same way.
    std::string configured = val.get_string();
    if (configured.empty()) configured = val.key();
    const boost::optional<fs::path> loaded = configured_file(provider_->get_core(), scripts, configured);
    if (loaded && same_entry(loaded.value(), target)) aliases.push_back(val.key());
  }

  boost::system::error_code ec;
  fs::remove(target, ec);
  if (ec) {
    nscapi::protobuf::functions::set_response_bad(*response, "Failed to delete " + target.string() + ": " + ec.message());
    return;
  }

  if (!aliases.empty()) {
    pf::settings_query s(provider_->get_id());
    for (const std::string &alias : aliases) s.erase(SCRIPT_PATH, alias);
    s.save();
    provider_->get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response,
                                                    "Deleted " + target.string() + " but failed to update the configuration: " + s.get_response_error());
      return;
    }
  }
  std::string msg = "Deleted " + target.string();
  if (!aliases.empty()) msg += " and removed it from " SCRIPT_PATH;
  // The instance already loaded keeps running until the module reloads; say
  // so rather than leave a caller wondering why its commands still answer.
  msg += ", it stays loaded until LUAScript is reloaded";
  nscapi::protobuf::functions::set_response_good(*response, msg);
}

void extscr_cli::add_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string script, alias, import_script;
  bool list = false, replace = false, no_config = false;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")

    ("script", po::value<std::string>(&script),
    "Script to add")

    ("alias", po::value<std::string>(&alias),
    "The alias of the script (defaults to basename of script)")

    ("list", po::bool_switch(&list),
    "List all scripts in the scripts folder.")

    ("import", po::value<std::string>(&import_script),
    "Import (copy to script folder) a script.")

    ("replace", po::bool_switch(&replace),
    "Used when importing to specify that the script will be overwritten.")

    ("no-config", po::bool_switch(&no_config),
    "Do not write the updated configuration (i.e. changes are only transient).")
  ;
  // clang-format on

  try {
    npo::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);
  } catch (const std::exception &e) {
    return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
  }

  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
    return;
  }
  if (script.empty()) {
    nscapi::protobuf::functions::set_response_bad(*response, "No script specified add --script");
    return;
  }
  fs::path file = provider_->get_core()->expand_path(script);
  fs::path script_root = provider_->get_root();

  if (!import_script.empty()) {
    file = script_root / file_helpers::meta::get_filename(file);
    script = REL_SCRIPT_PATH + file_helpers::meta::get_filename(file);
    if (fs::exists(file)) {
      if (replace) {
        fs::remove(file);
      } else {
        nscapi::protobuf::functions::set_response_bad(*response, "Script already exists, specify --replace to replace it");
        return;
      }
    }
    try {
      // copy_file does not create the destination directory, and nothing
      // guarantees it exists: ${scripts}/lua only materialises on Windows when
      // the sample scripts feature is selected, and a ${scripts} override
      // points somewhere that was never populated at all. Without this the
      // import failed with a bare "No such file or directory" naming a path
      // the operator had no reason to create by hand.
      fs::create_directories(file.parent_path());
      fs::copy_file(import_script, file);
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to import script: " + utf8::utf8_from_native(e.what()));
      return;
    }
  }

  bool found = fs::is_regular_file(file);
  if (!found) {
    boost::optional<fs::path> path = provider_->find_file(file.string());
    if (path) {
      file = path.value();
      found = fs::is_regular_file(file);
    }
  }
  if (!found) {
    nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
    return;
  }
  if (alias.empty()) {
    alias = file.filename().stem().string();
  }

  if (!no_config) {
    nscapi::protobuf::functions::settings_query s(provider_->get_id());
    s.set(SCRIPT_PATH, alias, script);
    s.set(MAIN_MODULES_SECTION, MODULE_NAME, "enabled");
    s.save();
    provider_->get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return;
    }
  }
  nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script);
}

void extscr_cli::configure(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  po::variables_map vm;
  po::options_description desc;
  typedef std::map<std::string, std::string> script_map_type;
  typedef std::vector<std::string> script_lst_type;
  script_map_type scripts;
  script_lst_type to_add;
  script_lst_type to_remove;
  bool module = false;

  pf::settings_query q(provider_->get_id());
  q.list(SCRIPT_PATH);
  q.get(MAIN_MODULES_SECTION, MODULE_NAME, "");

  provider_->get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
    return;
  }
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (val.matches(MAIN_MODULES_SECTION, MODULE_NAME) && val.get_bool())
      module = true;
    else if (val.matches(SCRIPT_PATH))
      scripts[val.get_string()] = val.key();
  }
  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("add", po::value<script_lst_type>(&to_add), "Scripts to add to the list of loaded scripts.")
    ("remove", po::value<script_lst_type>(&to_remove), "Scripts to remove from list of loaded scripts.")
    ;
  // clang-format on

  try {
    npo::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);
  } catch (const std::exception &e) {
    return npo::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
  }

  if (vm.count("help")) {
    nscapi::protobuf::functions::set_response_good(*response, npo::help(desc));
    return;
  }
  std::stringstream result;

  nscapi::protobuf::functions::settings_query sq(provider_->get_id());
  if (!module) {
    sq.set(MAIN_MODULES_SECTION, MODULE_NAME, "enabled");
  }
  for (const std::string &s : to_add) {
    if (!provider_->find_file(s)) {
      result << "Failed to find: " << s << std::endl;
    } else {
      if (scripts.find(s) == scripts.end()) {
        sq.set(SCRIPT_PATH, s, s);
        scripts[s] = s;
      } else {
        result << "Failed to add duplicate script: " << s << std::endl;
      }
    }
  }
  for (const std::string &s : to_remove) {
    const script_map_type::const_iterator v = scripts.find(s);
    if (v != scripts.end()) {
      sq.erase(SCRIPT_PATH, v->second);
      scripts.erase(s);
    } else {
      result << "Failed to remove nonexisting script: " << s << std::endl;
    }
  }
  for (const script_map_type::value_type &e : scripts) {
    result << e.second << std::endl;
  }

  sq.save();
  provider_->get_core()->settings_query(sq.request(), sq.response());
  if (!sq.validate_response())
    nscapi::protobuf::functions::set_response_bad(*response, sq.get_response_error());
  else
    nscapi::protobuf::functions::set_response_good(*response, result.str());
}
