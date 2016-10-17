#include <nscapi/nscapi_targets.hpp>
#include <boost/filesystem.hpp>
/*
void nscapi::targets::helpers::verify_file(nscapi::targets::target_object &target, std::string key, std::list<std::string> &errors) {
	if (!target.has_option(key))
		return;
	std::string value = target.options[key];
	if (value == "none" || value == "")
		return;
	boost::filesystem::path p = value;
	if (!boost::filesystem::is_regular(p))
		errors.push_back("File not found '" + key + "': " + p.string());
}
*/