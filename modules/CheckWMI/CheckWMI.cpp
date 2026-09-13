// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "CheckWMI.h"

#include <algorithm>
#include <boost/program_options.hpp>
#include <check/wql_query.hpp>
#include <map>
#include <memory>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/xtos.hpp>
#include <utility>
#include <vector>
#include <win/wmi/wmi_query.hpp>

namespace sh = nscapi::settings_helper;
namespace po = boost::program_options;

bool CheckWMI::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode) {
  try {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias(alias, "wmi");

    targets.set_path(settings.alias().get_settings_path("targets"));

    // A reload calls loadModuleEx again on the live module and the predefined
    // queries below are appended, so drop them or every reload doubles the
    // list. The modes and allow lists are replaced by their callbacks and stay
    // in force meanwhile.
    query_access_.reset();
    class_access_.reset();
    namespace_access_.reset();

    // clang-format off
    settings.alias().add_path_to_settings()
      ("targets", sh::fun_values_path([this] (const auto& key, const auto& value) { targets.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, value); }),
        "TARGET LIST SECTION", "A list of available remote target systems",
        "TARGET DEFINITION", "For more configuration options add a dedicated section")

      ("queries", sh::fun_values_path([this] (const auto& key, const auto& value) { query_access_.add_predefined(key, value); }),
        "PREDEFINED WMI QUERIES", "WMI queries check_wmi may run by name, as <name> = <query>.\n"
        "A name defined here can be used as query=<name> in any access mode, and is the only thing accepted when "
        "'query access' is set to predefined. The query is not parsed: an operator who writes it here has vouched for it.")
      ;
    // clang-format on

    settings.alias()
        .add_key_to_settings()

        .add_string("query access", sh::string_fun_key([this](const auto& value) { query_access_.set_mode(value); }, "any"), "WMI QUERY ACCESS MODE",
                    "Which WMI queries a caller may ask check_wmi to run: any (the default - any query the caller sends, which is how every earlier release "
                    "behaved), allowed (only a plain SELECT whose class matches 'allowed classes') or predefined (only names defined in the "
                    "[/settings/wmi/queries] section).\n"
                    "WMI reaches most of what the machine knows, including the filesystem through Win32_Directory and CIM_DataFile, so on a host where "
                    "callers may pass arguments (NRPE with 'allow arguments', or the REST API) this decides how much of it a check can read. See the "
                    "'Restricting what a check may read' section of the documentation.")

        .add_string("allowed classes", sh::string_fun_key([this](const auto& value) { class_access_.set_allow_list(value); }, ""), "ALLOWED WMI CLASSES",
                    "Comma separated list of WMI classes check_wmi may read when 'query access' is set to allowed. Entries may contain * and ?, for example "
                    "Win32_Service, Win32_PerfFormattedData_*.\n"
                    "Only a plain 'SELECT ... FROM <class> [WHERE ...]' can be checked this way. Anything else - ASSOCIATORS OF, REFERENCES OF, a class path "
                    "carrying a namespace - is refused rather than guessed at, and has to be configured as a predefined query instead.")

        .add_string("allowed namespaces", sh::string_fun_key([this](const auto& value) { namespace_access_.set_allow_list(value); }, ""),
                    "ALLOWED WMI NAMESPACES",
                    "Comma separated list of WMI namespaces check_wmi may bind to when 'query access' is not any. Entries may contain * and ?.\n"
                    "Leaving this empty means the caller may not change the namespace at all: only the default root\\cimv2 is used. It has no effect in the "
                    "default any mode.");

    settings.register_all();
    settings.notify();

    if (!query_access_.get_config_error().empty()) NSC_LOG_ERROR_STD(query_access_.get_config_error());

    targets.finalize(nscapi::settings_proxy::create(get_id(), get_core()));
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("loading: ", e);
    return false;
  } catch (...) {
    NSC_LOG_ERROR_EX("loading: ");
    return false;
  }
  return true;
}
bool CheckWMI::unloadModule() { return true; }

std::string build_namespace(std::string ns, const std::string &computer) {
  if (ns.empty()) ns = "root\\cimv2";
  if (!computer.empty()) ns = "\\\\" + computer + "\\" + ns;
  return ns;
}

// Decide whether the caller may bind to the namespace it asked for, and build
// the string WMI connects to.
//
// The rule is deliberately blunt when 'allowed namespaces' is empty: rather
// than leave the namespace open while the class is restricted - which would
// let the same class name be read from a different provider - an empty list in
// a restricted mode means the namespace may not be moved off the default at
// all. In the default `any` mode nothing is checked.
bool CheckWMI::resolve_namespace(const std::string &requested, const std::string &computer, std::string &out, std::string &error) const {
  const std::string ns = requested.empty() ? "root\\cimv2" : requested;
  if (query_access_.get_mode() != check::access::mode::any) {
    if (namespace_access_.allow_list_size() == 0) {
      if (!boost::algorithm::iequals(ns, "root\\cimv2")) {
        error = "Refusing namespace '" + ns +
                "': 'allowed namespaces' is empty in [/settings/wmi], so only the default root\\cimv2 may be used while 'query access' is restricted"
                " (list it there to permit it - a predefined query reading another namespace needs that too)";
        return false;
      }
    } else {
      const check::access::decision d = namespace_access_.check_value(ns, "namespace");
      if (!d.allowed) {
        error = d.error;
        return false;
      }
    }
  }
  out = build_namespace(ns, computer);
  return true;
}

#include <parsers/filter/cli_helper.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/where/filter_handler_impl.hpp>
#include <parsers/where/node.hpp>

namespace wmi_filter {
struct filter_obj {
  wmi_impl::row row;
  explicit filter_obj(wmi_impl::row row) : row(std::move(row)) {}

  std::string show() const { return row.to_string(); }

  std::string get_string(const std::string &col) const { return row.get_string(col); }
  std::string get_row() const { return row.to_string(); }
  long long get_int(const std::string &col) const { return row.get_int(col); }
};

typedef parsers::where::filter_handler_impl<std::shared_ptr<filter_obj> > native_context;
struct filter_obj_handler : native_context {
  filter_obj_handler() = default;
};
typedef modern_filter::modern_filters<filter_obj, filter_obj_handler> filter;
}  // namespace wmi_filter
void CheckWMI::check_wmi(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response) {
  typedef wmi_filter::filter filter_type;
  modern_filter::data_container data;
  modern_filter::cli_helper<filter_type> filter_helper(request, response, data);
  std::string given_target;
  target_helper::target_info target_info;
  boost::optional<target_helper::target_info> t;
  std::string query, ns = "root\\cimv2";

  // clang-format off
  filter_type filter;
  filter_helper.add_options("", "", "", filter.get_filter_syntax(), "ignored");
  filter_helper.add_syntax("${list}", "%(line)", "", "", "");
  filter_helper.get_desc().add_options()
    ("target", po::value<std::string>(&given_target), "The target to check (for checking remote machines).")
    ("user", po::value<std::string>(&target_info.username), "Remote username when checking remote machines.")
    ("password", po::value<std::string>(&target_info.password), "Remote password when checking remote machines.")
    ("namespace", po::value<std::string>(&ns)->default_value("root\\cimv2"), "The WMI root namespace to bind to.\n"
      "While 'query access' in [/settings/wmi] is restricted this must match 'allowed namespaces', and may not be changed at all when that list is empty.")
    ("query", po::value<std::string>(&query), "The WMI query to execute.\n"
      "Which queries may be run here is governed by 'query access' in [/settings/wmi]: by default any query is run, but an operator can restrict this to "
      "queries reading an allowed class, or to names predefined in [/settings/wmi/queries], in which case this takes such a name.")
    ;
  // clang-format on

  if (!filter_helper.parse_options()) return;

  if (query.empty()) return nscapi::protobuf::functions::set_response_bad(*response, "No query specified");

  // Hold the query against [/settings/wmi] 'query access' before WMI is touched.
  //
  // This does not go through policy::resolve() the way the other checks do,
  // because `allowed` means something different here: there is no list of
  // permitted query *texts* to match against - a query is judged by the class
  // it reads, which means it also has to be simple enough to say which class
  // that is. Only the mode and the predefined names come from the policy.
  std::string predefined_query;
  if (query_access_.lookup_predefined(query, predefined_query)) {
    // Operator-authored, so it is trusted as written: not parsed, and not held
    // against 'allowed classes'.
    query = predefined_query;
  } else if (!query_access_.get_config_error().empty()) {
    return nscapi::protobuf::functions::set_response_bad(*response, query_access_.get_config_error());
  } else if (query_access_.get_mode() == check::access::mode::predefined) {
    return nscapi::protobuf::functions::set_response_bad(
        *response, "Refusing query '" + query +
                       "': 'query access' is set to predefined, so only a query defined in [/settings/wmi/queries] may be used (see [/settings/wmi] in "
                       "the configuration)");
  } else if (query_access_.get_mode() == check::access::mode::allowed) {
    const check::wql::parse_result parsed = check::wql::extract_class(query);
    if (!parsed.ok) {
      return nscapi::protobuf::functions::set_response_bad(*response, "Refusing query: " + parsed.error + " (see [/settings/wmi] in the configuration)");
    }
    const check::access::decision d = class_access_.check_value(parsed.class_name, "WMI class");
    if (!d.allowed) return nscapi::protobuf::functions::set_response_bad(*response, d.error);
  }

  if (!given_target.empty()) {
    t = targets.find(given_target);
    if (t) {
      target_info.update_from(t.value());
    } else if (query_access_.get_mode() != check::access::mode::any) {
      // An unknown target is otherwise taken as a bare host name, which would
      // let a caller point a restricted check at a machine of its choosing
      // (and hand it the configured credentials). While access is restricted,
      // only a target defined in [/settings/wmi/targets] is accepted.
      return nscapi::protobuf::functions::set_response_bad(
          *response, "Refusing target '" + given_target +
                         "': it is not defined in [/settings/wmi/targets], and 'query access' is restricted (see [/settings/wmi] in the configuration)");
    } else {
      target_info.hostname = given_target;
    }
  }

  {
    std::string resolved_ns, error;
    if (!resolve_namespace(ns, target_info.hostname, resolved_ns, error)) return nscapi::protobuf::functions::set_response_bad(*response, error);
    ns = resolved_ns;
  }

  try {
    wmi_impl::query wmiQuery(query, ns, target_info.username, target_info.password);
    filter.context->registry_.add_string_var("line", &wmi_filter::filter_obj::get_row, "Get a list of all columns");
    for (const std::string &col : wmiQuery.get_columns()) {
      filter.context->registry_
          .add_int_var(
              col, [col](const auto &obj) { return obj->get_int(col); }, [col](auto obj) { return obj->get_string(col); }, "Column: " + col)
          .add_int_perf("", col, "");
    }

    if (!filter_helper.build_filter(filter)) return;

    wmi_impl::row_enumerator e = wmiQuery.execute();
    while (e.has_next()) {
      std::shared_ptr<wmi_filter::filter_obj> record = std::make_shared<wmi_filter::filter_obj>(e.get_next());
      filter.match(record);
    }
    filter_helper.post_process(filter);
  } catch (const wmi_impl::wmi_exception &e) {
    return nscapi::protobuf::functions::set_response_bad(*response, "WMIQuery failed: " + e.reason());
  }
}

inline std::string pad(const std::string &s, const std::size_t &c) { return s + std::string(c - s.size(), ' '); }

typedef std::vector<std::string> row_type;
std::string render_table(const std::vector<std::size_t> &widths, const row_type &headers, const std::list<row_type> &rows) {
  const auto count = widths.size();
  std::stringstream ss;
  std::string line;
  for (int i = 0; i < count; ++i) {
    line += std::string(widths[i] + 3, '-');
  }
  ss << line << "\n";
  if (headers.size() != widths.size()) throw wmi_impl::wmi_exception(E_INVALIDARG, "Invalid header size");
  for (int i = 0; i < count; ++i) ss << " " << pad(headers[i], widths[i]) << " ";
  ss << "\n" << line << "\n";
  for (const row_type &row : rows) {
    if (row.size() != widths.size()) throw wmi_impl::wmi_exception(E_INVALIDARG, "Invalid row size");
    for (int i = 0; i < count; ++i) ss << " " << pad(row[i], widths[i]) << " ";
    ss << "\n";
  }
  ss << line;
  return ss.str();
}

std::string render(const row_type &headers, std::vector<std::size_t> &widths, wmi_impl::row_enumerator e) {
  std::list<row_type> rows;
  std::size_t count = widths.size();
  while (e.has_next()) {
    wmi_impl::row wmi_row = e.get_next();
    row_type row;
    for (std::size_t i = 0; i < count; i++) {
      std::string c = wmi_row.get_string(headers[i]);
      widths[i] = (std::max)(c.size(), widths[i]);
      row.push_back(c);
    }
    rows.push_back(row);
  }
  return render_table(widths, headers, rows);
}

std::string list_ns_rec(const std::string &ns, const std::string &user, const std::string &password) {
  std::stringstream ss;
  wmi_impl::instances impl("__Namespace", ns, user, password);
  wmi_impl::row_enumerator e = impl.get();
  while (e.has_next()) {
    wmi_impl::row wmi_row = e.get_next();
    std::string str = wmi_row.get_string("Name");
    ss << ns << "\\" << str << "\n";
    // Some children enumerate via __Namespace but refuse ConnectServer — e.g.
    // root\CIMV2\mdm\dmmap on Windows 11 boxes that aren't MDM-enrolled. Skip
    // unreachable children so one bad subtree doesn't abort the whole walk.
    try {
      ss << list_ns_rec(ns + "\\" + str, user, password);
    } catch (const wmi_impl::wmi_exception &) {
    }
  }
  return ss.str();
}

NSCAPI::nagiosReturn CheckWMI::commandLineExec(const int _target_mode, const std::string &command, const std::list<std::string> &arguments,
                                               std::string &result) {
  try {
    if (command == "wmi" || command == "help" || command.empty()) {
      namespace po = boost::program_options;

      std::string query, ns, user, password, list_cls, list_inst;
      std::string computer;
      bool simple;
      int limit = -1;
      po::options_description desc("Allowed options");
      // clang-format off
      desc.add_options()
	("help,h", "Show help screen")
	("select,s", po::value<std::string>(&query), "Execute a query")
	("simple", "Use simple format")
	("list-classes", po::value<std::string>(&list_cls)->implicit_value(""), "list all classes of a given type")
	("list-instances", po::value<std::string>(&list_inst), "list all instances of a given type")
	("list-ns", "list all name spaces")
	("list-all-ns", "list all name spaces recursively")
	("limit,l", po::value<int>(&limit), "Limit number of rows")
	("namespace,n", po::value<std::string>(&ns)->default_value("root\\cimv2"), "Namespace")
	("computer,c", po::value<std::string>(&computer), "A remote computer to connect to ")
	("user,u", po::value<std::string>(&user), "The user for the remote computer")
	("password,p", po::value<std::string>(&password), "The password for the remote computer")
	;
      // clang-format on

      boost::program_options::variables_map vm;

      if (command == "help") {
        std::stringstream ss;
        ss << "wmi Command line syntax:" << std::endl;
        ss << desc;
        result = ss.str();
        return NSCAPI::exec_return_codes::returnOK;
      }

      std::vector<std::string> args(arguments.begin(), arguments.end());
      po::parsed_options parsed = po::basic_command_line_parser<char>(args).options(desc).run();
      po::store(parsed, vm);
      po::notify(vm);

      if (vm.count("help") || (vm.count("select") == 0 && vm.count("list-classes") == 0 && vm.count("list-instances") == 0 && vm.count("list-ns") == 0 &&
                               vm.count("list-all-ns") == 0)) {
        std::stringstream ss;
        ss << "CheckWMI Command line syntax:" << std::endl;
        ss << desc;
        result = ss.str();
        return NSCAPI::exec_return_codes::returnOK;
      }
      simple = vm.count("simple") > 0;

      ns = build_namespace(ns, computer);

      if (vm.count("select")) {
        try {
          std::vector<std::size_t> widths;
          row_type headers;
          wmi_impl::query wmiQuery(query, ns, user, password);
          std::list<std::string> cols = wmiQuery.get_columns();
          for (const std::string &col : cols) {
            headers.push_back(col);
            widths.push_back(col.size());
          }
          result = render(headers, widths, wmiQuery.execute());
          return NSCAPI::exec_return_codes::returnOK;
        } catch (const wmi_impl::wmi_exception &e) {
          result += "ERROR: " + e.reason();
          return NSCAPI::exec_return_codes::returnERROR;
        }
      }
      if (vm.count("list-classes")) {
        try {
          std::stringstream ss;
          wmi_impl::classes wmi_query(list_cls, ns, user, password);
          wmi_impl::row_enumerator e = wmi_query.get();
          while (e.has_next()) {
            wmi_impl::row wmi_row = e.get_next();
            ss << wmi_row.get_string("__CLASS") << "\n";
          }
          result = ss.str();
          return NSCAPI::exec_return_codes::returnOK;
        } catch (const wmi_impl::wmi_exception &e) {
          result += "ERROR: " + e.reason();
          return NSCAPI::exec_return_codes::returnERROR;
        }
      }
      if (vm.count("list-instances")) {
        try {
          std::stringstream ss;
          wmi_impl::instances wmi_query(list_inst, ns, user, password);
          wmi_impl::row_enumerator e = wmi_query.get();
          while (e.has_next()) {
            wmi_impl::row wmi_row = e.get_next();
            ss << wmi_row.get_string("Name") << "\n";
          }
          result = ss.str();
          return NSCAPI::exec_return_codes::returnOK;
        } catch (const wmi_impl::wmi_exception &e) {
          result += "ERROR: " + e.reason();
          return NSCAPI::exec_return_codes::returnERROR;
        }
      }
      if (vm.count("list-ns")) {
        try {
          std::stringstream ss;
          wmi_impl::instances wmi_query("__Namespace", ns, user, password);
          wmi_impl::row_enumerator e = wmi_query.get();
          while (e.has_next()) {
            wmi_impl::row wmi_row = e.get_next();
            ss << wmi_row.get_string("Name") << "\n";
          }
          result = ss.str();
          return NSCAPI::exec_return_codes::returnOK;
        } catch (wmi_impl::wmi_exception &e) {
          NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
          result += "ERROR: " + e.reason();
          return NSCAPI::exec_return_codes::returnERROR;
        }
      }
      if (vm.count("list-all-ns")) {
        try {
          result = list_ns_rec(ns, user, password);
        } catch (wmi_impl::wmi_exception &e) {
          NSC_LOG_ERROR_EXR("WMIQuery failed: ", e);
          result += "ERROR: " + e.reason();
          return NSCAPI::exec_return_codes::returnERROR;
        }
      }
      return NSCAPI::exec_return_codes::returnOK;
    }
    return NSCAPI::cmd_return_codes::returnIgnored;
  } catch (std::exception &e) {
    result += "ERROR: " + utf8::utf8_from_native(e.what());
    return NSCAPI::exec_return_codes::returnERROR;
  }
}