#pragma once

//////////////////////////////////////////////////////////////////////////
// Module wrappers (definitions)
#define NSC_WRAPPERS_MAIN() \
	extern "C" int NSModuleHelperInit(unsigned int id, nscapi::core_api::lpNSAPILoader f); \
	extern "C" int NSLoadModule(); \
	extern "C" int NSLoadModuleEx(unsigned int plugin_id, char* alias, int mode); \
	extern "C" void NSDeleteBuffer(char**buffer); \
	extern "C" int NSGetModuleName(char* buf, int buflen); \
	extern "C" int NSGetModuleDescription(char* buf, int buflen); \
	extern "C" int NSGetModuleVersion(int *major, int *minor, int *revision); \
	extern "C" NSCAPI::boolReturn NSHasCommandHandler(unsigned int plugin_id); \
	extern "C" NSCAPI::boolReturn NSHasMessageHandler(unsigned int plugin_id); \
	extern "C" void NSHandleMessage(unsigned int plugin_id, const char* data, unsigned int len); \
	extern "C" NSCAPI::nagiosReturn NSHandleCommand(unsigned int plugin_id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len); \
	extern "C" int NSUnloadModule(unsigned int plugin_id)


#define NSC_WRAPPERS_CLI() \
	extern "C" int NSCommandLineExec(unsigned int plugin_id, char *request_buffer, unsigned int request_len, char **response_buffer, unsigned int *response_len);

#define NSC_WRAPPERS_CHANNELS() \
	extern "C" int NSHasNotificationHandler(unsigned int plugin_id); \
	extern "C" int NSHandleNotification(unsigned int plugin_id, const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len);

#define NSC_WRAPPERS_ROUTING() \
	extern "C" int NSHasRoutingHandler(unsigned int plugin_id); \
	extern "C" int NSRouteMessage(unsigned int plugin_id, const char*, NSCAPI::nagiosReturn, const char*, unsigned int);

//////////////////////////////////////////////////////////////////////////
// Logging calls for the core wrapper 

#define NSC_LOG_ERROR_EXR(msg, ex) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG("Exception in " + std::string(msg) + ": " + utf8::utf8_from_native(ex.what()), NSCAPI::log_level::error); }
#define NSC_LOG_ERROR_EX(msg) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG("Exception in " + std::string(msg), NSCAPI::log_level::error); }
#define NSC_LOG_ERROR_STD(msg) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG((std::string)msg, NSCAPI::log_level::error); }
#define NSC_LOG_ERROR(msg) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG(msg, NSCAPI::log_level::error); }
#define NSC_LOG_ERROR_WA(msg, ws) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG((std::string)msg + utf8::cvt<std::string>(ws), NSCAPI::log_level::error); }
#define NSC_LOG_ERROR_W(msg) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { NSC_ANY_MSG(utf8::cvt<std::string>(msg), NSCAPI::log_level::error); }
#define NSC_LOG_ERROR_LISTW(lst) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { BOOST_FOREACH(const std::wstring &s, lst) { NSC_ANY_MSG(utf8::cvt<std::string>(s), NSCAPI::log_level::error); } }
#define NSC_LOG_ERROR_LISTS(lst) if (GET_CORE()->should_log(NSCAPI::log_level::error)) { BOOST_FOREACH(const std::string &s, lst) { NSC_ANY_MSG(s, NSCAPI::log_level::error); } }

#define NSC_LOG_CRITICAL_STD(msg) if (GET_CORE()->should_log(NSCAPI::log_level::critical)) { NSC_ANY_MSG((std::string)msg, NSCAPI::log_level::critical); }
#define NSC_LOG_CRITICAL(msg) if (GET_CORE()->should_log(NSCAPI::log_level::critical)) { NSC_ANY_MSG(msg, NSCAPI::log_level::critical); }

#define NSC_LOG_MESSAGE_STD(msg) if (GET_CORE()->should_log(NSCAPI::log_level::info)) { NSC_ANY_MSG((std::string)msg, NSCAPI::log_level::info); }
#define NSC_LOG_MESSAGE(msg) if (GET_CORE()->should_log(NSCAPI::log_level::info)) { NSC_ANY_MSG(msg, NSCAPI::log_level::info); }

#define NSC_DEBUG_MSG_STD(msg) if (GET_CORE()->should_log(NSCAPI::log_level::debug)) { NSC_ANY_MSG((std::string)msg, NSCAPI::log_level::debug); }
#define NSC_DEBUG_MSG(msg) if (GET_CORE()->should_log(NSCAPI::log_level::debug)) { NSC_ANY_MSG(msg, NSCAPI::log_level::debug); }

#define NSC_ANY_MSG(msg, type) GET_CORE()->log(type, __FILE__, __LINE__, msg)

//////////////////////////////////////////////////////////////////////////
// Message wrappers below this point

#ifdef _WIN32
#define NSC_WRAP_DLL() \
	BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID) { return TRUE; } \
	nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();
#else
#define NSC_WRAP_DLL() \
	nscapi::helper_singleton* nscapi::plugin_singleton = new nscapi::helper_singleton();
#endif


#define NSC_WRAPPERS_MAIN_DEF(impl_class, def_alias) \
	typedef impl_class plugin_impl_class; \
	static nscapi::plugin_instance_data<plugin_impl_class> plugin_instance; \
	extern int NSModuleHelperInit(unsigned int, nscapi::core_api::lpNSAPILoader f) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSModuleHelperInit(f); } \
	extern int NSLoadModuleEx(unsigned int id, char* alias, int mode) { \
		nscapi::basic_wrapper_static<plugin_impl_class>::set_alias(def_alias, alias); \
		nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSLoadModuleEx(id, alias, mode); } \
	extern int NSLoadModule() { return nscapi::basic_wrapper_static<plugin_impl_class>::NSLoadModule(); } \
	extern int NSGetModuleName(char* buf, int buflen) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleName(buf, buflen); } \
	extern int NSGetModuleDescription(char* buf, int buflen) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleDescription(buf, buflen); } \
	extern int NSGetModuleVersion(int *major, int *minor, int *revision) { return nscapi::basic_wrapper_static<plugin_impl_class>::NSGetModuleVersion(major, minor, revision); } \
	extern int NSUnloadModule(unsigned int id) { \
		int ret; {nscapi::basic_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		ret = wrapper.NSUnloadModule();} \
		plugin_instance.erase(id); \
		return ret; } \
	extern void NSDeleteBuffer(char**buffer) { nscapi::basic_wrapper_static<plugin_impl_class>::NSDeleteBuffer(buffer); }

#define NSC_WRAPPERS_HANDLE_MSG_DEF() \
	extern void NSHandleMessage(unsigned int id, const char* request_buffer, unsigned int request_buffer_len) { \
		nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHandleMessage(request_buffer, request_buffer_len); } \
	extern NSCAPI::boolReturn NSHasMessageHandler(unsigned int id) { \
		nscapi::message_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHasMessageHandler(); }

#define NSC_WRAPPERS_HANDLE_CMD_DEF() \
	extern NSCAPI::nagiosReturn NSHandleCommand(unsigned int id, const char* request_buffer, const unsigned int request_buffer_len, char** reply_buffer, unsigned int *reply_buffer_len) { \
		nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHandleCommand(request_buffer, request_buffer_len, reply_buffer, reply_buffer_len); } \
	extern NSCAPI::boolReturn NSHasCommandHandler(unsigned int id) { \
		nscapi::command_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHasCommandHandler(); }

#define NSC_WRAPPERS_ROUTING_DEF() \
	extern NSCAPI::nagiosReturn NSRouteMessage(unsigned int id, const char* channel, const char* command, const char* request_buffer, const unsigned int request_buffer_len) { \
		nscapi::routing_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSRouteMessage(channel, command, request_buffer, request_buffer_len); } \
	extern NSCAPI::boolReturn NSHasRoutingHandler(unsigned int id) { \
		nscapi::routing_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHasRoutingHandler(); }

#define NSC_WRAPPERS_HANDLE_NOTIFICATION_DEF() \
	extern int NSHandleNotification(unsigned int id, const char* channel, const char* buffer, unsigned int buffer_len, char** response_buffer, unsigned int *response_buffer_len) { \
		nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHandleNotification(channel, buffer, buffer_len, response_buffer, response_buffer_len); } \
	extern NSCAPI::boolReturn NSHasNotificationHandler(unsigned int id) { \
		nscapi::submission_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSHasNotificationHandler(); }

#define NSC_WRAPPERS_CLI_DEF() \
	extern int NSCommandLineExec(unsigned int id, char *request_buffer, unsigned int request_len, char **response_buffer, unsigned int *response_len) { \
		nscapi::cliexec_wrapper<plugin_impl_class> wrapper(plugin_instance.get(id)); \
		return wrapper.NSCommandLineExec(request_buffer, request_len, response_buffer, response_len); }

#define NSC_WRAPPERS_IGNORE_MSG_DEF() \
	extern void NSHandleMessage(unsigned int, const char*, unsigned int) {} \
	extern NSCAPI::boolReturn NSHasMessageHandler(unsigned int) { return NSCAPI::isfalse; }

#define NSC_WRAPPERS_IGNORE_CMD_DEF() \
	extern NSCAPI::nagiosReturn NSHandleCommand(unsigned int, const char*, const unsigned int, char**, unsigned int*) {  return NSCAPI::returnIgnored; } \
	extern NSCAPI::boolReturn NSHasCommandHandler(unsigned int) { return NSCAPI::isfalse; }

#define NSC_WRAPPERS_IGNORE_NOTIFICATION_DEF() \
	extern int NSHandleNotification(unsigned int, const char*, const char*, const char*, unsigned int) {} \
	extern NSCAPI::boolReturn NSHasNotificationHandler(unsigned int) { return NSCAPI::isfalse; }

#define GET_CORE() nscapi::plugin_singleton->get_core()
