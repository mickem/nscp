// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// #define _WIN32_WINNT 0x0500

// clang-format off
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include <windows.h>
// clang-format on

#include <Sddl.h>
#include <config.h>
#include <msi.h>
#include <msiquery.h>
#include <openssl/rand.h>

#include <boost/algorithm/string.hpp>
#include <error/error.hpp>
#include <file_helpers.hpp>
#include <fstream>
#include <iterator>
#include <memory>
#include <nsclient/logger/log_message_factory.hpp>
#include <nsclient/logger/logger.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <onboarding/onboarding.hpp>
#include <str/utils.hpp>
#include <str/wstring.hpp>
#include <str/xtos.hpp>
#include <string>

#include "../libs/settings_manager/settings_manager_impl.h"
// The Windows ROOT store exporter the service uses to produce ${ca-path}; the
// installer needs its own copy of that bundle, see prepare_trust_store() below.
#include <shlobj.h>

#include <list>
#include <nscp/boot_layout.hpp>
#include <nscp/layout_migration.hpp>
#include <nscp/path_defaults.hpp>
#include <win/acl.hpp>

#include "../service/windows_ca_store.hpp"
#include "installer_helper.hpp"
#include "keys.hpp"

const UINT COST_SERVICE_INSTALL = 2000;

bool install(msi_helper &h, std::wstring exe, std::wstring service_short_name, std::wstring service_long_name, std::wstring service_description,
             std::wstring service_deps);
bool uninstall(msi_helper &h, std::wstring service_name);

void copy_file(msi_helper &h, std::wstring source, std::wstring target) {
  if (boost::filesystem::is_regular_file(utf8::cvt<std::string>(source))) {
    h.logMessage(L"Copying: " + source + L" to " + target);
    if (!CopyFile(source.c_str(), target.c_str(), FALSE)) {
      h.errorMessage(L"Failed to copy file: " + utf8::cvt<std::wstring>(error::lookup::last_error()));
    }
  } else {
    h.logMessage(L"Copying failed: " + source + L" to " + target + L" source was not found.");
  }
}

class msi_logger : public nsclient::logging::logger {
  std::wstring error_;
  std::list<std::wstring> log_;
  msi_helper *h;

 public:
  msi_logger(msi_helper *h) : h(h) {}

  bool should_trace() const { return false; }
  bool should_debug() const { return false; }
  bool should_info() const { return true; }
  bool should_warning() const { return true; }
  bool should_error() const { return true; }
  bool should_critical() const { return true; }

  virtual void set_log_level(std::string level) {
    // ignored
  }
  std::string get_log_level() const { return "info"; }

  void debug(const std::string &module, const char *file, const int line, const std::string &message) { do_log("debug: " + message); }
  void trace(const std::string &module, const char *file, const int line, const std::string &message) {}
  void info(const std::string &module, const char *file, const int line, const std::string &message) { do_log("info: " + message); }
  void warning(const std::string &module, const char *file, const int line, const std::string &message) { do_log("warning: " + message); }
  void error(const std::string &module, const char *file, const int line, const std::string &message) { do_log("error: " + message); }
  void critical(const std::string &module, const char *file, const int line, const std::string &message) { do_log("error: (critical) " + message); }
  void raw(const std::string &message) { do_log(message); }

  void do_log(const std::string data) {
    std::wstring str = utf8::cvt<std::wstring>(data);
    if (str.empty()) return;
    h->setLastLog(str);
    if (boost::algorithm::starts_with(str, L"error:")) {
      h->errorMessage(str);
      if (!error_.empty()) error_ += L"\n";
      error_ += str.substr(6);
    }
    log_.push_back(str);
  }
  void asynch_configure() {}
  void synch_configure() {}
  bool startup() { return true; }
  bool shutdown() { return true; }

  std::wstring get_error() { return error_; }
  bool has_errors() { return !error_.empty(); }
  std::list<std::wstring> get_errors() { return log_; }

  void nsclient::logging::logger::add_subscriber(nsclient::logging::logging_subscriber_instance) {}
  void nsclient::logging::logger::clear_subscribers(void) {}
  void nsclient::logging::logger::destroy(void) {}
  void nsclient::logging::logger::configure(void) {}
  void nsclient::logging::logger::set_backend(std::string) {}
};

void nsclient::logging::log_message_factory::log_fatal(const std::string &message) {}

std::string nsclient::logging::log_message_factory::create_critical(const std::string &module, const char *file, const int line, const std::string &message) {
  return "critical: " + message;
}
std::string nsclient::logging::log_message_factory::create_error(const std::string &module, const char *file, const int line, const std::string &message) {
  return "error: " + message;
}
std::string nsclient::logging::log_message_factory::create_warning(const std::string &module, const char *file, const int line, const std::string &message) {
  return "warning: " + message;
}
std::string nsclient::logging::log_message_factory::create_info(const std::string &module, const char *file, const int line, const std::string &message) {
  return "info: " + message;
}
std::string nsclient::logging::log_message_factory::create_debug(const std::string &module, const char *file, const int line, const std::string &message) {
  return "debug: " + message;
}
std::string nsclient::logging::log_message_factory::create_trace(const std::string &module, const char *file, const int line, const std::string &message) {
  return "trace: " + message;
}

// --- on-disk layout ---------------------------------------------------------
// The installer resolves ${shared-path} itself, because a custom action runs
// without the core's path resolver. The modern layout moves that one token to
// %ProgramData%\NSClient++, and everything writable is defined relative to it,
// so this is the only place the two layouts differ here.

// %ProgramData%, without a trailing separator, or empty if it cannot be
// determined.
std::string common_appdata_folder() {
  wchar_t buffer[MAX_PATH] = {0};
  if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buffer))) return std::string();
  std::string folder = utf8::cvt<std::string>(std::wstring(buffer));
  while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/')) folder.pop_back();
  return folder;
}

// Where ${shared-path} resolves to, and whether that answer can be used.
struct resolved_shared_folder {
  std::string folder;
  // False only when the layout is modern and %ProgramData% could not be
  // determined. There is no usable folder in that case - see
  // shared_folder_for_layout for why every substitute is wrong - so each
  // caller decides how to fail (or, on uninstall, how to shrug).
  bool resolved = true;
};

// Resolve the tokens a `[paths] shared-path` override can plausibly use,
// without the core's path resolver. ${shared-path} is deliberately absent: it
// is the token being defined.
std::string expand_shared_path_override(const std::string &value, const std::string &install_folder) {
  std::string result = value;
  str::utils::replace(result, "${exe-path}", install_folder);
  str::utils::replace(result, "${base-path}", install_folder);
  const std::string common_appdata = common_appdata_folder();
  if (!common_appdata.empty()) str::utils::replace(result, "${common-appdata}", common_appdata);
  return result;
}

// Which layout this install should use: what the host is on now, as recorded in
// its boot.ini, reconciled with the LAYOUT property. The rules live in
// nscp::paths::resolve_requested_layout so they can be tested.
nscp::paths::layout resolve_layout(const std::string &install_folder, const std::wstring &property_value) {
  const nscp::paths::layout current = nscp::paths::layout_from_boot_ini_file((boost::filesystem::path(install_folder) / "boot.ini").string());
  return nscp::paths::resolve_requested_layout(current, boost::algorithm::trim_copy(utf8::cvt<std::string>(property_value)));
}

// Where ${shared-path} points for `layout`. Legacy is the install folder;
// modern is %ProgramData%\NSClient++, and when %ProgramData% cannot be
// determined the answer is `resolved = false` rather than a substitute (the
// rule, and the reasoning, live in nscp::paths::shared_folder_for_layout so
// they can be tested).
//
// boot.ini's `[paths] shared-path` beats both, and beats the layout: the
// service applies it either way, so an installer that ignored it would migrate
// into %ProgramData% while the running agent looked in D:\nscp-state and found
// nothing.
resolved_shared_folder shared_folder_for(const nscp::paths::layout layout, const std::string &install_folder, msi_helper *h = nullptr) {
  const std::string configured = boost::algorithm::trim_copy(
      nscp::paths::path_override_from_boot_ini_file((boost::filesystem::path(install_folder) / "boot.ini").string(), "shared-path"));
  if (!configured.empty()) {
    const std::string expanded = expand_shared_path_override(configured, install_folder);
    if (expanded.find("${") == std::string::npos) {
      if (h != nullptr) h->logMessage("Using [paths] shared-path from boot.ini: " + expanded);
      return {expanded, true};
    }
    // A token only the core can resolve. Guessing would put the state
    // somewhere the service does not look, so say so and fall back.
    if (h != nullptr) {
      h->logMessage("WARNING: boot.ini has [paths] shared-path = " + configured +
                    ", which uses a token this installer cannot resolve; falling back to the layout default. The agent will look in the configured location, "
                    "so move the files there by hand or use an absolute path.");
    }
  }

  const std::string folder = nscp::paths::shared_folder_for_layout(layout, install_folder, common_appdata_folder());
  if (folder.empty()) {
    if (h != nullptr) h->logMessage("WARNING: could not determine %ProgramData% for the modern layout");
    return {install_folder, false};
  }
  return {folder, true};
}

struct installer_settings_provider : public settings_manager::provider_interface {
  msi_helper *h;
  std::string basepath;
  // Where ${shared-path} resolves to. The same as basepath on the legacy
  // layout; %ProgramData%\NSClient++ on the modern one.
  std::string shared_path;
  // The layout shared_path was resolved for, so default_for() can answer any
  // other layout-dependent token consistently with it.
  nscp::paths::layout layout_ = nscp::paths::layout::legacy;
  std::string old_settings_map;
  std::shared_ptr<msi_logger> logger;
  std::map<std::string, std::string> path_overrides_;
  // The CA bundle ${ca-path} resolves to for this custom action, and whether we
  // are the ones who put it there (and so have to clean it up). See
  // prepare_trust_store().
  std::string ca_bundle_;
  bool ca_bundle_is_ours_ = false;
  bool trust_store_ready_ = false;
  bool user_ca_ = false;

  installer_settings_provider(msi_helper *h, std::wstring basepath, std::wstring old_settings_map)
      : h(h),
        basepath(utf8::cvt<std::string>(basepath)),
        shared_path(utf8::cvt<std::string>(basepath)),
        old_settings_map(utf8::cvt<std::string>(old_settings_map)),
        logger(new msi_logger(h)) {}
  installer_settings_provider(msi_helper *h, std::wstring basepath)
      : h(h),
        basepath(utf8::cvt<std::string>(basepath)),
        shared_path(utf8::cvt<std::string>(basepath)),
        // No settings map in this overload. This must be an empty string, not
        // `old_settings_map(...)` - there is no parameter of that name here, so
        // that spelling initialises the member from its own uninitialised self.
        old_settings_map(),
        logger(new msi_logger(h)) {}

  // Point ${shared-path} somewhere other than the install folder (the modern
  // layout). Everything writable - the configuration, ${certificate-path},
  // ${fleet-folder} - is expressed relative to it and follows.
  void set_shared_path(const std::string &path) {
    if (!path.empty()) shared_path = path;
  }

  // The layout this provider resolves for, so the shared default table can
  // answer layout-dependent tokens the same way the service would. Callers set
  // it alongside set_shared_path from the same resolve_layout answer.
  void set_layout(const nscp::paths::layout layout) { layout_ = layout; }

  ~installer_settings_provider() {
    if (!ca_bundle_is_ours_ || ca_bundle_.empty()) return;
    boost::system::error_code ec;
    boost::filesystem::remove(ca_bundle_, ec);
  }

  // The operator brought their own trust anchor (the TLS_CA property). It is
  // handed to the settings transport verbatim, so there is nothing for us to
  // export - and overwriting ${ca-path} with a store dump would be actively
  // wrong for someone who deliberately pinned a single issuing CA.
  void use_user_ca() { user_ca_ = true; }

  // Materialise the trust anchor an https:// settings source is verified
  // against, before the settings store is opened (and thus before that source
  // is downloaded).
  //
  // The service exports the Windows ROOT store to ${ca-path}
  // (${certificate-path}/windows-ca.pem) at boot, but nothing has done so by
  // the time the installer runs: ImportConfig is sequenced before InstallFiles,
  // so the install folder - let alone its security/ subfolder - does not exist
  // yet, and on a fresh install the service has never started. So the installer
  // exports its own copy to a temp file and points ${ca-path} at that for the
  // duration of the custom action, then removes it again. Without this there is
  // no anchor on disk at all and the fetch that delivers the agent's entire
  // configuration would have to run unverified.
  //
  // A failure here is not fatal on its own: it leaves ${ca-path} unexpanded,
  // and the download that follows fails with its own error. Note the level -
  // these are warnings, never errors: ImportConfig treats any error-level
  // message from this provider as "the settings context is broken" and throws
  // the operator's CONFIGURATION_TYPE away.
  void prepare_trust_store() override {
    if (trust_store_ready_ || user_ca_) return;
    trust_store_ready_ = true;
    try {
      const boost::filesystem::path bundle = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-install-ca-%%%%%%%%.pem");
      std::list<std::string> ca_errors;
      const unsigned int count = nsclient::windows_ca::export_root_store(bundle.string(), ca_errors);
      for (const std::string &e : ca_errors) {
        logger->warning("settings", __FILE__, __LINE__, "windows-ca: " + e);
      }
      if (count == 0) {
        boost::system::error_code ec;
        boost::filesystem::remove(bundle, ec);
        logger->warning("settings", __FILE__, __LINE__,
                        "No certificates were exported from the Windows ROOT store; an https:// settings source cannot be verified. Pass TLS_CA=<file> to "
                        "use your own CA bundle, or TLS_VERIFY_MODE=none to fetch it unverified.");
        return;
      }
      ca_bundle_ = bundle.string();
      ca_bundle_is_ours_ = true;
      logger->debug("settings", __FILE__, __LINE__, "Exported " + str::xtos(count) + " Windows ROOT certificates to " + ca_bundle_);
    } catch (const std::exception &e) {
      logger->warning("settings", __FILE__, __LINE__, std::string("Failed to export the Windows ROOT store: ") + e.what());
    } catch (...) {
      logger->warning("settings", __FILE__, __LINE__, "Failed to export the Windows ROOT store: <unknown exception>");
    }
  }

  // One ${token}, resolved with the same precedence the service and the
  // standalone clients use: boot.ini [paths] overrides beat everything, then
  // the handful of values only this custom action knows, then the compiled
  // default table shared with the other resolvers (path_defaults.hpp). The
  // table is what makes ${fleet-folder} and friends resolve at all: they are
  // written in terms of ${shared-path}, and this provider used to leave them
  // unexpanded - so the first settings->save() after enrollment hit SaveFile
  // of the literal "${fleet-folder}/fleet.ini" and failed the whole install.
  std::string get_folder(const std::string &key) {
    const auto it = path_overrides_.find(key);
    if (it != path_overrides_.end()) return it->second;
    // ${ca-path} means the same thing here as it does to the service - "the
    // platform trust store as a PEM bundle" - but it resolves to the temp copy
    // prepare_trust_store() exported, since the service's own copy does not
    // exist during an install. Left alone if the export never ran or failed,
    // so the transport reports a missing CA rather than reading some other
    // file (expand_tokens treats a token that resolves to itself as opaque).
    if (key == "ca-path") return ca_bundle_.empty() ? "${ca-path}" : ca_bundle_;
    if (key == "base-path" || key == "exe-path") return basepath;
    // Not basepath: on the modern layout the writable state lives elsewhere,
    // and this is the token everything writable is defined against. The
    // program stays where it was installed, so exe-path and base-path do not
    // move. Resolved for *this* run rather than through the default table,
    // because on the first LAYOUT=modern install the property, not boot.ini,
    // is what knows the layout until ExecPrepareLayout stamps it.
    if (key == "shared-path") return shared_path;
    if (key == "common-appdata") {
      const std::string folder = common_appdata_folder();
      return folder.empty() ? "${common-appdata}" : folder;
    }
    const std::string def = nscp::paths::default_for(key, layout_);
    if (!def.empty()) return def;
    // Last resort, matching the service and the clients: the install folder,
    // never a literal ${...} that fails whatever tries to open the path.
    logger->warning("settings", __FILE__, __LINE__, "Unknown path token ${" + key + "}; resolving to the install folder " + basepath);
    return basepath;
  }

  virtual std::string expand_path(std::string file) {
    return nscp::paths::expand_tokens(file, [this](const std::string &key) { return get_folder(key); });
  }
  std::string get_data(std::string key) {
    if (!old_settings_map.empty() && key == "old_settings_map_data") {
      return old_settings_map;
    }
    return "";
  }

  std::wstring get_error() { return logger->get_error(); }
  bool has_errors() { return logger->has_errors(); }
  std::list<std::wstring> get_errors() { return logger->get_errors(); }

  nsclient::logging::logger_instance get_logger() const { return logger; }

  void apply_path_overrides(std::map<std::string, std::string> overrides) override { path_overrides_ = std::move(overrides); }
};

static const wchar_t alphanum[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
// Generate a cryptographically random password of `len` alphanumeric chars.
//
// Was: srand(time(NULL)) + rand(). The seed was the install second (~30 bits
// of entropy) and rand()%62 is biased - both made the default admin password
// brute-forceable for any attacker who knew the install window.
//
// Now: OpenSSL RAND_bytes (already linked) plus rejection sampling so each
// character is drawn uniformly from the 62-char alphabet. Any RNG failure is
// fatal - the installer must never silently fall back to a guessable
// password, so we throw and let the surrounding installer_exception catch
// fail the custom action.
std::wstring genpwd(const int len) {
  constexpr unsigned int alpha_size = (sizeof(alphanum) / sizeof(wchar_t)) - 1;
  static_assert(alpha_size == 62, "alphanum size changed - revisit rejection bound");
  // Largest multiple of alpha_size that fits in a byte (256 - 256 % 62 = 248).
  // Bytes >= this would bias `b % alpha_size` toward the first 8 chars.
  constexpr unsigned char accept_bound = static_cast<unsigned char>(256u - (256u % alpha_size));

  std::wstring ret;
  ret.reserve(len);
  unsigned char buf[64];
  std::size_t pos = sizeof(buf);
  for (int i = 0; i < len;) {
    if (pos >= sizeof(buf)) {
      if (RAND_bytes(buf, sizeof(buf)) != 1) {
        throw installer_exception(L"RAND_bytes failed; cannot generate a secure password");
      }
      pos = 0;
    }
    const unsigned char b = buf[pos++];
    if (b >= accept_bound) continue;  // reject biased values, redraw
    ret += alphanum[b % alpha_size];
    ++i;
  }
  return ret;
}

bool mod_enabled(std::string key) {
  std::string val = settings_manager::get_settings()->get_string(MAIN_MODULES_SECTION, key, "0");
  return val == "enabled" || val == "1";
}
bool has_module(std::string key) { return settings_manager::get_settings()->has_key(MAIN_MODULES_SECTION, key); }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Many options:
// - configuration not allowed		=> CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=0
// - target not found				=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config read		=> CONF_CAN_CHANGE=1, CONF_HAS_ERRORS=0
// - target found + config NOT read => CONF_CAN_CHANGE=0, CONF_HAS_ERRORS=1
//
// Interpretation:
// CONF_HAS_ERRORS=1	=> Dont allow anything (inform of issue)
// CONF_CAN_CHANGE=1	=> Allow change

std::wstring read_map_data(msi_helper &h) {
  std::wstring ret;
  PMSIHANDLE hView = h.open_execute_view(L"SELECT Data FROM Binary WHERE Name='OldSettingsMap'");
  if (h.isNull(hView)) {
    h.logMessage(L"Failed to query service view!");
    return L" ";
  }

  PMSIHANDLE hRec = h.fetch_record(hView);
  if (hRec != NULL) {
    ret = h.get_record_blob(hRec, 1);
    ::MsiCloseHandle(hRec);
  }
  ::MsiCloseHandle(hView);
  return ret;
}

void dump_config(msi_helper &h, std::wstring title) {
  h.dumpReason(title);
  for (const auto key : {ALLOWED_HOSTS, NSCLIENT_PWD, CONF_SCHEDULER, CONF_CHECKS, CONF_NRPE, CONF_NSCA, CONF_WEB, CONF_NSCLIENT, NRPEMODE, CONFIGURATION_TYPE,
                         CONF_INCLUDES, IMPORT_CONFIG}) {
    h.dumpProperties(key);
  }
  h.dumpProperty(BACKUP_FILE);
  h.dumpProperty(INT_CONF_CAN_CHANGE);
  h.dumpProperty(INT_CONF_CAN_CHANGE_REASON);
  h.dumpProperty(INT_NSCP_ERROR);
  h.dumpProperty(INT_NSCP_ERROR_CONTEXT);
}

extern "C" UINT __stdcall DetectTool(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"DetectTool");

  try {
    h.logMessage("Detecting monitoring tool config");
    if (!boost::algorithm::trim_copy(h.getMsiPropery(OP5_SERVER)).empty()) {
      h.setPropertyValue(MONITORING_TOOL, MONITORING_TOOL_OP5);
    }
    std::wstring tool = h.getMsiPropery(MONITORING_TOOL);
    h.logMessage(L"Detected monitoring tool is: " + tool);
    dump_config(h, L"After DetectTool");
  } catch (installer_exception &e) {
    h.logMessage(L"Failed to detect monitoring tool: " + e.what());
    return ERROR_SUCCESS;
  } catch (nsclient::nsclient_exception &e) {
    h.logMessage(L"Failed to detect monitoring tool: " + utf8::cvt<std::wstring>(e.reason()));
    return ERROR_SUCCESS;
  } catch (...) {
    h.logMessage(L"Failed to detect monitoring tool: Unknown exception");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall ApplyTool(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ApplyTool");
  try {
    dump_config(h, L"Before ApplyTool");

    h.logMessage("Applying monitoring tool config");
    std::wstring tool = h.getMsiPropery(MONITORING_TOOL);

    if (tool == MONITORING_TOOL_OP5) {
      h.logMessage(L"Setting base config as Op5");
      h.setPropertyKeyAndDefault(NSCLIENT_PWD, L"", L"");
      h.setPropertyKeyAndDefault(CONF_CHECKS, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NRPE, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCA, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_WEB, L"", L"");
      h.setPropertyKeyAndDefault(CONF_NSCLIENT, L"1", L"");
      h.setPropertyKeyAndDefault(NRPEMODE, L"LEGACY", L"");

      h.setPropertyKeyAndDefault(CONF_INCLUDES, L"op5;op5.ini", L"");
      h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, L"registry://HKEY_LOCAL_MACHINE/software/NSClient++", L"");
      h.setFeatureLocal(L"OP5Monitoring");
      h.setConfCanChange(true, L"Op5 applied");
    } else if (tool == L"GENERIC") {
      h.logMessage(L"Setting base config as Generic");
      h.setPropertyKeyAndDefault(ALLOWED_HOSTS, L"127.0.0.1", L"");

      h.setPropertyKeyAndDefault(NSCLIENT_PWD, genpwd(16), L"");
      h.setPropertyKeyAndDefault(CONF_CHECKS, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NRPE, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCA, L"", L"");
      h.setPropertyKeyAndDefault(CONF_WEB, L"1", L"");
      h.setPropertyKeyAndDefault(CONF_NSCLIENT, L"", L"");
      h.setPropertyKeyAndDefault(NRPEMODE, L"SECURE", L"");

      h.setPropertyKeyAndDefault(CONF_INCLUDES, L"", L"");
      h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini", L"");
      h.setFeatureAbsent(L"OP5Monitoring");
      h.setConfCanChange(true, L"Generic applied");
    }

    h.setConfCanChange(true, L"Default config set from profile");
    h.setPropertyIfEmpty(CONFIGURATION_TYPE, L"ini://${shared-path}/nsclient.ini");

    dump_config(h, L"After ApplyTool");

  } catch (installer_exception &e) {
    h.logMessage(L"Failed to apply monitoring tool: " + e.what());
    return ERROR_SUCCESS;
  } catch (...) {
    h.logMessage(L"Failed to apply monitoring tool: Unknown exception");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall ImportConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ImportConfig");
  try {
    dump_config(h, L"Before ImportConfig");

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");

    std::string tls_version = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_VERSION"));
    std::string tls_verify_mode = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_VERIFY_MODE"));
    std::string tls_ca = utf8::cvt<std::string>(h.getMsiPropery(L"TLS_CA"));
    // The unset defaults are the service's, not laxer ones: this transport
    // fetches the file that becomes the agent's whole configuration, and the
    // installer is the very first time it runs. TLS_CA, when given, is used
    // verbatim (bring your own); otherwise ${ca-path} resolves to the ROOT
    // store copy the provider exports below.
    const bool user_ca = !tls_ca.empty();
    if (tls_version.empty()) {
      tls_version = "1.3";
    }
    if (tls_verify_mode.empty()) {
      tls_verify_mode = settings_manager::NSCSettingsImpl::kDefaultTlsVerifyMode;
    }
    if (!user_ca) {
      tls_ca = settings_manager::NSCSettingsImpl::kDefaultTlsCa;
    }

    std::wstring map_data = read_map_data(h);
    if (h.getMsiPropery(ALLOW_CONFIGURATION) == L"0") {
      h.setError(L"ImportConfig::1", L"Configuration is not allowed to change");
      h.setConfCanChange(false, L"Changes are not allowed");
      dump_config(h, L"After ImportConfig");
      return ERROR_SUCCESS;
    }

    auto wanted_source_context = h.getProperyValue(IMPORT_CONFIG);
    auto wanted_target_context = h.getProperyValue(CONFIGURATION_TYPE);
    std::wstring wanted_context;
    bool should_import = false;
    if (wanted_source_context.empty()) {
      wanted_context = wanted_target_context;
    } else {
      wanted_context = wanted_source_context;
      should_import = true;
    }
    auto default_context = h.getProperyKey(CONFIGURATION_TYPE);
    auto context = wanted_context.empty() ? default_context : wanted_context;
    h.logMessage(L"Reading existing config using: " + context + L" tls version=" + utf8::cvt<std::wstring>(tls_version) + L", tls_verify=" +
                 utf8::cvt<std::wstring>(tls_verify_mode) + L", tls_ca=" + utf8::cvt<std::wstring>(tls_ca));

    installer_settings_provider provider(&h, target, map_data);
    // The layout this host already uses: we are reading its current
    // configuration, which has not moved yet even if LAYOUT asks for it to.
    const nscp::paths::layout current_layout = resolve_layout(utf8::cvt<std::string>(target), L"");
    provider.set_layout(current_layout);
    const resolved_shared_folder import_shared = shared_folder_for(current_layout, utf8::cvt<std::string>(target), &h);
    if (!import_shared.resolved) {
      h.setError(L"ImportConfig",
                 L"This host is on the modern layout but %ProgramData% could not be determined, so its configuration cannot be located. This machine needs "
                 L"CSIDL_COMMON_APPDATA repaired before NSClient++ can be upgraded.");
      return ERROR_INSTALL_FAILURE;
    }
    provider.set_shared_path(import_shared.folder);
    if (user_ca) provider.use_user_ca();
    if (!settings_manager::init_installer_settings(&provider, utf8::cvt<std::string>(context), tls_version, tls_verify_mode, tls_ca)) {
      h.setError(L"ImportConfig::init_installer_settings", L"Settings context had fatal errors");
      h.setConfHasErrors(L"Failed to load existing configuration");
      dump_config(h, L"After ImportConfig");
      return ERROR_SUCCESS;
    }
    if (provider.has_errors()) {
      h.logMessage(L"Settings context reported errors (debug log end)");
      for (std::wstring l : provider.get_errors()) {
        h.logMessage(l);
      }
      h.logMessage(L"Settings context reported errors (debug log end)");
      if (!settings_manager::has_boot_conf()) {
        h.logMessage(L"boot.conf was NOT found (so no new configuration)");
        if (settings_manager::context_exists(DEFAULT_CONF_OLD_LOCATION)) {
          h.setError(L"ImportConfig::has_boot_conf", std::wstring(L"Old configuration (") + utf8::cvt<std::wstring>(DEFAULT_CONF_OLD_LOCATION) +
                                                         L") was found but we got errors accessing it: " + provider.get_error());
          h.setConfHasErrors(L"Errors reading old configuration");
          dump_config(h, L"After ImportConfig");
          return ERROR_SUCCESS;
        } else {
          h.logMessage(L"Failed to read configuration but no configuration was found (so we are assuming there is no configuration).");
          h.setConfCanChange(true, L"Why do we ignore errors here?");
          dump_config(h, L"After ImportConfig");
          return ERROR_SUCCESS;
        }
      } else {
        h.setError(L"ImportConfig::has_errors", L"boot.conf was found but we got errors booting it: " + provider.get_error());
        h.setConfHasErrors(L"Errors during read config");
        dump_config(h, L"After ImportConfig");
        return ERROR_SUCCESS;
      }
    }

    h.logMessage(L"Previous configuration loaded correctly...");

    if (should_import) {
      h.logMessage(L"Importing configuration from: " + utf8::cvt<std::wstring>(wanted_source_context) + L" to " +
                   utf8::cvt<std::wstring>(wanted_target_context));
      auto can_edit = settings_manager::get_core()->supports_edit(utf8::cvt<std::string>(wanted_target_context));
      if (!can_edit) {
        h.errorMessage(L"Cannot edit target context: " + utf8::cvt<std::wstring>(wanted_target_context));
        h.setConfHasErrors(L"Target configuration cannot be edited by installer");
        dump_config(h, L"After ImportConfig");
        return ERROR_INSTALL_FAILURE;
      }
      h.setConfCanChange(true, L"Target store is technically updatable");
      h.setPropertyKeyAndDefault(IMPORT_CONFIG, wanted_source_context, L"");
    } else {
      h.logMessage(L"No import requested, just reading existing configuration: " + utf8::cvt<std::wstring>(wanted_target_context));
      if (!settings_manager::get_settings()->supports_updates()) {
        h.applyPropertyValue(CONFIGURATION_TYPE);
        h.logMessage(L"Settings does not support updates");
        h.setConfCanChange(false, L"Using a settings system which do no support updates by installer");
        return ERROR_SUCCESS;
      }

      auto actual_context = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_context());
      h.setPropertyKeyAndDefault(CONFIGURATION_TYPE, actual_context, actual_context);
      h.logMessage(L"Using configuration context: " + actual_context + L", " + utf8::cvt<std::wstring>(settings_manager::get_settings()->get_info()));
      if (!settings_manager::get_settings()->supports_updates()) {
        h.errorMessage(L"Updates not supported");
        h.setConfCanChange(false, L"Using a settings system which do no support updates by installer");
        return ERROR_SUCCESS;
      }
      if (!settings_manager::get_core()->supports_updates()) {
        h.errorMessage(L"Using a settings store which cannot be updated by installer");
        h.setConfCanChange(false, L"Settings store does not support updates by installer");
        return ERROR_SUCCESS;
      }
    }

    if (settings_manager::get_core()->use_sensitive_keys()) {
      h.errorMessage(L"Using sensitive keys");
      h.setConfCanChange(false, L"Senstive keys cannot be updated by installer");
      return ERROR_SUCCESS;
    }

    h.logMessage(L"Configuration seems updatable...");

    h.setConfCanChange(true, L"Configuration seems good");

    h.logMessage(L"Applying old keys (as default values)...");

    if (settings_manager::get_settings()->has_key("/settings/default", "allowed hosts")) {
      auto old_allowed_hosts = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "allowed hosts", ""));
      h.setPropertyKeyAndDefault(ALLOWED_HOSTS, old_allowed_hosts, old_allowed_hosts);
    }
    if (settings_manager::get_settings()->has_key("/settings/default", "password")) {
      auto old_password = utf8::cvt<std::wstring>(settings_manager::get_settings()->get_string("/settings/default", "password", ""));
      h.setPropertyKeyAndDefault(NSCLIENT_PWD, old_password, old_password);
    }

    if (has_module("NRPEServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_NRPE, mod_enabled("NRPEServer"));
    }
    if (has_module("Scheduler")) {
      h.setPropertyKeyAndDefaultBool(CONF_SCHEDULER, mod_enabled("Scheduler"));
    }
    if (has_module("NSCAClient")) {
      h.setPropertyKeyAndDefaultBool(CONF_NSCA, mod_enabled("NSCAClient"));
    }
    if (has_module("NSClientServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_NSCLIENT, mod_enabled("NSClientServer"));
    }
    if (has_module("WEBServer")) {
      h.setPropertyKeyAndDefaultBool(CONF_WEB, mod_enabled("WEBServer"));
    }

    if (settings_manager::get_settings()->has_key("/settings/NRPE/server", "insecure") ||
        settings_manager::get_settings()->has_key("/settings/NRPE/server", "verify mode")) {
      std::string insecure = settings_manager::get_settings()->get_string("/settings/NRPE/server", "insecure", "");
      std::string verify = settings_manager::get_settings()->get_string("/settings/NRPE/server", "verify mode", "");
      h.logMessage(L"Old NRPE insecure: " + utf8::cvt<std::wstring>(insecure));
      h.logMessage(L"Old NRPE verify: " + utf8::cvt<std::wstring>(verify));
      if (insecure == "true" || insecure == "1") {
        h.logMessage("Setting old NRPE mode legacy");
        h.setPropertyKeyAndDefault(NRPEMODE, L"LEGACY", L"");
      } else if (verify == "peer-cert") {
        h.logMessage("Setting old NRPE mode secure");
        h.setPropertyKeyAndDefault(NRPEMODE, L"SECURE", L"");
      } else {
        h.logMessage(L"Unknown old NRPE mode: " + h.getProperyKey(NRPEMODE));
      }
    }

    if (has_module("CheckSystem") || has_module("CheckDisk") || has_module("CheckEventLog") || has_module("CheckHelpers") ||
        has_module("CheckExternalScripts") || has_module("CheckNSCP")) {
      h.setPropertyKeyAndDefaultBool(CONF_CHECKS, mod_enabled("CheckSystem") && mod_enabled("CheckDisk") && mod_enabled("CheckEventLog") &&
                                                      mod_enabled("CheckHelpers") && mod_enabled("CheckExternalScripts") && mod_enabled("CheckNSCP"));
    }

    h.logMessage(L"Old keys applied");
    settings_manager::destroy_settings();

    h.logMessage(L"Determaining which keys have changed");
    h.applyPropertyValue(ALLOWED_HOSTS);
    h.applyPropertyValue(NSCLIENT_PWD);
    h.applyPropertyValue(CONF_SCHEDULER);
    h.applyPropertyValue(CONF_CHECKS);
    h.applyPropertyValue(CONF_NRPE);
    h.applyPropertyValue(CONF_NSCA);
    h.applyPropertyValue(CONF_WEB);
    h.applyPropertyValue(CONF_NSCLIENT);
    h.applyPropertyValue(NRPEMODE);

    h.applyPropertyValue(CONFIGURATION_TYPE);
    h.applyPropertyValue(CONF_INCLUDES);

    dump_config(h, L"After ImportConfig");

  } catch (installer_exception &e) {
    h.setError(L"ImportConfig::e1", L"Failed to read old configuration file: " + e.what());
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (nsclient::nsclient_exception &e) {
    h.setError(L"ImportConfig::e2", L"Failed to read old configuration file: " + utf8::cvt<std::wstring>(e.what()));
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (std::exception &e) {
    h.setError(L"ImportConfig::e3", L"Failed to read old configuration file: " + utf8::cvt<std::wstring>(e.what()));
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  } catch (...) {
    h.setError(L"ImportConfig::e4", L"Failed to read old configuration file: <Unknown exception>");
    h.setConfHasErrors(L"Failed to read old configuration file");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool write_config(msi_helper &h, std::wstring path, std::wstring file);

void write_key(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring path, std::wstring key, std::wstring val) {
  data.write_int(mode);
  data.write_string(path);
  data.write_string(key);
  data.write_string(val);
  h.logMessage(L"write_key: " + path + L"." + key + L"=" + val);
}

void write_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, int mode, std::wstring key, std::wstring val) {
  std::wstring path = utf8::cvt<std::wstring>(MAIN_MODULES_SECTION);
  if (val == L"1" || val == L"enabled") {
    write_key(h, data, mode, path, key, L"enabled");
  } else {
    write_key(h, data, mode, path, key, L"disabled");
  }
}

void write_changed_key(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring path, std::wstring key) {
  std::wstring val = h.getProperyKey(prop);
  if (!h.propertyNotDefault(prop)) {
    h.logMessage(L"IGNORING property not changed: " + prop + L"; " + path + L"." + key + L"=" + val);
    return;
  }
  h.logMessage(L"write_changed_key: " + prop + L"; " + path + L"." + key + L"=" + val);
  write_key(h, data, 1, path, key, val);
}

void write_changed_key_mod(msi_helper &h, msi_helper::custom_action_data_w &data, std::wstring prop, std::wstring key) {
  std::wstring val = h.getProperyKey(prop);
  if (!h.propertyNotDefault(prop)) {
    h.logMessage(L"write_changed_key_mod: IGNORING property not changed: " + prop + L"; <modules>." + key + L"=" + val);
    return;
  }
  h.logMessage(L"write_changed_key_mod: " + prop + L"; <modules>." + key + L"=" + val);
  write_key_mod(h, data, 1, key, val);
}

bool write_property_if_set(msi_helper &h, msi_helper::custom_action_data_w &data, const std::wstring prop, std::wstring path, std::wstring key) {
  std::wstring val = boost::algorithm::trim_copy(h.getProperyKey(prop));
  if (!val.empty()) {
    h.logMessage(L"write_property_if_set: " + prop + L"; <modules>." + key + L"=" + val);
    write_key(h, data, 1, path, key, val);
    return true;
  } else {
    h.logMessage(L"IGNORING property not set: " + prop + L"; " + path + L"." + key + L"=" + val);
  }
  return false;
}

extern "C" UINT __stdcall BackupConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"BackupConfig");
  try {
    dump_config(h, L"Before BackupConfig");

    if (h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      h.logMessage(L"Configuration changes not allowed: set CONF_CAN_CHANGE=1");
      return ERROR_SUCCESS;
    }

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");
    boost::filesystem::path backup = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::path target_path = target;
    boost::filesystem::path config_file = target_path / L"nsclient.ini";
    if (boost::filesystem::exists(config_file)) {
      h.logMessage(L"Config file found: " + config_file.wstring());
      h.logMessage(L"Backup file: " + backup.wstring());
      copy_file(h, config_file.wstring(), backup.wstring());
      h.setPropertyValue(BACKUP_FILE, backup.wstring());
    }

  } catch (installer_exception &e) {
    h.errorMessage(L"Failed to install service: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to install service: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ScheduleWriteConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ScheduleWriteConfig");
  try {
    dump_config(h, L"Before ScheduleWriteConfig");

    std::wstring target = h.getTargetPath(L"INSTALLLOCATION");
    boost::filesystem::path backup = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::path target_path = target;
    boost::filesystem::path config_file = target_path / L"nsclient.ini";

    msi_helper::custom_action_data_w data;
    data.write_string(h.getTargetPath(L"INSTALLLOCATION"));
    data.write_string(h.getProperyKey(CONFIGURATION_TYPE));
    data.write_string(h.getMsiPropery(L"RESTORE_FILE"));
    data.write_string(h.getMsiPropery(BACKUP_FILE));
    data.write_string(h.getProperyKey(IMPORT_CONFIG));

    data.write_string(h.getMsiPropery(L"TLS_VERSION"));
    data.write_string(h.getMsiPropery(L"TLS_VERIFY_MODE"));
    data.write_string(h.getMsiPropery(L"TLS_CA"));

    if (h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      h.logMessage(L"Configuration changes not allowed (only updating boot.ini): set CONF_CAN_CHANGE=1");
      data.write_int(0);

      if (data.has_data()) {
        h.logMessage(L"Scheduling (ExecWriteConfig): " + data.to_string());
        HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
        if (FAILED(hr)) {
          h.errorMessage(L"failed to schedule config update");
          return hr;
        }
      }

      return ERROR_SUCCESS;
    } else {
      h.logMessage(L"Configuration changes allowed (updating boot.ini and nsclient.ini)");
      data.write_int(1);
    }

    std::wstring confInclude = h.getProperyKey(CONF_INCLUDES);
    h.logMessage(L"Adding include: " + confInclude);
    if (!confInclude.empty()) {
      std::vector<std::wstring> lst;
      boost::split(lst, confInclude, boost::is_any_of(L";"));
      for (int i = 0; i + 1 < lst.size(); i += 2) {
        h.logMessage(L" + : " + lst[i] + L"=" + lst[i + 1]);
        write_key(h, data, 1, L"/includes", lst[i], lst[i + 1]);
      }
    }

    write_changed_key_mod(h, data, CONF_NRPE, L"NRPEServer");
    write_changed_key_mod(h, data, CONF_SCHEDULER, L"Scheduler");
    write_changed_key_mod(h, data, CONF_NSCA, L"NSCAClient");
    write_changed_key_mod(h, data, CONF_NSCLIENT, L"NSClientServer");
    write_changed_key_mod(h, data, CONF_WMI, L"CheckWMI");
    write_changed_key_mod(h, data, CONF_WEB, L"WEBServer");

    if (h.propertyNotDefault(CONF_CHECKS)) {
      std::wstring modval = h.getProperyKey(CONF_CHECKS);
      if (modval == L"1") {
        modval = L"enabled";
      } else {
        modval = L"disabled";
      }
      write_key_mod(h, data, 1, L"CheckSystem", modval);
      write_key_mod(h, data, 1, L"CheckDisk", modval);
      write_key_mod(h, data, 1, L"CheckEventLog", modval);
      write_key_mod(h, data, 1, L"CheckHelpers", modval);
      write_key_mod(h, data, 1, L"CheckExternalScripts", modval);
      write_key_mod(h, data, 1, L"CheckNSCP", modval);
    }
    if (h.getProperyKey(CONF_NRPE) == L"1") {
      if (h.propertyNotDefault(NRPEMODE)) {
        std::wstring mode = h.getProperyKey(NRPEMODE);
        write_key(h, data, 1, L"/settings/NRPE/server", L"ssl options", L"");
        write_key(h, data, 1, L"/settings/NRPE/server", L"tls version", L"tlsv1.2+");
        if (mode == L"LEGACY") {
          write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"true");
          write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"none");
        } else {
          write_key(h, data, 1, L"/settings/NRPE/server", L"insecure", L"false");
          write_key(h, data, 1, L"/settings/NRPE/server", L"verify mode", L"peer-cert");
        }
      }
    }

    std::wstring defpath = L"/settings/default";
    write_changed_key(h, data, ALLOWED_HOSTS, defpath, L"allowed hosts");
    write_changed_key(h, data, NSCLIENT_PWD, defpath, L"password");

    // Operator-supplied TLS material: ExecInstallCerts puts the files at the
    // default names under ${certificate-path}, so certificate.pem and ca.pem
    // are picked up with no configuration at all - but a *separate* private
    // key is only read where a `certificate key` setting points at it (the
    // modules default to reading the key from the certificate file). Wire it
    // up for the two servers the installer manages TLS for; other servers are
    // configured by hand, as documented. Written unexpanded, like the fleet
    // include, so the configuration stays relocatable.
    if (!boost::algorithm::trim_copy(h.getMsiPropery(CERTIFICATE_KEY)).empty()) {
      const std::wstring key_file = L"${certificate-path}/certificate_key.pem";
      write_key(h, data, 1, L"/settings/NRPE/server", L"certificate key", key_file);
      write_key(h, data, 1, L"/settings/WEB/server", L"certificate key", key_file);
    }

    std::wstring confSet = h.getMsiPropery(L"CONF_SET");
    h.logMessage(L"Adding conf: " + confSet);
    if (!confSet.empty()) {
      std::vector<std::wstring> lst;
      boost::split(lst, confSet, boost::is_any_of(L";"));
      for (int i = 0; i + 2 < lst.size(); i += 3) {
        h.logMessage(L" + : " + lst[i] + L" " + lst[i + 1] + L"=" + lst[i + 2]);
        write_key(h, data, 1, lst[i], lst[i + 1], lst[i + 2]);
      }
    }

    // Fleet onboarding: the core starts the sync worker whenever the
    // enrollment manifest ExecEnrollFleet wrote exists (no module to enable),
    // so all the configuration needs is an include of the fleet-managed file.
    // Written unexpanded so the configuration stays relocatable, and only when
    // enrollment was actually requested.
    // ${fleet-folder} rather than the folder it happens to expand to: the same
    // include `nscp enroll` writes on either platform, and the service resolves
    // the token (see FLEET_FOLDER_KEY in path_manager).
    if (!boost::algorithm::trim_copy(h.getMsiPropery(FLEET_SERVER)).empty()) {
      write_key(h, data, 1, L"/includes", L"fleet", utf8::cvt<std::wstring>(std::string("${" FLEET_FOLDER_KEY "}/fleet.ini")));
    }

    if (write_property_if_set(h, data, OP5_SERVER, L"/settings/op5", L"server")) {
      write_key(h, data, 1, L"/modules", L"OP5Client", L"enabled");
    }
    write_property_if_set(h, data, OP5_USER, L"/settings/op5", L"user");
    write_property_if_set(h, data, OP5_PASSWORD, L"/settings/op5", L"password");
    write_property_if_set(h, data, OP5_HOSTGROUPS, L"/settings/op5", L"hostgroups");
    write_property_if_set(h, data, OP5_CONTACTGROUP, L"/settings/op5", L"contactgroups");

    if (data.has_data()) {
      h.logMessage(L"Scheduling (ExecWriteConfig): " + data.to_string());
      HRESULT hr = h.do_deferred_action(L"ExecWriteConfig", data, 1000);
      if (FAILED(hr)) {
        h.errorMessage(L"failed to schedule config update");
        return hr;
      }
    }
  } catch (installer_exception &e) {
    h.errorMessage(L"Failed to install service: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to install service: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}
extern "C" UINT __stdcall ExecWriteConfig(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecWriteConfig");
  try {
    h.logMessage(L"RAW: " + h.getMsiPropery(L"CustomActionData"));
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    h.logMessage(L"Got CA data: " + data.to_string());
    std::wstring target = data.get_next_string();
    std::string target_context = utf8::cvt<std::string>(data.get_next_string());
    std::wstring restore = data.get_next_string();
    std::wstring backup = data.get_next_string();
    std::wstring import_context = data.get_next_string();

    std::wstring tls_version = data.get_next_string();
    std::wstring tls_verify_mode = data.get_next_string();
    std::wstring tls_ca = data.get_next_string();
    int update_nsclient_ini = data.get_next_int();

    h.logMessage(L"Target: " + target);
    h.logMessage("Context: " + target_context);
    h.logMessage(L"Restore: " + restore);
    h.logMessage(L"Backup: " + backup);
    h.logMessage(L"Import config: " + import_context);
    h.logMessage(L"Update ns-client.ini: " + update_nsclient_ini ? L"Yes" : L"No");

    std::string source_context;
    bool do_import = false;
    if (!import_context.empty()) {
      source_context = utf8::cvt<std::string>(import_context);
      do_import = true;
    } else {
      source_context = target_context;
    }
    h.logMessage(L"Source config: " + utf8::cvt<std::wstring>(source_context));

    boost::filesystem::path target_path = target;
    boost::filesystem::path old_path = target_path / "nsc.ini.old";
    boost::filesystem::path legacy_config_path = target_path / "nsc.ini";

    // ExecPrepareLayout ran before this and recorded the layout in boot.ini, so
    // reading it back tells us where the configuration lives now: on the modern
    // layout it has just been moved to the shared folder, and restoring the
    // backup into the install folder would leave a stale copy in the very place
    // the migration exists to empty.
    const nscp::paths::layout recorded_layout = resolve_layout(utf8::cvt<std::string>(target), L"");
    const resolved_shared_folder write_shared = shared_folder_for(recorded_layout, utf8::cvt<std::string>(target), &h);
    if (!write_shared.resolved) {
      h.setError(L"ExecWriteConfig",
                 L"This host is on the modern layout but %ProgramData% could not be determined, so the configuration cannot be written where the service "
                 L"will read it.");
      return ERROR_INSTALL_FAILURE;
    }
    const std::string shared_folder = write_shared.folder;

    boost::filesystem::path restore_path = restore;

    boost::filesystem::path backup_path = backup;

    if (boost::filesystem::exists(old_path)) h.logMessage(L"Found old (.old) file: " + strEx::xtos(boost::filesystem::file_size(old_path)));
    if (boost::filesystem::exists(legacy_config_path)) h.logMessage(L"Found legacy file: " + strEx::xtos(boost::filesystem::file_size(legacy_config_path)));

    if (boost::filesystem::exists(backup_path)) {
      h.logMessage(L"Restoring from backup: " + backup_path.wstring());
      h.logMessage(L"Found Backup file with size: " + strEx::xtos(boost::filesystem::file_size(backup_path)));
      boost::filesystem::path config_path = boost::filesystem::path(shared_folder) / "nsclient.ini";
      copy_file(h, backup_path.wstring(), config_path.wstring());
      if (!boost::filesystem::remove(backup_path)) {
        h.errorMessage(L"Failed to remove backup file: " + backup_path.wstring());
      }
    }

    if (boost::filesystem::exists(restore_path)) {
      h.logMessage(L"Found restore file: " + strEx::xtos(boost::filesystem::file_size(restore_path)));
      h.logMessage(L"Restore path exists: " + restore);
      if (!boost::filesystem::exists(legacy_config_path)) {
        h.logMessage(L"Restoring nsc.ini configuration file");
        copy_file(h, restore_path.wstring(), legacy_config_path.wstring());
      }
      if (!boost::filesystem::exists(old_path)) {
        h.logMessage(L"Creating backup nsc.ini.old configuration file");
        copy_file(h, restore_path.wstring(), old_path.wstring());
      }
    }

    installer_settings_provider provider(&h, target);
    provider.set_layout(recorded_layout);
    provider.set_shared_path(shared_folder);
    if (!tls_ca.empty()) provider.use_user_ca();

    // Same defaults as ImportConfig - see the note there. Only the properties
    // the operator actually set are written to boot.ini further down, so an
    // unset TLS_VERIFY_MODE/TLS_CA leaves the [tls] section alone and the
    // service applies its own (identical) defaults at boot.
    auto use_tls_version = tls_version.empty() ? std::string("1.3") : utf8::cvt<std::string>(tls_version);
    auto use_tls_verify_mode =
        tls_verify_mode.empty() ? std::string(settings_manager::NSCSettingsImpl::kDefaultTlsVerifyMode) : utf8::cvt<std::string>(tls_verify_mode);
    auto use_tls_ca = tls_ca.empty() ? std::string(settings_manager::NSCSettingsImpl::kDefaultTlsCa) : utf8::cvt<std::string>(tls_ca);

    h.logMessage(L"Writing existing config TLS options: tls version=" + utf8::cvt<std::wstring>(use_tls_version) + L", tls_verify=" +
                 utf8::cvt<std::wstring>(use_tls_verify_mode) + L", tls_ca=" + utf8::cvt<std::wstring>(use_tls_ca));
    h.logMessage(L"Loading config: " + utf8::cvt<std::wstring>(source_context));
    if (!settings_manager::init_installer_settings(&provider, source_context, use_tls_version, use_tls_verify_mode, use_tls_ca)) {
      h.errorMessage(L"Failed to boot settings when writing: " + provider.get_error());
      return ERROR_SUCCESS;
    }

    if (!tls_version.empty()) {
      h.logMessage(L"Setting boot.ini TLS version: " + tls_version);
      settings_manager::write_boot_ini_key("tls", "version", utf8::cvt<std::string>(tls_version));
    }
    if (!tls_verify_mode.empty()) {
      h.logMessage(L"Setting boot.ini TLS verify mode: " + tls_verify_mode);
      settings_manager::write_boot_ini_key("tls", "verify mode", utf8::cvt<std::string>(tls_verify_mode));
    }
    if (!tls_ca.empty()) {
      h.logMessage(L"Setting boot.ini TLS CA: " + tls_ca);
      settings_manager::write_boot_ini_key("tls", "ca", utf8::cvt<std::string>(tls_ca));
    }

    if (!update_nsclient_ini) {
      h.logMessage("Changing context: " + target_context);
      settings_manager::set_boot_ini_primary(target_context);

      h.logMessage(L"Not updating nsclient.ini");
      return ERROR_SUCCESS;
    }

    h.logMessage("Switching to: " + target_context);
    settings_manager::change_context(target_context);

    while (data.has_more()) {
      unsigned int mode = data.get_next_int();
      std::string path = utf8::cvt<std::string>(data.get_next_string());
      std::string key = utf8::cvt<std::string>(data.get_next_string());
      std::string val = utf8::cvt<std::string>(data.get_next_string());

      if (mode == 1) {
        h.logMessage("Set key: " + path + "/" + key + " = " + val);
        settings_manager::get_settings()->set_string(path, key, val);
      } else if (mode == 2) {
        h.logMessage("***UNSUPPORTED*** Remove key: " + path + "/" + key + " = " + val);
      } else {
        h.errorMessage(L"Unknown mode in CA data: " + strEx::xtos(mode) + L": " + data.to_string());
        return ERROR_INSTALL_FAILURE;
      }
    }
    h.logMessage("Saving settings, not updating existing keys: " + target_context);
    settings_manager::get_settings()->save(do_import);
  } catch (const installer_exception &e) {
    h.errorMessage(L"Failed to write configuration: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Failed to write configuration: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to write configuration: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fleet onboarding
//
// FLEET_SERVER + FLEET_TOKEN (the install command generated by the fleet
// server) enroll the host while it is being installed: generate a keypair,
// POST a CSR with the one-time bootstrap token and store the returned
// certificate material as ${certificate-path}/agent-state.json inside the
// install folder. The core starts its fleet sync worker at boot whenever that
// manifest exists - there is no module to enable - so writing it here is all
// the installer has to do to hand the host over to the fleet server.
//
// Split over two custom actions for the usual reason: only a deferred action
// runs elevated (and after InstallFiles, so the install folder exists), and a
// deferred action can read nothing but CustomActionData. So the immediate half
// validates the properties - failing before a single file has been copied when
// the operator got the command line wrong - and hands the values across.

namespace {

bool is_true(const std::wstring &value) {
  const std::wstring v = boost::algorithm::to_lower_copy(boost::algorithm::trim_copy(value));
  return v == L"1" || v == L"true" || v == L"yes";
}

std::wstring url_scheme(const std::wstring &url) {
  const std::wstring::size_type sep = url.find(L"://");
  if (sep == std::wstring::npos) return L"";
  return boost::algorithm::to_lower_copy(url.substr(0, sep));
}

std::string url_scheme(const std::string &url) {
  const std::string::size_type sep = url.find("://");
  if (sep == std::string::npos) return "";
  return boost::algorithm::to_lower_copy(url.substr(0, sep));
}

// ${shared-path}, ${exe-path} and ${base-path} all mean the install folder to
// the installer, exactly as installer_settings_provider::expand_path resolves
// them. Used to turn the compiled-in CERT_FOLDER token into a real path
// without booting the settings subsystem (which the deferred action cannot do
// before the configuration has been written).
std::string expand_install_path(const std::string &token, const std::string &install_folder, const std::string &shared_folder) {
  std::string result = token;
  // ${shared-path} is where the writable state lives, which is the install
  // folder only on the legacy layout. Callers that have not been taught about
  // the layout pass the install folder for both and get the old behaviour.
  str::utils::replace(result, "${shared-path}", shared_folder.empty() ? install_folder : shared_folder);
  str::utils::replace(result, "${exe-path}", install_folder);
  str::utils::replace(result, "${base-path}", install_folder);
  return result;
}

// MsiGetTargetPath hands back a trailing backslash; strip it so the paths we
// build (and log) do not come out as "...\NSClient++\/security".
std::string as_install_folder(const std::wstring &target) {
  std::string folder = utf8::cvt<std::string>(target);
  while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/')) {
    folder.pop_back();
  }
  return folder;
}

// The sync worker renders the fleet-managed configuration into the fleet folder
// on its first run. Create a placeholder so the include ScheduleWriteConfig adds
// is not a dangling reference until then - also when the host was already
// enrolled and this install had nothing else to do, since the include is
// (re)written either way.
//
// This expands FLEET_FOLDER, the ${fleet-folder} default, rather than resolving
// the token: a custom action runs without the core's path resolver. Both agree
// unless someone has relocated the token, which an MSI install has no way to
// know about anyway.
void ensure_fleet_ini(const std::string &install_folder, const std::string &shared_folder) {
  const std::string fleet_ini = expand_install_path(std::string(FLEET_FOLDER) + "/fleet.ini", install_folder, shared_folder);
  boost::system::error_code ec;
  const boost::filesystem::path fleet_dir = boost::filesystem::path(fleet_ini).parent_path();
  if (!fleet_dir.empty()) boost::filesystem::create_directories(fleet_dir, ec);
  if (boost::filesystem::exists(fleet_ini, ec)) return;
  std::ofstream placeholder(fleet_ini.c_str());
  placeholder << "; Managed by the fleet sync - populated on the first sync." << std::endl;
}

// The enrollment call is plain HTTPS (no client certificate yet), so it needs
// a trust anchor on disk. The service exports the Windows ROOT store to
// ${ca-path} at boot, but nothing has done so during an install, so we export
// our own copy to a temp file and remove it again afterwards - the same trick
// installer_settings_provider::prepare_trust_store() plays for an https://
// settings source.
class temp_ca_bundle {
  std::string path_;

 public:
  ~temp_ca_bundle() {
    if (path_.empty()) return;
    boost::system::error_code ec;
    boost::filesystem::remove(path_, ec);
  }
  const std::string &path() const { return path_; }

  // Returns an empty string on success, otherwise a description of why no
  // anchor could be produced (which is fatal for the enrollment: see the
  // caller).
  std::string export_root_store(msi_helper &h) {
    const boost::filesystem::path bundle = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("nscp-enroll-ca-%%%%%%%%.pem");
    std::list<std::string> ca_errors;
    const unsigned int count = nsclient::windows_ca::export_root_store(bundle.string(), ca_errors);
    for (const std::string &e : ca_errors) {
      h.logMessage("windows-ca: " + e);
    }
    if (count == 0) {
      boost::system::error_code ec;
      boost::filesystem::remove(bundle, ec);
      return "no certificates could be exported from the Windows ROOT store";
    }
    path_ = bundle.string();
    h.logMessage("Exported " + str::xtos(count) + " Windows ROOT certificates to verify the fleet server");
    return "";
  }
};

}  // namespace

// Decide the layout and hand the answer to the deferred half.
//
// Scheduled before the fleet enrollment and the configuration write so their
// deferred halves run after this one: both write into ${shared-path}, and on
// the modern layout that folder has to exist, be locked down, and have the old
// installation's files moved into it first.
// Give component conditions a costing-time view of the layout: read
// `[layout] mode` from the installed boot.ini into CURRENT_LAYOUT, before
// CostFinalize evaluates the conditions.
//
// This exists for the NSClientConfig components. Their key file is
// INSTALLLOCATION\nsclient.ini, which is *absent* on a modern host (the
// configuration lives in %ProgramData%), so NeverOverwrite never suppresses
// them and every upgrade or repair lays a fresh default configuration into
// Program Files - where the migration then reports it blocked, nothing cleans
// it up, and an admin edits the wrong file. With CURRENT_LAYOUT=modern the
// components are simply not installed.
//
// Deliberately boot.ini only, not the LAYOUT property: on the *first* switch
// the host is still legacy at costing time, the template is wanted (it is what
// the migration moves), and ExecPrepareLayout does the moving later.
//
// Best effort throughout: not setting the property keeps today's behaviour,
// which is also the pre-layout behaviour every legacy host already has.
extern "C" UINT __stdcall ReadLayout(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ReadLayout");
  try {
    // Candidate install folders, most authoritative first. Running before
    // CostFinalize is the whole point of this action, but it also means the
    // directory manager has not resolved INSTALLLOCATION yet: MsiGetTargetPath
    // normally fails here, and the INSTALLLOCATION *property* is only set when
    // the operator passed one on the command line. DEFAULT_INSTALLLOCATION is
    // a type-51 rendering of the Directory-table default
    // ([ProgramFiles..Folder]NSClient++, see ReadLayout.DefaultLocation in
    // Product.wxs), which is where every unattended install actually lives -
    // without that fallback an upgrade of a modern host resolves no folder at
    // all, leaves CURRENT_LAYOUT unset, and the default configuration template
    // lands in Program Files again.
    std::vector<std::string> candidates;
    try {
      candidates.push_back(as_install_folder(h.getTargetPath(L"INSTALLLOCATION")));
    } catch (const installer_exception &) {
      // Not resolved yet - expected before CostFinalize; the properties below
      // carry the answer.
    }
    candidates.push_back(as_install_folder(h.getMsiPropery(L"INSTALLLOCATION")));
    candidates.push_back(as_install_folder(h.getMsiPropery(L"DEFAULT_INSTALLLOCATION")));
    for (const std::string &install_folder : candidates) {
      if (install_folder.empty()) continue;
      const boost::filesystem::path boot_ini = boost::filesystem::path(install_folder) / "boot.ini";
      if (!boost::filesystem::exists(boot_ini)) continue;
      // The first boot.ini that exists decides: it is the installed agent's
      // own record of its layout.
      if (nscp::paths::layout_from_boot_ini_file(boot_ini.string()) == nscp::paths::layout::modern) {
        h.setPropertyValue(L"CURRENT_LAYOUT", L"modern");
        h.logMessage("This host is on the modern layout: leaving the default configuration template out of the install folder");
      }
      return ERROR_SUCCESS;
    }
    return ERROR_SUCCESS;
  } catch (...) {
    return ERROR_SUCCESS;
  }
}

extern "C" UINT __stdcall SchedulePrepareLayout(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"SchedulePrepareLayout");
  try {
    const std::string install_folder = as_install_folder(h.getTargetPath(L"INSTALLLOCATION"));
    const std::wstring requested = h.getMsiPropery(LAYOUT_MODE);
    if (!requested.empty() && !nscp::paths::is_known_layout(utf8::cvt<std::string>(boost::algorithm::trim_copy(requested)))) {
      // Do not guess: an unrecognised value keeps the layout the host has.
      h.logMessage(L"Unknown LAYOUT '" + requested + L"'; keeping the current layout. Use LAYOUT=modern or LAYOUT=legacy.");
    }
    const nscp::paths::layout layout = resolve_layout(install_folder, requested);
    const resolved_shared_folder shared = shared_folder_for(layout, install_folder, &h);
    if (!shared.resolved) {
      // Failing is the only honest answer here: the deferred half would
      // otherwise ACL-lock the install folder, migrate nothing, and stamp a
      // layout whose files are not where the service will look for them.
      h.errorMessage(
          L"LAYOUT: this host uses the modern layout but %ProgramData% could not be determined (SHGetFolderPath(CSIDL_COMMON_APPDATA) failed), so there is "
          L"no correct place to put the writable state. Repair the machine's shell folders and run the installer again.");
      return ERROR_INSTALL_FAILURE;
    }
    const std::string shared_folder = shared.folder;

    h.logMessage("Layout: " + std::string(nscp::paths::layout_name(layout)));
    h.logMessage("Shared folder: " + shared_folder);
    const std::string trimmed = boost::algorithm::trim_copy(utf8::cvt<std::string>(requested));
    if (!trimmed.empty() && nscp::paths::is_known_layout(trimmed) && nscp::paths::parse_layout(trimmed) != layout) {
      h.logMessage(L"WARNING: LAYOUT=" + requested + L" was not applied; this installation stays on '" +
                   utf8::cvt<std::wstring>(std::string(nscp::paths::layout_name(layout))) +
                   L"'. Moving back to the legacy layout is not supported; uninstall and reinstall instead.");
    }

    msi_helper::custom_action_data_w data;
    data.write_string(utf8::cvt<std::wstring>(install_folder));
    data.write_string(utf8::cvt<std::wstring>(shared_folder));
    data.write_string(utf8::cvt<std::wstring>(std::string(nscp::paths::layout_name(layout))));
    const HRESULT hr = h.do_deferred_action(L"ExecPrepareLayout", data, 1000);
    if (FAILED(hr)) {
      h.errorMessage(L"failed to schedule the layout preparation");
      return hr;
    }
    return ERROR_SUCCESS;
  } catch (const std::exception &e) {
    h.setError(L"SchedulePrepareLayout", utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.setError(L"SchedulePrepareLayout", L"Unknown exception");
    return ERROR_INSTALL_FAILURE;
  }
}

// Create the shared folder, restrict it, move an existing installation's files
// into it, and record the layout in boot.ini - in that order, and before
// anything else writes there.
extern "C" UINT __stdcall ExecPrepareLayout(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecPrepareLayout");
  try {
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    const std::string install_folder = as_install_folder(data.get_next_string());
    const std::string shared_folder = utf8::cvt<std::string>(data.get_next_string());
    const std::string layout_name = utf8::cvt<std::string>(data.get_next_string());
    const nscp::paths::layout layout = nscp::paths::parse_layout(layout_name);

    const boost::filesystem::path boot_ini = boost::filesystem::path(install_folder) / "boot.ini";

    // Legacy is what the agent assumes when the key is absent, so there is
    // nothing to create, nothing to move and nothing to record - and recording
    // it anyway would edit the boot.ini of every installation that never asked
    // for any of this.
    if (layout != nscp::paths::layout::modern) {
      h.logMessage("Layout: legacy, leaving " + boot_ini.string() + " untouched");
      return ERROR_SUCCESS;
    }

    // Everything below is modern-only: create the folder, lock it down, then
    // move the old installation's files into it.
    {
      boost::system::error_code ec;
      boost::filesystem::create_directories(shared_folder, ec);
      if (ec) {
        h.setError(L"ExecPrepareLayout", L"Failed to create " + utf8::cvt<std::wstring>(shared_folder) + L": " + utf8::to_unicode(ec.message()));
        return ERROR_INSTALL_FAILURE;
      }
      // Before anything is written into it: %ProgramData% grants
      // Users: Read & Execute by inheritance, and this folder is about to hold
      // the configuration (passwords) and the fleet identity's private key.
      std::list<std::string> acl_errors;
      if (!nsclient::windows_acl::protect_directory(shared_folder, acl_errors)) {
        for (const std::string &e : acl_errors) h.logMessage("acl: " + e);
        h.setError(L"ExecPrepareLayout", L"Failed to restrict " + utf8::cvt<std::wstring>(shared_folder) + L" to SYSTEM and administrators");
        return ERROR_INSTALL_FAILURE;
      }

      // An upgrade from the legacy layout: move what the old installation left
      // in the install folder. A no-op on a fresh install because there is
      // nothing there yet.
      if (shared_folder != install_folder) {
        // First switch vs. an upgrade of an install already on the modern
        // layout. On the first switch the folder we just created and adopted
        // must be empty - a file already in %ProgramData%\NSClient++ is planted
        // or leftover, not ours, and must not be adopted (round-2 #3). On an
        // already-modern upgrade the destination legitimately holds the state
        // migrated last time, so keep the idempotent behaviour.
        const nscp::paths::layout current = nscp::paths::layout_from_boot_ini_file(boot_ini.string());
        const nscp::paths::destination_policy policy =
            current == nscp::paths::layout::modern ? nscp::paths::destination_policy::adopt_existing : nscp::paths::destination_policy::require_pristine;
        const nscp::paths::migration_report report = nscp::paths::apply_migration(install_folder, shared_folder, policy);
        for (const std::string &line : report.describe()) h.logMessage("layout: " + line);
        if (!report.ok()) {
          h.setError(L"ExecPrepareLayout", L"Failed to move the existing configuration to " + utf8::cvt<std::wstring>(shared_folder));
          return ERROR_INSTALL_FAILURE;
        }
      }
    }

    // Only now, once the files are actually there.
    CSimpleIni boot_conf;
    const SI_Error load_result = boot_conf.LoadFile(boot_ini.string().c_str());
    // LoadFile returns an error both when the file is genuinely absent (the
    // fresh-install case: create it) and when it exists but could not be read -
    // locked by AV/backup, or a decode error on non-UTF bytes in a [settings]
    // URL or [paths] value. Rewriting in the second case would replace the
    // operator's [settings] store list, [tls] trust config and [paths]
    // overrides with a bare [layout] stub, so the agent boots on the next start
    // with no remote configuration and no CA. Tell the two apart by existence
    // and refuse to clobber.
    boost::system::error_code exists_ec;
    if (boost::filesystem::exists(boot_ini, exists_ec) && load_result < 0) {
      h.setError(L"ExecPrepareLayout", L"Refusing to overwrite " + utf8::cvt<std::wstring>(boot_ini.string()) +
                                           L": it exists but could not be read (error " + strEx::xtos(static_cast<int>(load_result)) +
                                           L"). Recording the layout would discard its other sections.");
      return ERROR_INSTALL_FAILURE;
    }
    boot_conf.SetValue(L"layout", L"mode", utf8::cvt<std::wstring>(layout_name).c_str());
    if (boot_conf.SaveFile(boot_ini.string().c_str()) < 0) {
      h.setError(L"ExecPrepareLayout", L"Failed to write " + utf8::cvt<std::wstring>(boot_ini.string()));
      return ERROR_INSTALL_FAILURE;
    }
    h.logMessage("Recorded layout '" + layout_name + "' in " + boot_ini.string());
    return ERROR_SUCCESS;
  } catch (const std::exception &e) {
    h.setError(L"ExecPrepareLayout", utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.setError(L"ExecPrepareLayout", L"Unknown exception");
    return ERROR_INSTALL_FAILURE;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Operator-supplied TLS material (GitHub #568)
//
// CERTIFICATE, CERTIFICATE_KEY and CERTIFICATE_CA name PEM files on the
// installing machine to install as ${certificate-path}/certificate.pem,
// certificate_key.pem and ca.pem - the default names every server module
// reads. With the files in place the service never generates its self-signed
// fallback (that only happens when the file at the default path is missing),
// so this is how a silent install ships CA-signed material instead of
// self-signed certificates.
//
// Split over two custom actions for the usual reason: the immediate half
// validates the properties - failing before a single file has been copied
// when the operator got the command line wrong - and the deferred half runs
// elevated, after ExecPrepareLayout has created (and on the modern layout
// secured) the folder the files go into.

namespace {

// What lands where. certificate_key.pem is also the file ScheduleWriteConfig
// points the `certificate key` settings at, and all three names are already
// in the uninstall cleanup lists (the RemoveFile rows in Product.wxs and
// removable_security_files below) - the operator keeps the originals, so the
// copies do not outlive the installation.
struct cert_property {
  const wchar_t *property;
  const char *file_name;
  // A cheap sanity marker so CERTIFICATE=<key file> (and friends) fails here,
  // naming the property, instead of as a TLS handshake error weeks later.
  // "PRIVATE KEY-----" matches the PKCS#8, RSA/EC and encrypted PEM headers
  // alike.
  const char *pem_marker;
  // What the file was expected to contain, for the error message.
  const wchar_t *expected;
};
const cert_property cert_properties[] = {
    {CERTIFICATE, "certificate.pem", "-----BEGIN CERTIFICATE-----", L"a PEM certificate"},
    {CERTIFICATE_KEY, "certificate_key.pem", "PRIVATE KEY-----", L"a PEM private key"},
    {CERTIFICATE_CA, "ca.pem", "-----BEGIN CERTIFICATE-----", L"a PEM certificate (bundle)"},
};

bool file_contains_marker(const std::string &file, const char *marker) {
  std::ifstream in(file.c_str(), std::ios::binary);
  if (!in) return false;
  const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return content.find(marker) != std::string::npos;
}

}  // namespace

extern "C" UINT __stdcall ScheduleInstallCerts(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ScheduleInstallCerts");
  try {
    const std::wstring certificate = boost::algorithm::trim_copy(h.getMsiPropery(CERTIFICATE));
    const std::wstring certificate_key = boost::algorithm::trim_copy(h.getMsiPropery(CERTIFICATE_KEY));
    const std::wstring ca = boost::algorithm::trim_copy(h.getMsiPropery(CERTIFICATE_CA));
    if (certificate.empty() && certificate_key.empty() && ca.empty()) {
      h.logMessage(L"No CERTIFICATE/CERTIFICATE_KEY/CERTIFICATE_CA given: not installing any TLS material");
      return ERROR_SUCCESS;
    }

    // Everything below fails the install rather than installing an agent whose
    // TLS material is not what the operator asked for: the service would fall
    // back to a generated self-signed certificate (or refuse to serve), and on
    // an unattended install nobody reads the log that says why.
    if (!certificate_key.empty() && certificate.empty()) {
      h.errorMessage(
          L"CERTIFICATE_KEY was given without CERTIFICATE. A private key on its own is never read - pass CERTIFICATE=<file> with the certificate it "
          L"belongs to (or put both in one file and pass only CERTIFICATE).");
      return ERROR_INSTALL_FAILURE;
    }
    // A separate key file is only used where a `certificate key` setting
    // points at it (the modules default to reading the key from the
    // certificate file itself), and those settings are written by
    // ScheduleWriteConfig - the exact test repeated here so we fail precisely
    // when they would not be written, which would leave every server trying
    // to read a private key from certificate.pem and failing to start TLS.
    if (!certificate_key.empty() && h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      std::wstring reason = boost::algorithm::trim_copy(h.getMsiPropery(INT_CONF_CAN_CHANGE_REASON));
      if (reason.empty()) reason = L"the installer is not allowed to change the configuration";
      h.errorMessage(L"Refusing to install a separate certificate key: the configuration cannot be changed (" + reason +
                     L"), so the `certificate key` settings pointing the servers at it cannot be written. Let the installer write the configuration (do "
                     L"not pass ALLOW_CONFIGURATION=0), or concatenate the key into the certificate file and pass only CERTIFICATE.");
      return ERROR_INSTALL_FAILURE;
    }

    for (const cert_property &p : cert_properties) {
      const std::wstring value = boost::algorithm::trim_copy(h.getMsiPropery(p.property));
      if (value.empty()) continue;
      const std::string file = utf8::cvt<std::string>(value);
      boost::system::error_code ec;
      if (!boost::filesystem::is_regular_file(file, ec)) {
        h.errorMessage(std::wstring(p.property) + L"=" + value + L" does not name a file on this machine.");
        return ERROR_INSTALL_FAILURE;
      }
      if (!file_contains_marker(file, p.pem_marker)) {
        h.errorMessage(std::wstring(p.property) + L"=" + value + L" could not be read or does not look like " + p.expected +
                       L" (expected the file to contain \"" + utf8::cvt<std::wstring>(p.pem_marker) + L"\").");
        return ERROR_INSTALL_FAILURE;
      }
    }

    // The servers read the private key from the certificate file when no
    // `certificate key` is configured, so a certificate without an embedded
    // key needs the separate key file - catch the combination that would pass
    // every per-file check above and still serve nothing.
    if (!certificate.empty() && certificate_key.empty() && !file_contains_marker(utf8::cvt<std::string>(certificate), "PRIVATE KEY-----")) {
      h.errorMessage(L"CERTIFICATE=" + certificate +
                     L" contains no private key and no CERTIFICATE_KEY was given, so the servers would have no key to serve TLS with. Pass the key as "
                     L"CERTIFICATE_KEY=<file>, or concatenate certificate and key into one file.");
      return ERROR_INSTALL_FAILURE;
    }

    msi_helper::custom_action_data_w data;
    data.write_string(h.getTargetPath(L"INSTALLLOCATION"));
    data.write_string(certificate);
    data.write_string(certificate_key);
    data.write_string(ca);
    h.logMessage(L"Scheduling (ExecInstallCerts): " + data.to_string());
    const HRESULT hr = h.do_deferred_action(L"ExecInstallCerts", data, 1000);
    if (FAILED(hr)) {
      h.errorMessage(L"failed to schedule the certificate installation");
      return hr;
    }
    return ERROR_SUCCESS;
  } catch (const installer_exception &e) {
    h.errorMessage(L"Failed to schedule the certificate installation: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Failed to schedule the certificate installation: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to schedule the certificate installation: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
}

extern "C" UINT __stdcall ExecInstallCerts(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecInstallCerts");
  try {
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    const std::string install_folder = as_install_folder(data.get_next_string());
    // Same order the immediate half wrote them, which is the order of
    // cert_properties.
    const std::string sources[] = {utf8::cvt<std::string>(data.get_next_string()), utf8::cvt<std::string>(data.get_next_string()),
                                   utf8::cvt<std::string>(data.get_next_string())};

    // Where the service will look for the files, which depends on the layout -
    // ExecPrepareLayout has already created (and on the modern layout secured)
    // the shared folder and recorded the choice in boot.ini, so reading it
    // back here is how the two agree.
    const resolved_shared_folder shared = shared_folder_for(resolve_layout(install_folder, L""), install_folder, &h);
    if (!shared.resolved) {
      h.errorMessage(
          L"Cannot install the TLS material: this host uses the modern layout but %ProgramData% could not be determined, so the files cannot be placed "
          L"where the service will look for them.");
      return ERROR_INSTALL_FAILURE;
    }
    const std::string security_folder = expand_install_path(CERT_FOLDER, install_folder, shared.folder);

    boost::system::error_code ec;
    boost::filesystem::create_directories(security_folder, ec);
    if (ec) {
      h.errorMessage(L"Cannot install the TLS material: could not create " + utf8::cvt<std::wstring>(security_folder) + L": " +
                     utf8::cvt<std::wstring>(ec.message()));
      return ERROR_INSTALL_FAILURE;
    }

    std::size_t index = 0;
    for (const cert_property &p : cert_properties) {
      const std::string source = sources[index++];
      if (source.empty()) continue;
      const boost::filesystem::path target = boost::filesystem::path(security_folder) / p.file_name;
      // Overwriting is deliberate, and different from the fleet identity: the
      // property is an explicit ask, and the operator still has the source
      // file - unlike a generated key, nothing is lost by replacing it.
      if (boost::filesystem::exists(target, ec)) {
        h.logMessage("Replacing the existing " + target.string());
      }
      boost::filesystem::copy_file(source, target, boost::filesystem::copy_options::overwrite_existing, ec);
      if (ec) {
        h.errorMessage(L"Failed to install " + utf8::cvt<std::wstring>(source) + L" as " + utf8::cvt<std::wstring>(target.string()) + L": " +
                       utf8::cvt<std::wstring>(ec.message()));
        return ERROR_INSTALL_FAILURE;
      }
      h.logMessage("Installed " + source + " as " + target.string());
    }
    return ERROR_SUCCESS;
  } catch (const installer_exception &e) {
    h.errorMessage(L"Failed to install the TLS material: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Failed to install the TLS material: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to install the TLS material: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
}

// Uninstall cleanup for the modern layout.
//
// The RemoveFile rows under INSTALLLOCATION_SECURITY take care of the legacy
// layout, but on the modern one this host's client certificate, the
// bundle-signing trust anchor and the fleet identity's private key live in
// %ProgramData%\NSClient++\security, which the MSI knows nothing about - so
// uninstalling an enrolled host used to leave all of it on disk.
//
// The immediate half resolves the shared folder while boot.ini is still there
// to read; the deferred half does the deleting, elevated.
namespace {
// The generated, host-specific material - the files that must not outlive the
// installation. Deliberately the same list as the RemoveFile rows in
// Product.wxs plus the trust-store export, and deliberately not nsclient.ini:
// an operator's configuration is theirs to keep, as it has always been.
const char *const removable_security_files[] = {"certificate.pem", "certificate_key.pem", "ca.pem", "agent-state.json", "windows-ca.pem"};
}  // namespace

extern "C" UINT __stdcall ScheduleRemoveSecrets(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ScheduleRemoveSecrets");
  try {
    const std::string install_folder = as_install_folder(h.getTargetPath(L"INSTALLLOCATION"));
    const nscp::paths::layout layout = resolve_layout(install_folder, L"");
    const resolved_shared_folder shared = shared_folder_for(layout, install_folder, &h);
    if (!shared.resolved) {
      // An uninstall must not fail over this: leaving the product half-removed
      // is worse than a file that needs deleting by hand. Say which files.
      h.logMessage(
          L"WARNING: %ProgramData% could not be determined, so the generated security material (certificate.pem, certificate_key.pem, ca.pem, "
          L"agent-state.json, windows-ca.pem) under the shared security folder was not removed; delete it by hand.");
      return ERROR_SUCCESS;
    }
    const std::string shared_folder = shared.folder;
    if (shared_folder == install_folder) {
      // Legacy layout: the RemoveFile rows already cover this folder.
      h.logMessage("Layout: legacy, leaving the security folder to the RemoveFile rows");
      return ERROR_SUCCESS;
    }

    h.logMessage("Removing the generated security material from " + shared_folder);
    msi_helper::custom_action_data_w data;
    data.write_string(utf8::cvt<std::wstring>(shared_folder));
    const HRESULT hr = h.do_deferred_action(L"ExecRemoveSecrets", data, 1000);
    if (FAILED(hr)) {
      h.errorMessage(L"failed to schedule the removal of the generated security material");
      return hr;
    }
    return ERROR_SUCCESS;
  } catch (const std::exception &e) {
    h.setError(L"ScheduleRemoveSecrets", utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.setError(L"ScheduleRemoveSecrets", L"Unknown exception");
    return ERROR_INSTALL_FAILURE;
  }
}

extern "C" UINT __stdcall ExecRemoveSecrets(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecRemoveSecrets");
  try {
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    const boost::filesystem::path security = boost::filesystem::path(utf8::cvt<std::string>(data.get_next_string())) / "security";

    for (const char *const name : removable_security_files) {
      const boost::filesystem::path file = security / name;
      boost::system::error_code ec;
      if (!boost::filesystem::exists(file, ec)) continue;
      if (!boost::filesystem::remove(file, ec) || ec) {
        // Not fatal: failing an uninstall leaves the product half-removed,
        // which is worse than a file we could not delete. Say so loudly enough
        // that an operator who cares can finish the job by hand.
        h.logMessage("WARNING: failed to remove " + file.string() + (ec ? ": " + ec.message() : std::string()));
        continue;
      }
      h.logMessage("Removed " + file.string());
    }

    // Only if we emptied it: anything the operator put there themselves is
    // theirs, and remove() on a non-empty directory fails harmlessly.
    boost::system::error_code ignored;
    boost::filesystem::remove(security, ignored);
    return ERROR_SUCCESS;
  } catch (const std::exception &e) {
    h.setError(L"ExecRemoveSecrets", utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.setError(L"ExecRemoveSecrets", L"Unknown exception");
    return ERROR_INSTALL_FAILURE;
  }
}

extern "C" UINT __stdcall ScheduleEnrollFleet(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ScheduleEnrollFleet");
  try {
    const std::wstring server = boost::algorithm::trim_copy(h.getMsiPropery(FLEET_SERVER));
    if (server.empty()) {
      h.logMessage(L"No FLEET_SERVER given: not enrolling with a fleet server");
      return ERROR_SUCCESS;
    }
    const std::wstring token = boost::algorithm::trim_copy(h.getMsiPropery(FLEET_TOKEN));
    const std::wstring verify_mode = boost::algorithm::trim_copy(h.getMsiPropery(FLEET_VERIFY_MODE));
    const bool insecure = is_true(h.getMsiPropery(FLEET_INSECURE));
    const std::wstring scheme = url_scheme(server);

    // Everything below fails the install rather than installing an agent that
    // silently never joined the fleet: enrollment was asked for explicitly, and
    // a host that is not enrolled is not managed.
    if (token.empty()) {
      h.errorMessage(
          L"FLEET_SERVER was given but FLEET_TOKEN is empty. Generate an install command on the fleet server and pass its bootstrap token as "
          L"FLEET_TOKEN=<token>.");
      return ERROR_INSTALL_FAILURE;
    }
    if (scheme != L"https" && scheme != L"http") {
      h.errorMessage(L"FLEET_SERVER must be a url including the scheme, for example https://fleet.example.com (got: " + server + L").");
      return ERROR_INSTALL_FAILURE;
    }
    // The bootstrap token exchanges for a client certificate; over plain HTTP
    // anyone on the network path can read it and enroll as this host. Same
    // rule as `nscp enroll`: opt in with FLEET_INSECURE=1.
    if (scheme == L"http" && !insecure) {
      h.errorMessage(
          L"Refusing to enroll over plain HTTP: the bootstrap token would be sent in cleartext, so anyone on the network path could read it and "
          L"enroll as this host. Use an https:// FLEET_SERVER url, or pass FLEET_INSECURE=1 to allow plain HTTP anyway.");
      return ERROR_INSTALL_FAILURE;
    }
    // Enrollment is where this agent decides who the fleet server is: the
    // response carries the certificate every later call pins against and the
    // key that authorises executable bundles. Handing both to whoever answers
    // an unverified connection is a decision the operator has to make out loud.
    if (boost::algorithm::iequals(verify_mode, L"none") && !insecure) {
      h.errorMessage(
          L"Refusing to enroll without verifying the fleet server certificate (FLEET_VERIFY_MODE=none). The enrollment response supplies the "
          L"certificate this agent pins for every later call and the key it trusts for executable bundles, so an unverified enrollment hands both "
          L"to whoever answers. Point FLEET_CA at the issuing CA (recommended), or pass FLEET_INSECURE=1 to accept it anyway.");
      return ERROR_INSTALL_FAILURE;
    }

    // The include of the fleet-managed configuration is written by
    // ScheduleWriteConfig, and only when it is allowed to change the
    // configuration - the exact test repeated here, so we fail precisely when
    // the include would not be written. Without it the host enrolls, reports
    // in and syncs, but nothing ever reads what the fleet server sends back:
    // managed on paper, unmanaged in practice. The enrollment that follows
    // burns the bootstrap token, so this has to fail before the token is
    // spent, not leave a note in a log nobody reads on an unattended install.
    if (h.getMsiPropery(INT_CONF_CAN_CHANGE) != L"1") {
      std::wstring reason = boost::algorithm::trim_copy(h.getMsiPropery(INT_CONF_CAN_CHANGE_REASON));
      if (reason.empty()) reason = L"the installer is not allowed to change the configuration";
      h.errorMessage(L"Refusing to enroll with a fleet server: the configuration cannot be changed (" + reason +
                     L"), so the include of the fleet-managed configuration ([/includes] fleet=${fleet-folder}/fleet.ini) cannot be added and this host "
                     L"would never read the configuration the fleet server sends it. Let the installer write the configuration (do not pass "
                     L"ALLOW_CONFIGURATION=0, or pass CONF_CAN_CHANGE=1), or install without FLEET_SERVER/FLEET_TOKEN and enroll afterwards with `nscp enroll` "
                     L"once the include is in place.");
      return ERROR_INSTALL_FAILURE;
    }

    msi_helper::custom_action_data_w data;
    data.write_string(h.getTargetPath(L"INSTALLLOCATION"));
    data.write_string(server);
    data.write_string(token);
    data.write_string(boost::algorithm::trim_copy(h.getMsiPropery(FLEET_HOSTNAME)));
    data.write_string(boost::algorithm::trim_copy(h.getMsiPropery(FLEET_CA)));
    data.write_string(verify_mode);
    data.write_int(insecure ? 1 : 0);

    // Deliberately not logged: it carries the bootstrap token (which is also
    // why FLEET_TOKEN and ExecEnrollFleet are in MsiHiddenProperties).
    h.logMessage(L"Scheduling fleet enrollment (ExecEnrollFleet) with: " + server);
    const HRESULT hr = h.do_deferred_action(L"ExecEnrollFleet", data, COST_SERVICE_INSTALL);
    if (hr == ERROR_INSTALL_USEREXIT) return ERROR_INSTALL_USEREXIT;
    if (FAILED(hr)) {
      h.errorMessage(L"Failed to schedule the fleet enrollment.");
      return ERROR_INSTALL_FAILURE;
    }
  } catch (const installer_exception &e) {
    h.errorMessage(L"Failed to schedule the fleet enrollment: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Failed to schedule the fleet enrollment: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to schedule the fleet enrollment: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall ExecEnrollFleet(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"ExecEnrollFleet");
  try {
    msi_helper::custom_action_data_r data(h.getMsiPropery(L"CustomActionData"));
    const std::string install_folder = as_install_folder(data.get_next_string());
    onboarding::enrollment_request request;
    request.server_url = utf8::cvt<std::string>(boost::algorithm::trim_copy(data.get_next_string()));
    request.bootstrap_token = utf8::cvt<std::string>(boost::algorithm::trim_copy(data.get_next_string()));
    request.hostname = utf8::cvt<std::string>(boost::algorithm::trim_copy(data.get_next_string()));
    request.ca = utf8::cvt<std::string>(boost::algorithm::trim_copy(data.get_next_string()));
    request.verify_mode = utf8::cvt<std::string>(boost::algorithm::trim_copy(data.get_next_string()));
    const bool insecure = data.get_next_int() == 1;

    // The enrollment manifest lives where the service looks for it:
    // ${certificate-path}/agent-state.json, i.e. inside the install folder.
    // Where the service will look for it, which depends on the layout this
    // install is using - ExecPrepareLayout has already created and secured that
    // folder and written the choice into boot.ini, so reading it back here is
    // how the two agree.
    const resolved_shared_folder enroll_shared = shared_folder_for(resolve_layout(install_folder, L""), install_folder, &h);
    if (!enroll_shared.resolved) {
      h.errorMessage(
          L"Fleet enrollment failed: this host uses the modern layout but %ProgramData% could not be determined, so the enrollment manifest cannot be "
          L"written where the service will look for it.");
      return ERROR_INSTALL_FAILURE;
    }
    const std::string shared_folder = enroll_shared.folder;
    const std::string state_file = expand_install_path(std::string(CERT_FOLDER) + "/agent-state.json", install_folder, shared_folder);
    h.logMessage("Fleet server: " + request.server_url);
    h.logMessage("Enrollment manifest: " + state_file);

    // An existing identity is never replaced: it is the only copy of this
    // host's private key, and the fleet server knows the certificate that goes
    // with it. Re-running the installer (upgrade, repair, re-install over an
    // enrolled host) therefore keeps the host enrolled as it was - which also
    // means an install that failed and rolled back after this point does not
    // need a second bootstrap token on the retry (the first one is burned
    // server-side the moment it is used).
    boost::system::error_code ec;
    if (boost::filesystem::exists(state_file, ec)) {
      h.logMessage(L"This host is already enrolled: keeping the existing identity (delete the manifest above to enroll again).");
      ensure_fleet_ini(install_folder, shared_folder);
      return ERROR_SUCCESS;
    }

    const boost::filesystem::path state_dir = boost::filesystem::path(state_file).parent_path();
    if (!state_dir.empty()) {
      boost::filesystem::create_directories(state_dir, ec);
      if (ec) {
        h.errorMessage(L"Fleet enrollment failed: could not create " + utf8::cvt<std::wstring>(state_dir.string()) + L": " +
                       utf8::cvt<std::wstring>(ec.message()));
        return ERROR_INSTALL_FAILURE;
      }
    }

    // Only an https:// enrollment has a certificate to verify. A plain HTTP
    // one - which the immediate half allows only with FLEET_INSECURE=1 - makes
    // no TLS handshake at all, so exporting a trust anchor for it (and failing
    // the install when the ROOT store yields none) would refuse an install
    // over a check that is never made. Same rule as `nscp enroll`, which only
    // fills in a CA when the scheme is https.
    temp_ca_bundle bundle;
    const bool https = url_scheme(request.server_url) == "https";
    const bool verify_disabled = boost::algorithm::iequals(request.verify_mode, "none");
    if (https && !verify_disabled && request.ca.empty()) {
      const std::string error = bundle.export_root_store(h);
      if (!error.empty()) {
        h.errorMessage(L"Fleet enrollment failed: the fleet server certificate cannot be verified because " + utf8::cvt<std::wstring>(error) +
                       L". Point FLEET_CA at the issuing CA, or pass FLEET_VERIFY_MODE=none together with FLEET_INSECURE=1 to enroll without verifying the "
                       L"server (only on a trusted network).");
        return ERROR_INSTALL_FAILURE;
      }
      request.ca = bundle.path();
    }
    if (insecure && (!https || verify_disabled)) {
      h.logMessage(
          L"WARNING: enrolling over an unauthenticated connection (plain HTTP or FLEET_VERIFY_MODE=none). The bootstrap token was sent, and the trust "
          L"anchors stored by this enrollment received, over a network path nothing verified.");
    }

    h.logMessage(L"Enrolling with the fleet server...");
    const onboarding::enrolled_identity state = onboarding::enroll(request);
    onboarding::save_state(state, state_file);
    h.logMessage("Enrollment successful, agent API (mTLS): " + state.mtls_url);
    ensure_fleet_ini(install_folder, shared_folder);
  } catch (const onboarding::onboarding_error &e) {
    std::wstring message = L"Fleet enrollment failed: " + utf8::to_unicode(e.what());
    if (e.retryable()) {
      message +=
          L" This looks like a temporary problem (the fleet server was unreachable, busy or erroring); check the network path to the server and "
          L"install again.";
    }
    h.errorMessage(message);
    return ERROR_INSTALL_FAILURE;
  } catch (const installer_exception &e) {
    h.errorMessage(L"Fleet enrollment failed: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (const std::exception &e) {
    h.errorMessage(L"Fleet enrollment failed: " + utf8::to_unicode(e.what()));
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Fleet enrollment failed: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall NeedUninstall(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"NeedUninstall");
  try {
    std::list<std::wstring> list = h.enumProducts();
    for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
      if ((*cit) == L"{E7CF81FE-8505-4D4A-8ED3-48949C8E4D5B}") {
        h.errorMessage(L"Found old NSClient++/OP5 client installed, will uninstall it now!");
        std::wstring command = L"msiexec /uninstall " + (*cit);
        const size_t cmd_len = command.length() + 1;
        wchar_t *cmd = new wchar_t[cmd_len];
        wcsncpy_s(cmd, cmd_len, command.c_str(), command.length());
        cmd[command.length()] = 0;
        PROCESS_INFORMATION pi;
        STARTUPINFO si;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);

        BOOL processOK = CreateProcess(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        delete[] cmd;
        if (processOK) {
          DWORD dwstate = WaitForSingleObject(pi.hProcess, 1000 * 60);
          if (dwstate == WAIT_TIMEOUT) h.errorMessage(L"Failed to wait for process (probably not such a big deal, the uninstall usualy takes alonger)!");
        } else {
          h.errorMessage(L"Failed to start process: " + utf8::cvt<std::wstring>(error::lookup::last_error()));
        }
      }
    }

  } catch (installer_exception &e) {
    h.errorMessage(L"Failed to start service: " + e.what());
    return ERROR_INSTALL_FAILURE;
  } catch (...) {
    h.errorMessage(L"Failed to start service: <UNKNOWN EXCEPTION>");
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
};

// Overwrite the ARP UninstallString and add a QuietUninstallString.
//
// Windows Installer's RegisterProduct standard action auto-populates the
// ARP entry's UninstallString. For this package it lands as
// `MsiExec.exe /I{ProductCode}` (the maintenance-dialog form) rather than
// the canonical `/X{ProductCode}` form - GitHub issue #495. Most automation
// (Icinga2 autodiscovery, Chocolatey, SCCM removal scripts) reads
// UninstallString verbatim and breaks on `/I` because it spawns an
// interactive dialog instead of uninstalling.
//
// The right place to land this fix is *after* RegisterProduct in the
// install sequence; otherwise MSI's auto-write overwrites whatever we
// put in. So this is a deferred custom action sequenced via
// `<Custom Action="..." After="RegisterProduct">` in Product.wxs. We
// receive the ProductCode through CustomActionData (set by an immediate
// "set property" CA of the same name, also wired in Product.wxs) because
// deferred CAs only have access to that single property.
//
// We let Windows handle WoW64 redirection naturally: the 32-bit MSI's
// installer_lib.dll is a 32-bit binary and writes through the WoW64
// redirector (landing under WOW6432Node where the 32-bit ARP entry
// lives); the 64-bit MSI's DLL writes to the unredirected path. That
// matches where MSI itself wrote the entry.
extern "C" UINT __stdcall WriteArpUninstallStrings(MSIHANDLE hInstall) {
  msi_helper h(hInstall, L"WriteArpUninstallStrings");
  try {
    // Deferred CAs can read only CustomActionData (and a handful of
    // Windows Installer built-ins). The immediate "set property" CA
    // populates this with [ProductCode].
    wchar_t product[128] = {0};
    DWORD len = sizeof(product) / sizeof(product[0]);
    if (MsiGetProperty(hInstall, L"CustomActionData", product, &len) != ERROR_SUCCESS || product[0] == L'\0') {
      h.logMessage(L"WriteArpUninstallStrings: CustomActionData empty - skipping");
      return ERROR_SUCCESS;
    }

    std::wstring key_path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
    key_path += product;

    HKEY hKey = NULL;
    LONG rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path.c_str(), 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS) {
      // Not an installer-blocking failure - log and continue so the
      // install still completes. The user can always uninstall via
      // Settings > Apps if the canonical string is missing.
      h.logMessage(L"WriteArpUninstallStrings: failed to open Uninstall key for " + std::wstring(product) + L": " +
                   utf8::cvt<std::wstring>(error::lookup::last_error(rc)));
      return ERROR_SUCCESS;
    }

    const std::wstring uninstall_value = L"MsiExec.exe /X" + std::wstring(product);
    const std::wstring quiet_value = L"MsiExec.exe /X" + std::wstring(product) + L" /qn /norestart";

    // REG_EXPAND_SZ to match what Windows Installer itself writes (the
    // existing key is REG_EXPAND_SZ even though there are no actual
    // environment variables in the string - matching the type keeps tools
    // that inspect via Reg.exe / regedit happy).
    rc = RegSetValueExW(hKey, L"UninstallString", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE *>(uninstall_value.c_str()),
                        static_cast<DWORD>((uninstall_value.size() + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS) {
      h.logMessage(L"WriteArpUninstallStrings: failed to set UninstallString: " + utf8::cvt<std::wstring>(error::lookup::last_error(rc)));
    }

    rc = RegSetValueExW(hKey, L"QuietUninstallString", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE *>(quiet_value.c_str()),
                        static_cast<DWORD>((quiet_value.size() + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS) {
      h.logMessage(L"WriteArpUninstallStrings: failed to set QuietUninstallString: " + utf8::cvt<std::wstring>(error::lookup::last_error(rc)));
    }

    RegCloseKey(hKey);
  } catch (installer_exception &e) {
    h.errorMessage(L"WriteArpUninstallStrings: " + e.what());
    return ERROR_SUCCESS;  // Not fatal.
  } catch (...) {
    h.errorMessage(L"WriteArpUninstallStrings: <UNKNOWN EXCEPTION>");
    return ERROR_SUCCESS;
  }
  return ERROR_SUCCESS;
}

extern "C" UINT __stdcall TranslateSid(MSIHANDLE hInstall) {
  TCHAR szSid[MAX_PATH] = {0};
  TCHAR szSidProperty[MAX_PATH] = {0};
  TCHAR szName[MAX_PATH] = {0};
  DWORD size = MAX_PATH;
  UINT ret = 0;
  ret = MsiGetProperty(hInstall, L"TRANSLATE_SID", szSid, &size);

  if (ret != ERROR_SUCCESS) {
    return 4444;
  }

  size = MAX_PATH;
  ret = MsiGetProperty(hInstall, L"TRANSLATE_SID_PROPERTY", szSidProperty, &size);

  if (ret != ERROR_SUCCESS) {
    return 4445;
  }

  PSID pSID = NULL;

  if (!ConvertStringSidToSid(szSid, &pSID)) {
    return 4446;
  }

  size = MAX_PATH;
  TCHAR szRefDomain[MAX_PATH] = {0};
  SID_NAME_USE nameUse;
  DWORD refSize = MAX_PATH;
  if (!LookupAccountSid(NULL, pSID, szName, &size, szRefDomain, &refSize, &nameUse)) {
    if (pSID != NULL) {
      LocalFree(pSID);
    }
    return 4447;
  }

  ret = MsiSetProperty(hInstall, szSidProperty, szName);
  if (!ConvertStringSidToSid(szSid, &pSID)) {
    if (pSID != NULL) {
      LocalFree(pSID);
    }
    return 4448;
  }

  if (pSID != NULL) {
    LocalFree(pSID);
  }
  return ERROR_SUCCESS;
}
