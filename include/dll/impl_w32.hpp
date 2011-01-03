#pragma once
#include <unicode_char.hpp>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>

#include <error.hpp>

namespace dll {
	namespace win32 {
		class impl : public boost::noncopyable {
		private:
			HMODULE handle_;
			boost::filesystem::wpath module_;
		public:
			impl(boost::filesystem::wpath module) : module_(module), handle_(NULL) {
				if (!boost::filesystem::is_regular(module_)) {
					module_ = fix_module_name(module_);
				}
			}
			static boost::filesystem::wpath fix_module_name( boost::filesystem::wpath module ) {
				if (boost::filesystem::is_regular(module))
					return module;
				boost::filesystem::wpath mod = module / std::wstring(_T(".dll"));
				if (boost::filesystem::is_regular(mod))
					return mod;
				return module;
			}

			static bool is_module(std::wstring file) {
				return boost::ends_with(file, _T(".dll"));
			}

			void load_library() {
				if (handle_ != NULL)
					unload_library();
				handle_ = LoadLibrary(module_.file_string().c_str());
				if (handle_ == NULL)
					throw dll_exception(_T("Could not load library: ") + error::lookup::last_error() + _T(": ") + module_.file_string());
			}
			LPVOID load_proc(std::string name) {
				if (handle_ == NULL)
					throw dll_exception(_T("Failed to load process since module is not loaded: ") + module_.file_string());
				LPVOID ep = GetProcAddress(handle_, name.c_str());
				return ep;
			}

			void unload_library() {
				if (handle_ == NULL)
					return;
				FreeLibrary(handle_);
				handle_ = NULL;
			}
			bool is_loaded() const { return handle_ != NULL; }
			boost::filesystem::wpath get_file() const { return module_; }
			std::wstring get_filename() const { return module_.leaf(); }
			std::wstring get_module_name() {
				std::wstring ext = _T(".dll");
				int l = ext.length();
				std::wstring fn = get_filename();
				if ((fn.length() > l) && (fn.substr(fn.size()-l) == ext))
					return fn.substr(0, fn.size()-l);
				return fn;
			}
		};
	}
}


