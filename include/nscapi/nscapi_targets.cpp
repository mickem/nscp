#include <nscapi/nscapi_targets.hpp>
#include <boost/filesystem.hpp>



void nscapi::targets::helpers::verify_file(nscapi::targets::target_object &target, std::wstring key, std::list<std::wstring> &errors) {
	if (!target.has_option(key))
		return;
	std::wstring value = target.options[key];
	if (value == _T("none") || value == _T(""))
		return;
	boost::filesystem::wpath p = value;
	if (!boost::filesystem::is_regular(p))
		errors.push_back(_T("File not found '") + key + _T("': ") + p.string());
}
