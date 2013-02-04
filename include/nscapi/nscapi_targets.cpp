#include <nscapi/nscapi_targets.hpp>
#include <boost/filesystem.hpp>
#include <utf8.hpp>



void nscapi::targets::helpers::verify_file(nscapi::targets::target_object &target, std::wstring key, std::list<std::wstring> &errors) {
	if (!target.has_option(key))
		return;
	std::wstring value = target.options[key];
	if (value == _T("none") || value == _T(""))
		return;
	boost::filesystem::path p = utf8::cvt<std::string>(value);
	if (!boost::filesystem::is_regular(p))
		errors.push_back(_T("File not found '") + key + _T("': ") + utf8::cvt<std::wstring>(p.string()));
}
