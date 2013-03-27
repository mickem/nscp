#pragma once

#include <unicode_char.hpp>

#define DEFINE_SETTING_S(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const std::wstring name ## _DEFAULT = _T(value);
#define NSCLIENT_SETTINGS_SYSTRAY_EXE _T("systray_exe")
#define NSCLIENT_SETTINGS_SYSTRAY_EXE_DEFAULT _T("nstray.exe")

#define DEFINE_PATH(name, path) \
	const std::wstring name ## _PATH = _T(path);

#define DEFINE_SETTING_I(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const long long name ## _DEFAULT = value;

#define DEFINE_SETTING_B(name, path, key, value) \
	const std::wstring name ## _PATH = _T(path); \
	const std::wstring name = _T(key); \
	const bool name ## _DEFAULT = value;

#define DESCRIBE_SETTING(name, title, description) \
	const std::wstring name ## _TITLE = _T(title); \
	const std::wstring name ## _DESC = _T(description); \
	const bool name ## _ADVANCED = false;

#define DESCRIBE_SETTING_ADVANCED(name, title, description) \
	const std::wstring name ## _TITLE = _T(title); \
	const std::wstring name ## _DESC = _T(description); \
	const bool name ## _ADVANCED = true;

#define SETTINGS_KEY(key) \
	setting_keys::key ## _PATH, setting_keys::key, setting_keys::key ## _DEFAULT

#define SETTINGS_REG_KEY_S_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT, setting_keys::key ## _ADVANCED
#define SETTINGS_REG_KEY_I_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, boost::lexical_cast<std::wstring>(setting_keys::key ## _DEFAULT), setting_keys::key ## _ADVANCED
#define SETTINGS_REG_KEY_B_GEN(key, type) \
	setting_keys::key ## _PATH, setting_keys::key, type, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _DEFAULT==1?_T("1"):_T("0"), setting_keys::key ## _ADVANCED
#define SETTINGS_REG_PATH_GEN(key) \
	setting_keys::key ## _PATH, setting_keys::key ## _TITLE, setting_keys::key ## _DESC, setting_keys::key ## _ADVANCED

#define GENERIC_KEY_ALLOWED_HOSTS "allowed hosts"
#define GENERIC_KEY_BIND_TO "bind to"
#define GENERIC_KEY_SOCK_READ_TIMEOUT "socket read timeout"
#define GENERIC_KEY_SOCK_LISTENQUE "socket queue size"
#define GENERIC_KEY_SOCK_CACHE_ALLOWED "allowed hosts caching"
#define GENERIC_KEY_PWD_MASTER_KEY "master key"
#define GENERIC_KEY_PWD "password"
#define GENERIC_KEY_OBFUSCATED_PWD "obfuscated password"
#define GENERIC_KEY_USE_SSL "use ssl"


// Main Registry ROOT
#define NS_HKEY_ROOT HKEY_LOCAL_MACHINE
#define NS_REG_ROOT _T("SOFTWARE\\NSClient++")

namespace setting_keys {
#define MAIN_MODULES_SECTION "/modules"
#define CHECK_SYSTEM_SECTION "/settings/system"
#define CHECK_SYSTEM_COUNTERS_SECTION "/settings/system/PDH counters"
}
