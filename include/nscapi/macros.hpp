/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

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

#define NSC_DEBUG_MSG_STD(msg) if (GET_CORE()->should_log(NSCAPI::log_level::debug)) { NSC_ANY_MSG((std::string)msg, NSCAPI::log_level::debug); }
#define NSC_TRACE_MSG(msg) if (GET_CORE()->should_log(NSCAPI::log_level::trace)) { NSC_ANY_MSG(msg, NSCAPI::log_level::trace); }
#define NSC_TRACE_ENABLED() if (GET_CORE()->should_log(NSCAPI::log_level::trace))

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

#define GET_CORE() nscapi::plugin_singleton->get_core()