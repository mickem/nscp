#pragma once

#include <unicode_char.hpp>
#include <boost/noncopyable.hpp>
#include <error.hpp>
namespace dll {
	namespace iunix {

		class impl : public boost::noncopyable {
		private:
			std::wstring module_;
		public:
			impl(std::wstring module) : module_(module) {}

			void load_library() {
				throw dll_exception(_T("Could not load library: ") + module_);
			}
			void* load_proc(std::string name) {
				throw dll_exception(_T("Failed to load process from module: ") + module_);
			}

			void unload_library() {
			}

			bool is_loaded() const { return false; }
			std::wstring get_file() const { return module_; }
		};
	}
}

