// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "icinga_agent_import_command.hpp"

#include <config.h>

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <sstream>
#include <str/utf8.hpp>
#include <vector>

#include "icinga_agent_import.hpp"
#include "icinga_agent_pki.hpp"
#include "icinga_agent_service.hpp"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
namespace pf = nscapi::protobuf::functions;

namespace icinga_agent {

namespace {

const char *const agent_settings_path = "/settings/Icinga/agent";

struct import_arguments {
  std::string from;
  std::string certificates;
  std::string config;
  import_options options;
  bool dry_run = false;
  bool force = false;
  bool stop_icinga = false;
};

// The agent's key, certificate and CA as read from its certificate folder.
struct identity {
  std::string certificate;
  std::string key;
  std::string ca;
};

// Where the Icinga 2 agent keeps its data: one tree on Windows, the usual
// places under / elsewhere.
std::string default_icinga_root() {
#ifdef WIN32
  const char *program_data = std::getenv("ProgramData");
  return (fs::path(program_data ? program_data : "C:\\ProgramData") / "icinga2").string();
#else
  return "/";
#endif
}

po::options_description describe_options(import_arguments &args) {
  po::options_description desc("nscp icinga-agent import: take over the identity of the Icinga 2 agent on this host");
  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("from", po::value<std::string>(&args.from)->default_value(args.from),
      "The Icinga 2 agent's data directory, holding var/lib/icinga2/certs and etc/icinga2.")
    ("certificates", po::value<std::string>(&args.certificates),
      "The directory with <name>.crt, <name>.key and ca.crt, when it is not <from>/var/lib/icinga2/certs.")
    ("config", po::value<std::string>(&args.config),
      "The directory with constants.conf and zones.conf, when it is not <from>/etc/icinga2.")
    ("endpoint-name", po::value<std::string>(&args.options.endpoint_name),
      "This agent's endpoint name. Default: NodeName in constants.conf, else the name of the only key among the certificates.")
    ("parent-endpoint", po::value<std::string>(&args.options.parent_endpoint),
      "The parent's endpoint name. Default: the one other Endpoint in zones.conf.")
    ("parent-host", po::value<std::string>(&args.options.parent_host),
      "The parent's address. Default: `host` of that Endpoint in zones.conf.")
    ("parent-port", po::value<std::string>(&args.options.parent_port),
      "The parent's port. Default: `port` of that Endpoint, else 5665.")
    ("force", po::value<bool>(&args.force)->implicit_value(true)->default_value(false),
      "Replace a different key that is already in the certificate directory.")
    ("stop-icinga", po::value<bool>(&args.stop_icinga)->implicit_value(true)->default_value(false),
      "After a successful import, stop the icinga2 service and disable it, so it does not connect as the same endpoint again.")
    ("dry-run", po::value<bool>(&args.dry_run)->implicit_value(true)->default_value(false),
      "Show what would be imported and written, and change nothing.")
    ;
  // clang-format on
  return desc;
}

const char *const usage =
    "Usage: nscp icinga-agent import [--from <icinga2 data directory>] [--parent-host <address>] [--stop-icinga] [--dry-run]\n"
    "Copies the Icinga 2 agent's key, certificate and CA into this agent's certificate directory and points\n"
    "/settings/Icinga/agent at the same parent, so NSClient++ connects as the endpoint the master already knows.\n"
    "Stop the Icinga 2 agent before NSClient++ is restarted (--stop-icinga does it): both cannot be connected as\n"
    "the same endpoint.\n";

bool read_file(const fs::path &path, std::string &content) {
  std::ifstream in(path.string().c_str(), std::ios::binary);
  if (!in) return false;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  content = buffer.str();
  return true;
}

std::string required_file(const fs::path &path, const std::string &what) {
  std::string content;
  if (!read_file(path, content)) throw import_error("Cannot read " + what + " at " + path.string());
  return content;
}

// The names of the keys in the folder, without ".key".
std::vector<std::string> key_names(const fs::path &certificates) {
  std::vector<std::string> names;
  boost::system::error_code ec;
  for (fs::directory_iterator it(certificates, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().extension() == ".key") names.push_back(it->path().stem().string());
  }
  return names;
}

// Who the agent is and who it reports to, from its own configuration; a
// file that is not there reads as empty and the options have to say.
import_plan plan_from(const import_arguments &args, const fs::path &certificate_source) {
  const fs::path config_source = args.config.empty() ? fs::path(args.from) / "etc" / "icinga2" : fs::path(args.config);
  std::string constants, zones;
  read_file(config_source / "constants.conf", constants);
  read_file(config_source / "zones.conf", zones);
  const std::string endpoint_name = resolve_endpoint_name(args.options, constants, key_names(certificate_source));
  const import_plan plan = plan_import(args.options, endpoint_name, zones);
  if (!socket_helpers::is_valid_peer_principal(plan.parent_endpoint)) {
    throw import_error("'" + socket_helpers::escape_for_log(plan.parent_endpoint) + "' cannot be used as `parent endpoint`");
  }
  return plan;
}

identity read_identity(const fs::path &certificate_source, const std::string &endpoint_name) {
  identity id;
  id.certificate = required_file(certificate_source / (endpoint_name + ".crt"), "the certificate");
  id.key = required_file(certificate_source / (endpoint_name + ".key"), "the key");
  id.ca = required_file(certificate_source / "ca.crt", "the Icinga CA");
  return id;
}

// An identity NSClient++ already holds under this name is its own: replacing
// it is not something to do by accident.
void refuse_other_key(const pki_paths &paths, const identity &id, const std::string &endpoint_name, const bool force) {
  std::string existing;
  if (force || !read_file(paths.key, existing) || existing == id.key) return;
  throw import_error("There is already a different key for '" + endpoint_name + "' at " + paths.key +
                     ". Importing replaces this agent's own identity: pass --force if that is what you want.");
}

std::string describe(const import_plan &plan, const pki_paths &paths, const fs::path &certificate_source, const bool dry_run) {
  std::stringstream out;
  out << (dry_run ? "Would import" : "Imported") << " the Icinga 2 agent '" << plan.endpoint_name << "' from " << certificate_source.string() << std::endl;
  out << "  key:         " << paths.key << std::endl;
  out << "  certificate: " << paths.certificate << std::endl;
  out << "  CA:          " << paths.ca << std::endl;
  out << "Parent: " << plan.parent_endpoint << " at " << plan.parent_host << ":" << (plan.parent_port.empty() ? "5665" : plan.parent_port) << std::endl;
  return out.str();
}

// Empty when the settings were written, else why not.
std::string write_settings(nscapi::core_wrapper *core, const unsigned int plugin_id, const import_plan &plan) {
  pf::settings_query s(plugin_id);
  s.set(MAIN_MODULES_SECTION, "IcingaAgent", "enabled");
  s.set(agent_settings_path, "endpoint name", plan.endpoint_name);
  s.set(agent_settings_path, "parent endpoint", plan.parent_endpoint);
  s.set(agent_settings_path, "parent host", plan.parent_host);
  if (!plan.parent_port.empty()) s.set(agent_settings_path, "parent port", plan.parent_port);
  // The CA is in place, so there is never a reason to connect unverified.
  s.set(agent_settings_path, "trust on first use", "false");
  s.save();
  core->settings_query(s.request(), s.response());
  return s.validate_response() ? std::string() : s.get_response_error();
}

// The last step, and only after everything else worked: until here the host
// is still monitored by the Icinga agent. False when it could not be stopped.
bool stop_replaced_agent(const std::string &endpoint_name, std::ostream &out) {
  try {
    out << stop_icinga_service() << std::endl;
    out << "Next: restart NSClient++." << std::endl;
    return true;
  } catch (const std::exception &e) {
    out << "The import is complete, but the Icinga 2 agent was not stopped: " << utf8::utf8_from_native(e.what()) << std::endl;
    out << "Stop it yourself before NSClient++ is restarted: both cannot be connected as '" << endpoint_name << "' at once." << std::endl;
    return false;
  }
}

void import(nscapi::core_wrapper *core, const unsigned int plugin_id, const std::string &certificate_directory, const import_arguments &args,
            PB::Commands::ExecuteResponseMessage::Response *response) {
  const fs::path certificate_source = args.certificates.empty() ? fs::path(args.from) / "var" / "lib" / "icinga2" / "certs" : fs::path(args.certificates);
  const import_plan plan = plan_from(args, certificate_source);
  const identity id = read_identity(certificate_source, plan.endpoint_name);
  if (certificate_directory.empty()) throw import_error("No `certificate directory` is configured under /settings/Icinga/agent");
  const pki_paths paths = pki_paths::for_endpoint(certificate_directory, plan.endpoint_name);
  refuse_other_key(paths, id, plan.endpoint_name, args.force);

  std::stringstream result;
  result << describe(plan, paths, certificate_source, args.dry_run);
  if (args.dry_run) {
    if (args.stop_icinga) result << "Would stop and disable the icinga2 service." << std::endl;
    result << "Nothing was changed (--dry-run)." << std::endl;
    pf::set_response_good(*response, result.str());
    return;
  }

  import_identity(paths, plan.endpoint_name, id.certificate, id.key, id.ca);
  const std::string settings_error = write_settings(core, plugin_id, plan);
  if (!settings_error.empty()) {
    pf::set_response_bad(*response, "The certificates were imported, but the settings could not be written: " + settings_error);
    return;
  }
  result << "Settings written to " << agent_settings_path << "; the IcingaAgent module is enabled." << std::endl;
  result << "The Icinga 2 agent's copy of the key is still at " << (certificate_source / (plan.endpoint_name + ".key")).string()
         << ": delete it once that agent is uninstalled." << std::endl;
  if (args.stop_icinga) {
    if (!stop_replaced_agent(plan.endpoint_name, result)) {
      pf::set_response_bad(*response, result.str());
      return;
    }
  } else {
    result << "Next: stop the Icinga 2 agent, then restart NSClient++. Both cannot be connected as '" << plan.endpoint_name << "' at once." << std::endl;
  }
  pf::set_response_good(*response, result.str());
}

}  // namespace

bool run_import_command(nscapi::core_wrapper *core, const unsigned int plugin_id, const std::string &certificate_directory,
                        const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  import_arguments args;
  args.from = default_icinga_root();
  po::options_description desc = describe_options(args);
  try {
    // Everything after the `import` word.
    std::vector<std::string> tokens;
    for (int i = 1; i < request.arguments_size(); ++i) tokens.push_back(request.arguments(i));
    po::variables_map vm;
    po::store(nscapi::program_options::basic_command_line_parser(tokens).options(desc).run(), vm);
    po::notify(vm);
    if (vm.count("help")) {
      pf::set_response_good(*response, nscapi::program_options::help(desc, usage));
      return true;
    }
    import(core, plugin_id, certificate_directory, args, response);
  } catch (const import_error &e) {
    pf::set_response_bad(*response, e.what());
  } catch (const std::runtime_error &e) {
    pf::set_response_bad(*response, std::string("Not imported: ") + utf8::utf8_from_native(e.what()));
  } catch (...) {
    nscapi::program_options::report_command_exception(desc, request.command(), *response, "Failed to import the Icinga 2 agent");
  }
  return true;
}

}  // namespace icinga_agent
