#pragma once

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
			boost::filesystem::path module_;
			void* handle_;

		public:
			impl(boost::filesystem::path module) : module_(module), handle_(NULL) {
				if (!boost::filesystem::is_regular(module)) {
					module_ = fix_module_name(module_);
				}
			}
			static boost::filesystem::path fix_module_name( boost::filesystem::path module ) {

				if (boost::filesystem::is_regular(module))
					return module;
				/* this one (below) is wrong I think */
				boost::filesystem::path mod = module / get_extension();
				if (boost::filesystem::is_regular(mod))
					return mod;
				mod = boost::filesystem::path(module.string() + get_extension());
				if (boost::filesystem::is_regular(mod))
					return mod;
				mod = mod.branch_path() / boost::filesystem::path(std::string("lib") + mod.leaf());
				if (boost::filesystem::is_regular(mod))
					return mod;
				return module;
			}
			static std::string get_extension() {
#if defined(CYGWIN)
				return ".so";
#elif defined(HP)
				return ".so";
#else
				return ".so";
#endif
			}

			static bool is_module(std::string file) {
				return boost::ends_with(file, get_extension());
			}

			void load_library() {
				std::string dllname = to_string(module_.string());
#if defined(LINUX) || defined(SUN) || defined(AIX) || defined(CYGWIN)
				handle_ = dlopen(dllname.c_str(), RTLD_NOW);
				if (handle_ == NULL)
					throw dll_exception(std::string("Could not load library: ") + dlerror() + ": " + module_.string());
#elif defined(HP)
				handle_ = shl_load(dllname.c_str(), BIND_DEFERRED|DYNAMIC_PATH, 0L);
				if (handle_ == NULL)
					throw dll_exception("Could not load library: " + module_.string());
#else
				/* This type of UNIX has no DLL support yet */
				throw dll_exception("Unsupported Unix flavour (please report this): " + module_.string());
#endif
			}
			void* load_proc(std::string name) {
				if (handle_ == NULL)
					throw dll_exception("Failed to load process from module: " + module_.string());
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
				throw dll_exception("Unsupported Unix flavour (please report this): " + module_.string());
#endif
			}

			void unload_library() {
#if defined(LINUX) || defined(SUN) || defined(AIX) || defined(CYGWIN)
				dlclose(handle_);
#elif defined(HP)
				shl_unload(handle_);
#else
				/* This type of UNIX has no DLL support yet */
				throw dll_exception("Unsupported Unix flavour (please report this): " + module_.string());
#endif

			}

			bool is_loaded() const { return handle_!=NULL; }
			boost::filesystem::path get_file() const { return module_; }
			std::string get_filename() const { return module_.leaf(); }
			std::string get_module_name() {
				std::string ext = get_extension();
				std::size_t l = ext.length();
				std::string fn = get_filename();
				if ((fn.length() > l) && (fn.substr(fn.size()-l) == ext))
					return fn.substr(0, fn.size()-l);
				return fn;
			}
		};
	}
}

