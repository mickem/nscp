#pragma once

#include <unicode_char.hpp>
#include <boost/noncopyable.hpp>
#include <error.hpp>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(AIX)
#include <dlfcn.h>
#elif defined(LINUX) || defined(SUN) || defined(CYGWIN)
#include <dlfcn.h>
#elif defined(HP)
#include <dl.h>
#endif

namespace dll {
	namespace iunix {
		class impl : public boost::noncopyable {
		private:
			boost::filesystem::wpath module_;
			void* handle_;

		public:
			impl(boost::filesystem::wpath module) : module_(module), handle_(NULL) {
				if (!boost::filesystem::is_regular(module)) {
					module_ = fix_module_name(module_);
				}
			}
			boost::filesystem::wpath fix_module_name( boost::filesystem::wpath module_ ) {
				boost::filesystem::wpath mod = module_ / get_extension();
				if (boost::filesystem::is_regular(mod))
					return mod;
				return module_;
			}
			static std::wstring get_extension() {
#if defined(CYGWIN)
				return _T(".so");
#elif defined(HP)
				return _T(".so");
#else
				return _T(".so");
#endif
			}

			static bool is_module(std::wstring file) {
				return boost::ends_with(file, get_extension());
			}

			void load_library() {
				std::string dllname = to_string(module_.string());
#if defined(LINUX) || defined(SUN) || defined(AIX) || defined(CYGWIN)
				handle_ = dlopen(dllname.c_str(), RTLD_NOW);
				if (handle_ == NULL)
					throw dll_exception(_T("Could not load library: ") + to_wstring(dlerror()) + _T(": ") + module_.string());
#elif defined(HP)
				handle_ = shl_load(dllname.c_str(), BIND_DEFERRED|DYNAMIC_PATH, 0L);
				if (handle_ == NULL)
					throw dll_exception(_T("Could not load library: ") + error::lookup::last_error() + _T(": ") + module_.string());
#else
				/* This type of UNIX has no DLL support yet */
				throw dll_exception(_T("Unsupported Unix flavour (please report this): ") + module_.string());
#endif
			}
			void* load_proc(std::string name) {
				if (handle_ == NULL)
					throw dll_exception(_T("Failed to load process from module: ") + module_.string());
				void *ep = NULL;
#if defined(LINUX) || defined(SUN) || defined(AIX) || defined(CYGWIN)
				ep = (void*) dlsym(handle_, name.c_str());
				return ep;
#elif defined(HP)
				int rcode = shl_findsym((shl_t)&handle_, name.c_str(), TYPE_PROCEDURE, &ep);
				if (rcode == -1)
					return NULL;
					//throw dll_exception(_T("Failed to load process from module: ") + module_.string());
				return ep;
#else
				/* This type of UNIX has no DLL support yet */
				throw dll_exception(_T("Unsupported Unix flavour (please report this): ") + module_.string());
#endif
			}

			void unload_library() {
#if defined(LINUX) || defined(SUN) || defined(AIX) || defined(CYGWIN)
				dlclose(handle_);
#elif defined(HP)
				shl_unload(handle_);
#else
				/* This type of UNIX has no DLL support yet */
				throw dll_exception(_T("Unsupported Unix flavour (please report this): ") + module_.string());
#endif

			}

			bool is_loaded() const { return handle_!=NULL; }
			std::wstring get_file() const { return module_.string(); }
		};
	}
}

