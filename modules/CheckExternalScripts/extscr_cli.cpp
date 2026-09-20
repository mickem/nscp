// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "extscr_cli.h"

#include <config.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <string>

#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;
namespace pf = nscapi::protobuf::functions;
namespace npo = nscapi::program_options;
namespace json = boost::json;

extscr_cli::extscr_cli(const std::shared_ptr<script_provider_interface> &provider) : provider_(provider) {}

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

bool extscr_cli::validate_sandbox(boost::filesystem::path pscript, PB::Commands::ExecuteResponseMessage::Response *response) {
  boost::filesystem::path path = provider_->get_root();
  // Resolve symlinks before the containment check. path_contains_file compares
  // lexically only, so a symlink placed inside the script root but pointing
  // outside it would otherwise pass, letting show/delete read or remove files
  // anywhere the service account can reach. weakly_canonical resolves the real
  // targets (the candidate has already been confirmed to be a regular file by
  // the callers); fall back to the lexical path if resolution fails.
  boost::system::error_code ec;
  boost::filesystem::path real_root = boost::filesystem::weakly_canonical(path, ec);
  if (ec) real_root = path;
  boost::filesystem::path real_script = boost::filesystem::weakly_canonical(pscript, ec);
  if (ec) real_script = pscript;
  if (!file_helpers::checks::path_contains_file(real_root, real_script)) {
    nscapi::protobuf::functions::set_response_bad(*response, "Not allowed outside: " + path.string());
    return false;
  }
  return true;
}

namespace {
// weakly_canonical, falling back to the lexical path when resolution fails -
// the same treatment validate_sandbox gives both sides of its comparison.
boost::filesystem::path resolve(const boost::filesystem::path &path) {
  boost::system::error_code ec;
  const boost::filesystem::path resolved = boost::filesystem::weakly_canonical(path, ec);
  return ec ? path : resolved;
}

// True when this file is one the service itself created and nobody else can
// rewrite.
//
// Needed only for ${temp}. The script root and ${shared-path} are the agent's
// own directories, but ${temp} is the *shared* temp directory - `/tmp`, or
// `C:\Windows\Temp` for a SYSTEM service - which is why the REST upload route
// stages there under a random, exclusively created, owner-only name (see
// upload_staging.hpp). Accepting any path under ${temp}, as this first did,
// threw that away: a local account can drop a file in `/tmp` and have it
// imported and registered as a command that runs as the service account.
//
// Ownership is the discriminator, not the name - a name pattern is something
// an attacker can simply match. On Windows this returns true and the path is
// admitted on the directory check alone; expressing "created by us" there
// means reading the DACL, and the honest thing is to leave that to a change
// that can be tested on Windows rather than guess at it here.
bool is_our_own_file(const boost::filesystem::path &path) {
#ifdef WIN32
  (void)path;
  return true;
#else
  struct stat st = {};
  if (::stat(path.string().c_str(), &st) != 0) return false;
  if (!S_ISREG(st.st_mode)) return false;
  if (st.st_uid != ::geteuid()) return false;
  // Writable by anyone else means the content can still change between this
  // check and the copy below.
  return (st.st_mode & (S_IWGRP | S_IWOTH)) == 0;
#endif
}
}  // namespace

bool extscr_cli::validate_import_source(const boost::filesystem::path &source, PB::Commands::ExecuteResponseMessage::Response *response) {
  const boost::filesystem::path real_source = resolve(source);
  // The script root and ${shared-path} are the agent's own directories: being
  // inside one is enough.
  const std::string owned_roots[] = {provider_->get_root().string(), provider_->get_core()->expand_path("${shared-path}")};
  for (const std::string &root : owned_roots) {
    if (root.empty()) continue;
    if (file_helpers::checks::path_contains_file(resolve(root), real_source)) return true;
  }
  // ${temp} is where the REST PUT /api/v2/scripts route stages an upload before
  // handing it to `add --import`, so it has to be reachable - but it is shared
  // with every local account, so being inside it proves nothing on its own. The
  // staged file is ours and owner-only; a planted one is not.
  const std::string temp_root = provider_->get_core()->expand_path("${temp}");
  if (!temp_root.empty() && file_helpers::checks::path_contains_file(resolve(temp_root), real_source) && is_our_own_file(real_source)) {
    return true;
  }
  // Deliberately without the resolved path or the list of roots: naming them
  // would answer "where does this agent keep its scripts" and "does this path
  // exist" for a caller who is being refused.
  nscapi::protobuf::functions::set_response_bad(
      *response, "Importing is only allowed from the script folder, ${shared-path} or the upload staging area. Copy the script there first.");
  return false;
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
    for (const std::string &cmd : provider_->get_commands()) {
      if (json) {
        data.push_back(json::value(cmd));
      } else {
        resp += cmd + "\n";
      }
    }
  } else {
    boost::filesystem::path dir = provider_->get_core()->expand_path("${scripts}");
    boost::filesystem::path rel = provider_->get_core()->expand_path("${base-path}");
    boost::filesystem::recursive_directory_iterator iter(dir), eod;
    for (boost::filesystem::path const &i : boost::make_iterator_range(iter, eod)) {
      std::string s = i.string();
      // Relative to the install base when the file is under it, which is the
      // case on Windows and is what `show` and the web UI's script list have
      // always been handed. When it is not - the normal case on unix, where
      // ${scripts} is not below ${base-path} - the path is left absolute.
      // It used to have its leading separator sliced off regardless, leaving a
      // rootless `usr/lib/nsclient/scripts/x` that named no file at all.
      if (boost::algorithm::starts_with(s, rel.string())) {
        s = s.substr(rel.string().size());
        if (!s.empty() && (s[0] == '\\' || s[0] == '/')) s = s.substr(1);
      }
      if (s.empty()) continue;
      // Skip scripts under a `lib` folder unless --include-lib was given. The
      // previous test used a substring match on the whole path (so a `libs` or
      // `calibrate` folder was wrongly excluded) AND never consulted the parsed
      // `lib` flag, so --include-lib did nothing. Match a path *component* named
      // exactly `lib`, and honour the flag.
      bool in_lib = false;
      for (const boost::filesystem::path &part : i) {
        if (part.string() == "lib") {
          in_lib = true;
          break;
        }
      }
      if (boost::filesystem::is_regular_file(i) && (lib || !in_lib)) {
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

void extscr_cli::show(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string script;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("script", po::value<std::string>(&script),
    "Script to show.")
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

  commands::command_object_instance command_def = provider_->find_command(script);
  if (command_def) {
    nscapi::protobuf::functions::set_response_good(*response, command_def->command);
  } else {
    boost::filesystem::path pscript = script;
    bool found = boost::filesystem::is_regular_file(pscript);
    if (!found) {
      pscript = provider_->get_core()->expand_path("${base-path}/" + script);
      found = boost::filesystem::is_regular_file(pscript);
    }
#ifdef WIN32
    if (!found) {
      pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
      found = boost::filesystem::is_regular_file(pscript);
    }
#endif
    if (found) {
      if (!validate_sandbox(pscript, response)) {
        return;
      }

      std::ifstream t(pscript.string().c_str());
      std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

      nscapi::protobuf::functions::set_response_good(*response, str);
    } else {
      nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
    }
  }
}

void extscr_cli::delete_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string script;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("script", po::value<std::string>(&script),
    "Script to delete.")
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

  commands::command_object_instance command_def = provider_->find_command(script);
  if (command_def) {
    provider_->remove_command(script);

    nscapi::protobuf::functions::settings_query s(provider_->get_id());
    s.save();
    provider_->get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return;
    }
    nscapi::protobuf::functions::set_response_good(*response,
                                                   "Script definition has been removed don't forget to delete any artifact for: " + command_def->command);
  } else {
    boost::filesystem::path pscript = script;
    bool found = boost::filesystem::is_regular_file(pscript);
    if (!found) {
      pscript = provider_->get_core()->expand_path("${base-path}/" + script);
      found = boost::filesystem::is_regular_file(pscript);
    }
#ifdef WIN32
    if (!found) {
      pscript = boost::algorithm::replace_all_copy(script, "/", "\\");
      found = boost::filesystem::is_regular_file(pscript);
    }
#endif
    if (found) {
      if (!validate_sandbox(pscript, response)) {
        return;
      }
      boost::filesystem::remove(pscript);
      nscapi::protobuf::functions::set_response_good(*response, "Script file was removed");
    } else {
      nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + script);
    }
  }
}

void extscr_cli::add_script(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string script, arguments, alias, import_script;
  bool wrapped = false, list = false, replace = false, no_config = false;

  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("script", po::value<std::string>(&script), "Script to add")
    ("alias", po::value<std::string>(&alias), "Name of command to execute script (defaults to basename of script)")
    ("arguments", po::value<std::string>(&arguments), "Arguments for script.")
    ("list", po::bool_switch(&list), "List all scripts in the scripts folder.")
    ("wrapped", po::bool_switch(&wrapped), "Add this to add a wrapped script such as ps1, vbs or similar..")
    ("import", po::value<std::string>(&import_script), "Import (copy to script folder) a script.")
    ("replace", po::bool_switch(&replace), "Used when importing to specify that the script will be overwritten.")
    ("no-config", po::bool_switch(&no_config), "Do not write the updated configuration (i.e. changes are only transient).")
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
    // The destination name comes from --script, not from --import: with it
    // empty the join below produced the script root itself, and the import
    // copied over that directory path instead of into it.
    nscapi::protobuf::functions::set_response_bad(*response, "No script specified add --script");
    return;
  }
  boost::filesystem::path file = provider_->get_core()->expand_path(script);
  boost::filesystem::path script_root = provider_->get_root();

  if (!import_script.empty()) {
    // The sandbox that keeps `show` and `delete` inside the script root is
    // only worth anything if nothing can be carried into it first: `add
    // --import /etc/shadow` followed by `show` is otherwise an arbitrary file
    // read with the agent's privileges, for the same principals that reach
    // the legacy /exec route.
    if (!validate_import_source(provider_->get_core()->expand_path(import_script), response)) return;
    file = script_root / file_helpers::meta::get_filename(file);
    // What gets recorded has to be runnable as-is, because this module
    // resolves nothing: the value is handed to the shell (or to execvp)
    // verbatim, with no ${...} expansion and no search of the script folder.
    //
    // On Windows the launcher starts the child with ${base-path} as its
    // working directory and ${scripts} sits directly below it, so the
    // historical relative spelling resolves and is kept - it is what every
    // existing nsclient.ini and the web UI's script list show.
    //
    // Everywhere else it resolved to nothing at all: the backslash is an
    // ordinary filename character on unix, ${scripts} is not below
    // ${base-path} (/usr/lib/nsclient/scripts versus /usr/sbin), and the unix
    // launcher does not set a working directory for the child in the first
    // place - so the imported script exited 127 the moment it was run.
    // Record where the file actually is instead.
#ifdef WIN32
    script = "scripts\\" + file_helpers::meta::get_filename(file);
#else
    // Quoted when it has to be: the recorded value is a command line, and
    // parse_command tokenises it with boost::escaped_list_separator on spaces
    // (with `"` as the quote character). An unquoted /opt/my scripts/x.sh would
    // split into two argv entries and exit 127 - the very symptom recording an
    // absolute path is meant to cure. Quote only when there is a space, so the
    // ordinary case still reads as a plain path in nsclient.ini.
    script = file.string();
    if (script.find(' ') != std::string::npos) script = "\"" + script + "\"";
#endif
    if (boost::filesystem::exists(file)) {
      if (replace) {
        boost::filesystem::remove(file);
      } else {
        nscapi::protobuf::functions::set_response_bad(*response, "Script already exists specify --overwrite to replace the script");
        return;
      }
    }
    try {
      // copy_file does not create the destination directory, and nothing
      // guarantees ${scripts} exists: on Windows it only materialises when the
      // sample scripts feature is selected, and a ${scripts} override points
      // somewhere that was never populated at all. Without this the import
      // failed with a bare "No such file or directory" naming a path the
      // operator had no reason to create by hand.
      boost::filesystem::create_directories(file.parent_path());
      boost::filesystem::copy_file(import_script, file);
    } catch (const std::exception &e) {
      nscapi::protobuf::functions::set_response_bad(*response, "Failed to import script: " + utf8::utf8_from_native(e.what()));
      return;
    }
  }

  if (!wrapped) {
    bool found = boost::filesystem::is_regular_file(file);
    if (!found) {
      file = file = provider_->get_core()->expand_path("${shared-path}/" + file.string());
      found = boost::filesystem::is_regular_file(file);
    }
    if (!found) {
      nscapi::protobuf::functions::set_response_bad(*response, "Script not found: " + file.string());
      return;
    }
  }
  if (alias.empty()) {
    alias = file.filename().stem().string();
  }

  // The command line stored/registered for this script. Only add the separator
  // space when there actually are arguments, so an argument-less add does not
  // leave a trailing space in the stored value, the alias description, or the
  // transient command. Previously the transient registration used `script`
  // alone, so `add --no-config` (and the in-memory copy on a normal add)
  // silently dropped the `--arguments` the operator supplied.
  const std::string command_line = arguments.empty() ? script : (script + " " + arguments);

  if (!no_config) {
    nscapi::protobuf::functions::settings_query s(provider_->get_id());
    if (!wrapped)
      s.set("/settings/external scripts/scripts", alias, command_line);
    else
      s.set("/settings/external scripts/wrapped scripts", alias, command_line);
    s.set(MAIN_MODULES_SECTION, "CheckExternalScripts", "enabled");
    s.save();
    provider_->get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return;
    }
  }
  std::string actual = "";
  if (wrapped)
    actual = "\nActual command is: " + provider_->generate_wrapped_command(command_line);
  else {
    provider_->add_command(alias, command_line);
    nscapi::core_helper core(provider_->get_core(), provider_->get_id());
    core.register_command(alias, "Alias for: " + command_line);
  }
  nscapi::protobuf::functions::set_response_good(*response, "Added " + alias + " as " + script + actual);
}

void extscr_cli::configure(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  po::variables_map vm;
  po::options_description desc;
  std::string arguments = "false";
  // The module reads `allow arguments` / `allow nasty characters` from
  // `/settings/external scripts` (see CheckExternalScripts::loadModuleEx). The
  // previous `/settings/external scripts/server` path was never read by anyone,
  // so this tool reported and "applied" a lockdown that had no effect - a
  // fail-dangerous mismatch (running `install --arguments=false` left an
  // existing `allow arguments=true` in force). Write to the path the module
  // actually consults.
  const std::string path = "/settings/external scripts";

  pf::settings_query q(provider_->get_id());
  q.get(path, "allow arguments", false);
  q.get(path, "allow nasty characters", false);

  provider_->get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
    return;
  }
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (val.matches(path, "allow arguments") && val.get_bool())
      arguments = "true";
    else if (val.matches(path, "allow nasty characters") && val.get_bool())
      arguments = "safe";
  }
  desc.add_options()("help", "Show help.")("arguments", po::value<std::string>(&arguments)->default_value(arguments)->implicit_value("safe"),
                                           "Allow arguments. false=don't allow, safe=allow non escape chars, all=allow all arguments.");

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

  nscapi::protobuf::functions::settings_query s(provider_->get_id());
  s.set(MAIN_MODULES_SECTION, "CheckExternalScripts", "enabled");
  if (arguments == "all" || arguments == "unsafe") {
    result << "UNSAFE Arguments are allowed." << std::endl;
    s.set(path, "allow arguments", "true");
    s.set(path, "allow nasty characters", "true");
  } else if (arguments == "safe" || arguments == "true") {
    result << "SAFE Arguments are allowed." << std::endl;
    s.set(path, "allow arguments", "true");
    s.set(path, "allow nasty characters", "false");
  } else {
    result << "Arguments are NOT allowed." << std::endl;
    s.set(path, "allow arguments", "false");
    s.set(path, "allow nasty characters", "false");
  }
  s.save();
  provider_->get_core()->settings_query(s.request(), s.response());
  if (!s.validate_response())
    nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
  else
    nscapi::protobuf::functions::set_response_good(*response, result.str());
}
