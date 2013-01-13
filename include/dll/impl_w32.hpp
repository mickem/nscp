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
			boost::filesystem::path module_;
		public:
			impl(boost::filesystem::path module) : module_(module), handle_(NULL) {
				if (!boost::filesystem::is_regular(module_)) {
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
				mod = boost::filesystem::path(module.wstring() + get_extension());
				if (boost::filesystem::is_regular(mod))
					return mod;
				return module;
			}

			static std::wstring get_extension() {
				return _T(".dll");
			}


			static bool is_module(std::wstring file) {
				return boost::ends_with(file, get_extension());
			}

			void load_library() {
				if (handle_ != NULL)
					unload_library();
				handle_ = LoadLibrary(module_.filename().wstring().c_str());
				if (handle_ == NULL)
					throw dll_exception(_T("Could not load library: ") + error::lookup::last_error() + _T(": ") + module_.filename().wstring());
			}
			LPVOID load_proc(std::string name) {
				if (handle_ == NULL)
					throw dll_exception(_T("Failed to load process since module is not loaded: ") + module_.filename().wstring());
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
			boost::filesystem::path get_file() const { return module_; }
			std::wstring get_filename() const { return module_.filename().wstring(); }
			std::wstring get_module_name() {
				std::wstring ext = _T(".dll");
				std::wstring::size_type l = ext.length();
				std::wstring fn = get_filename();
				if ((fn.length() > l) && (fn.substr(fn.size()-l) == ext))
					return fn.substr(0, fn.size()-l);
				return fn;
			}
		};
	}
}


