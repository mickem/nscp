// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <nscapi/nscapi_core_wrapper.hpp>
#include <nscapi/nscapi_helper.hpp>
#include <nsclient/nsclient_exception.hpp>

#define CORE_LOG_ERROR(msg)                                 \
  if (should_log(NSCAPI::log_level::error)) {               \
    log(NSCAPI::log_level::error, __FILE__, __LINE__, msg); \
  }

#define LEGACY_BUFFER_LENGTH 4096

namespace nscapi {
class core_wrapper_impl {
 public:
  std::string alias;  // This is actually the wrong value if multiple modules are loaded!
};
}  // namespace nscapi

nscapi::core_wrapper::core_wrapper()
    : pimpl(new core_wrapper_impl()),
      fNSAPIGetApplicationName(nullptr),
      fNSAPIGetApplicationVersionStr(nullptr),
      fNSAPIMessage(nullptr),
      fNSAPISimpleMessage(nullptr),
      fNSAPIInject(nullptr),
      fNSAPIExecCommand(nullptr),
      fNSAPIDestroyBuffer(nullptr),
      fNSAPINotify(nullptr),
      fNSAPIReload(nullptr),
      fNSAPICheckLogMessages(nullptr),
      fNSAPISettingsQuery(nullptr),
      fNSAPIExpandPath(nullptr),
      fNSAPIGetLoglevel(nullptr),
      fNSAPIRegistryQuery(nullptr),
      fNSCAPIEmitEvent(nullptr),
      fNSAPIStorageQuery(nullptr),
      fNSAPISetTag(nullptr),
      fNSAPIGetTags(nullptr),
      fNSAPISetLogOption(nullptr) {}
nscapi::core_wrapper::~core_wrapper() { delete pimpl; }

//////////////////////////////////////////////////////////////////////////
// Callbacks into the core
//////////////////////////////////////////////////////////////////////////

bool nscapi::core_wrapper::should_log(NSCAPI::nagiosReturn msgType) const {
  enum log_status { unknown, set };
  static NSCAPI::log_level::level level = NSCAPI::log_level::info;
  static log_status status = unknown;
  if (status == unknown) {
    level = get_loglevel();
    status = set;
  }
  return logging::matches(level, msgType);
}

/**
 * Callback to send a message through to the core
 *
 * @param msgType Message type (debug, warning, etc.)
 * @param file File where message was generated (__FILE__)
 * @param line Line where message was generated (__LINE__)
 * @param message Message in human readable format
 */
void nscapi::core_wrapper::log(std::string message) const {
  if (!fNSAPIMessage) {
    return;
  }
  try {
    return fNSAPIMessage(message.c_str(), static_cast<unsigned int>(message.size()));
  } catch (...) {
  }
}

/**
 * Callback to send a message through to the core
 *
 * @param loglevel Message type (debug, warning, etc.)
 * @param file File where message was generated (__FILE__)
 * @param line Line where message was generated (__LINE__)
 * @param logMessage Message in human readable format
 * @throws nsclient::nsclient_exception When core pointer set is unavailable.
 */
void nscapi::core_wrapper::log(NSCAPI::log_level::level loglevel, std::string file, int line, std::string logMessage) const {
  if (!should_log(loglevel)) return;
  if (!fNSAPISimpleMessage) {
    return;
  }
  try {
    return fNSAPISimpleMessage(pimpl->alias.c_str(), loglevel, file.c_str(), line, logMessage.c_str());
  } catch (...) {
  }
}

NSCAPI::log_level::level nscapi::core_wrapper::get_loglevel() const {
  if (!fNSAPIGetLoglevel) {
    return NSCAPI::log_level::debug;
  }
  return fNSAPIGetLoglevel();
}

/**
 * Inject a request command in the core (this will then be sent to the plug-in stack for processing)
 * @param command Command to inject
 * @param argLen The length of the argument buffer
 * @param **argument The argument buffer
 * @param *returnMessageBuffer Buffer to hold the returned message
 * @param returnMessageBufferLen Length of returnMessageBuffer
 * @param *returnPerfBuffer Buffer to hold the returned performance data
 * @param returnPerfBufferLen returnPerfBuffer
 * @return The returned status of the command
 */
NSCAPI::nagiosReturn nscapi::core_wrapper::query(const char *request, const unsigned int request_len, char **response, unsigned int *response_len) const {
  if (!fNSAPIInject) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPIInject(request, request_len, response, response_len);
}

NSCAPI::errorReturn nscapi::core_wrapper::emit_event(std::string &request) const {
  return emit_event(request.c_str(), static_cast<unsigned int>(request.size()));
}
NSCAPI::errorReturn nscapi::core_wrapper::emit_event(const char *request, const unsigned int request_len) const {
  if (!fNSCAPIEmitEvent) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSCAPIEmitEvent(request, request_len);
}

void nscapi::core_wrapper::DestroyBuffer(char **buffer) const {
  if (!fNSAPIDestroyBuffer) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPIDestroyBuffer(buffer);
}

bool nscapi::core_wrapper::submit_message(std::string channel, std::string request, std::string &response) const {
  if (!fNSAPINotify) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool ret = NSCAPI::api_ok(submit_message(channel.c_str(), request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));

  if (buffer_size > 0 && buffer != nullptr) {
    response = std::string(buffer, buffer_size);
  }

  DestroyBuffer(&buffer);
  return ret;
}

bool nscapi::core_wrapper::reload(std::string module) const {
  if (!fNSAPIReload) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return NSCAPI::api_ok(fNSAPIReload(module.c_str()));
}
NSCAPI::nagiosReturn nscapi::core_wrapper::submit_message(const char *channel, const char *request, const unsigned int request_len, char **response,
                                                          unsigned int *response_len) const {
  if (!fNSAPINotify) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPINotify(channel, request, request_len, response, response_len);
}

bool nscapi::core_wrapper::query(const std::string &request, std::string &result) const {
  if (!fNSAPIInject) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool retC = NSCAPI::api_ok(query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));

  if (buffer_size > 0 && buffer != nullptr) {
    // PluginCommand::ResponseMessage rsp_msg;
    result = std::string(buffer, buffer_size);
  }

  DestroyBuffer(&buffer);
  if (!retC) {
    CORE_LOG_ERROR("Failed to execute query");
  }
  return retC;
}

bool nscapi::core_wrapper::exec_command(const std::string target, std::string request, std::string &result) const {
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool retC = NSCAPI::api_ok(exec_command(target.c_str(), request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));

  if (buffer_size > 0 && buffer != nullptr) {
    result = std::string(buffer, buffer_size);
  }

  DestroyBuffer(&buffer);
  if (!retC) {
    CORE_LOG_ERROR("Failed to execute command on " + target);
  }
  return retC;
}
NSCAPI::nagiosReturn nscapi::core_wrapper::exec_command(const char *target, const char *request, const unsigned int request_len, char **response,
                                                        unsigned int *response_len) const {
  if (!fNSAPIExecCommand) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPIExecCommand(target, request, request_len, response, response_len);
}

std::string nscapi::core_wrapper::expand_path(std::string value) const {
  if (!fNSAPIExpandPath) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  unsigned int buf_len = LEGACY_BUFFER_LENGTH;
  char *buffer = new char[buf_len + 1];
  if (!NSCAPI::api_ok(fNSAPIExpandPath(value.c_str(), buffer, buf_len))) {
    delete[] buffer;
    throw nsclient::nsclient_exception("Failed to expand path: " + value);
  }
  std::string ret = buffer;
  delete[] buffer;
  return ret;
}
NSCAPI::errorReturn nscapi::core_wrapper::settings_query(const char *request, const unsigned int request_len, char **response,
                                                         unsigned int *response_len) const {
  if (!fNSAPISettingsQuery) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPISettingsQuery(request, request_len, response, response_len);
}
bool nscapi::core_wrapper::settings_query(const std::string request, std::string &response) const {
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool retC = NSCAPI::api_ok(settings_query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));
  if (buffer_size > 0 && buffer != nullptr) {
    response = std::string(buffer, buffer_size);
  }
  DestroyBuffer(&buffer);
  return retC;
}

NSCAPI::errorReturn nscapi::core_wrapper::registry_query(const char *request, const unsigned int request_len, char **response,
                                                         unsigned int *response_len) const {
  if (!fNSAPIRegistryQuery) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPIRegistryQuery(request, request_len, response, response_len);
}
bool nscapi::core_wrapper::registry_query(const std::string request, std::string &response) const {
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool retC = NSCAPI::api_ok(registry_query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));
  if (buffer_size > 0 && buffer != nullptr) {
    response = std::string(buffer, buffer_size);
  }
  DestroyBuffer(&buffer);
  return retC;
}

NSCAPI::errorReturn nscapi::core_wrapper::storage_query(const char *request, const unsigned int request_len, char **response,
                                                        unsigned int *response_len) const {
  if (!fNSAPIStorageQuery) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return fNSAPIStorageQuery(request, request_len, response, response_len);
}
bool nscapi::core_wrapper::storage_query(const std::string request, std::string &response) const {
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  bool retC = NSCAPI::api_ok(storage_query(request.c_str(), static_cast<unsigned int>(request.size()), &buffer, &buffer_size));
  if (buffer_size > 0 && buffer != nullptr) {
    response = std::string(buffer, buffer_size);
  }
  DestroyBuffer(&buffer);
  return retC;
}

bool nscapi::core_wrapper::set_log_option(const std::string &option) const {
  if (!fNSAPISetLogOption) return false;
  return NSCAPI::api_ok(fNSAPISetLogOption(option.c_str()));
}

bool nscapi::core_wrapper::set_tag(const std::string &key, const std::string &value) const {
  // Degrade gracefully on cores without the tag API (loaded as nullptr).
  if (!fNSAPISetTag) return false;
  return NSCAPI::api_ok(fNSAPISetTag(key.c_str(), value.c_str()));
}

std::string nscapi::core_wrapper::get_tags_json() const {
  if (!fNSAPIGetTags) return "{}";
  char *buffer = nullptr;
  unsigned int buffer_size = 0;
  const bool retC = NSCAPI::api_ok(fNSAPIGetTags(&buffer, &buffer_size));
  std::string response;
  if (buffer_size > 0 && buffer != nullptr) {
    response = std::string(buffer, buffer_size);
  }
  DestroyBuffer(&buffer);
  if (!retC || response.empty()) return "{}";
  return response;
}

namespace {
// Skip JSON insignificant whitespace.
void tags_skip_ws(const std::string &s, std::size_t &i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
}

// Parse one JSON string starting at s[i]=='"' into out, resolving standard
// escapes. Returns false (and leaves the map caller to bail) on a malformed or
// unterminated string. \uXXXX is decoded to UTF-8; the core only \u-escapes
// control characters, so surrogate pairs do not occur here.
bool tags_parse_string(const std::string &s, std::size_t &i, std::string &out) {
  if (i >= s.size() || s[i] != '"') return false;
  ++i;
  while (i < s.size()) {
    const char c = s[i++];
    if (c == '"') return true;
    if (c != '\\') {
      out.push_back(c);
      continue;
    }
    if (i >= s.size()) return false;
    const char e = s[i++];
    switch (e) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      case 'u': {
        if (i + 4 > s.size()) return false;
        unsigned int cp = 0;
        for (int k = 0; k < 4; ++k) {
          const char h = s[i++];
          cp <<= 4;
          if (h >= '0' && h <= '9')
            cp |= static_cast<unsigned int>(h - '0');
          else if (h >= 'a' && h <= 'f')
            cp |= static_cast<unsigned int>(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F')
            cp |= static_cast<unsigned int>(h - 'A' + 10);
          else
            return false;
        }
        if (cp < 0x80) {
          out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
          out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
          out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
          out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
          out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
          out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        break;
      }
      default:
        return false;
    }
  }
  return false;  // unterminated
}
}  // namespace

std::map<std::string, std::string> nscapi::core_wrapper::parse_tags_json(const std::string &json) {
  std::map<std::string, std::string> result;
  std::size_t i = 0;
  tags_skip_ws(json, i);
  if (i >= json.size() || json[i] != '{') return result;
  ++i;
  tags_skip_ws(json, i);
  if (i < json.size() && json[i] == '}') return result;  // {}
  while (i < json.size()) {
    tags_skip_ws(json, i);
    std::string key;
    if (!tags_parse_string(json, i, key)) return result;
    tags_skip_ws(json, i);
    if (i >= json.size() || json[i] != ':') return result;
    ++i;
    tags_skip_ws(json, i);
    std::string value;
    if (!tags_parse_string(json, i, value)) return result;
    result[key] = value;
    tags_skip_ws(json, i);
    if (i >= json.size()) return result;
    if (json[i] == ',') {
      ++i;
      continue;
    }
    return result;  // '}' or anything else: stop, returning what we have
  }
  return result;
}

std::map<std::string, std::string> nscapi::core_wrapper::get_tags() const { return parse_tags_json(get_tags_json()); }

/**
 * Retrieve the application name (in human readable format) from the core.
 * @return A string representing the application name.
 * @throws nsclient::nsclient_exception When core pointer set is unavailable or an unexpected error occurs.
 */
std::string nscapi::core_wrapper::getApplicationName() const {
  if (!fNSAPIGetApplicationName) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  unsigned int buf_len = LEGACY_BUFFER_LENGTH;
  char *buffer = new char[buf_len + 1];
  if (!NSCAPI::api_ok(fNSAPIGetApplicationName(buffer, buf_len))) {
    delete[] buffer;
    throw nsclient::nsclient_exception("Application name could not be retrieved");
  }
  std::string ret = buffer;
  delete[] buffer;
  return ret;
}

bool nscapi::core_wrapper::checkLogMessages(int type) {
  if (!fNSAPICheckLogMessages) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  return NSCAPI::api_ok(fNSAPICheckLogMessages(type));
}
/**
 * Retrieve the application version as a string (in human readable format) from the core.
 * @return A string representing the application version.
 * @throws nsclient::nsclient_exception When core pointer set is unavailable.
 */
std::string nscapi::core_wrapper::getApplicationVersionString() const {
  if (!fNSAPIGetApplicationVersionStr) throw nsclient::nsclient_exception("NSCore has not been initiated...");
  unsigned int buf_len = LEGACY_BUFFER_LENGTH;
  char *buffer = new char[buf_len + 1];
  if (!NSCAPI::api_ok(fNSAPIGetApplicationVersionStr(buffer, buf_len))) {
    delete[] buffer;
    return "";
  }
  std::string ret = buffer;
  delete[] buffer;
  return ret;
}

void nscapi::core_wrapper::set_alias(const std::string default_alias_, const std::string alias_) { pimpl->alias = default_alias_; }

/**
 * Wrapper function around the ModuleHelperInit call.
 * This wrapper retrieves all pointers and stores them for future use.
 * @param f A function pointer to a function that can be used to load function from the core.
 * @return NSCAPI::success or NSCAPI::failure
 */
bool nscapi::core_wrapper::load_endpoints(core_api::lpNSAPILoader f) {
  fNSAPIGetApplicationName = reinterpret_cast<core_api::lpNSAPIGetApplicationName>(f("NSAPIGetApplicationName"));
  fNSAPIGetApplicationVersionStr = reinterpret_cast<core_api::lpNSAPIGetApplicationVersionStr>(f("NSAPIGetApplicationVersionStr"));
  fNSAPIMessage = reinterpret_cast<core_api::lpNSAPIMessage>(f("NSAPIMessage"));
  fNSAPISimpleMessage = reinterpret_cast<core_api::lpNSAPISimpleMessage>(f("NSAPISimpleMessage"));
  fNSAPIInject = reinterpret_cast<core_api::lpNSAPIInject>(f("NSAPIInject"));
  fNSAPIExecCommand = reinterpret_cast<core_api::lpNSAPIExecCommand>(f("NSAPIExecCommand"));
  fNSAPIDestroyBuffer = reinterpret_cast<core_api::lpNSAPIDestroyBuffer>(f("NSAPIDestroyBuffer"));
  fNSAPINotify = reinterpret_cast<core_api::lpNSAPINotify>(f("NSAPINotify"));
  fNSAPICheckLogMessages = reinterpret_cast<core_api::lpNSAPICheckLogMessages>(f("NSAPICheckLogMessages"));
  fNSAPIReload = reinterpret_cast<core_api::lpNSAPIReload>(f("NSAPIReload"));

  fNSAPISettingsQuery = reinterpret_cast<core_api::lpNSAPISettingsQuery>(f("NSAPISettingsQuery"));
  fNSAPIRegistryQuery = reinterpret_cast<core_api::lpNSAPIRegistryQuery>(f("NSAPIRegistryQuery"));
  fNSAPIExpandPath = reinterpret_cast<core_api::lpNSAPIExpandPath>(f("NSAPIExpandPath"));

  fNSAPIGetLoglevel = reinterpret_cast<core_api::lpNSAPIGetLoglevel>(f("NSAPIGetLoglevel"));

  fNSCAPIEmitEvent = reinterpret_cast<core_api::lpNSCAPIEmitEvent>(f("NSCAPIEmitEvent"));
  fNSAPIStorageQuery = reinterpret_cast<core_api::lpNSAPIStorageQuery>(f("NSAPIStorageQuery"));

  fNSAPISetTag = reinterpret_cast<core_api::lpNSAPISetTag>(f("NSAPISetTag"));
  fNSAPIGetTags = reinterpret_cast<core_api::lpNSAPIGetTags>(f("NSAPIGetTags"));
  fNSAPISetLogOption = reinterpret_cast<core_api::lpNSAPISetLogOption>(f("NSAPISetLogOption"));

  return true;
}