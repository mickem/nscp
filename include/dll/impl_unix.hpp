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

#undef BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>

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
			static boost::filesystem::wpath fix_module_name( boost::filesystem::wpath module ) {

				if (boost::filesystem::is_regular(module))
					return module;
				/* this one (below) is wrong I think */
				boost::filesystem::wpath mod = module / get_extension();
				if (boost::filesystem::is_regular(mod))
					return mod;
				mod = boost::filesystem::wpath(module.string() + get_extension());
				if (boost::filesystem::is_regular(mod))
					return mod;
				mod = mod.branch_path() / boost::filesystem::wpath(std::wstring(_T("lib") + mod.leaf()));
				if (boost::filesystem::is_regular(mod))
					return mod;
				return module;
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
			boost::filesystem::wpath get_file() const { return module_; }
			std::wstring get_filename() const { return module_.leaf(); }
			std::wstring get_module_name() {
				std::wstring ext = get_extension();
				int l = ext.length();
				std::wstring fn = get_filename();
				if ((fn.length() > l) && (fn.substr(fn.size()-l) == ext))
					return fn.substr(0, fn.size()-l);
				return fn;
			}
		};
	}
}

