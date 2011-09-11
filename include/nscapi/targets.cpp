#include <nscapi/targets.hpp>

namespace sh = nscapi::settings_helper;


nscapi::target_handler::target nscapi::target_handler::read_target(boost::shared_ptr<nscapi::settings_proxy> proxy, std::wstring path, std::wstring alias, std::wstring host) {
	target t;
	t.alias = alias;
	t.host = host;
	std::wstring tpath = path + _T("/") + alias;

	sh::settings_registry settings(proxy);

	settings.path(path).add_path()
		(alias, sh::wstring_map_path(&t.options), 
		_T("TARGET DEFENITION"), _T("Target defnition for: ") + alias)

		;

	settings.path(tpath).add_key()
		(_T("protocol"), sh::wstring_key(&t.protocol, _T("*")),
		_T("TARGET PROTOCOL"), _T("The default protocol to use for target"))

		(_T("host"), sh::wstring_key(&t.host, t.host),
		_T("TARGET HOST"), _T("Target host ip address"))

		(_T("alias"), sh::wstring_key(&t.alias, t.alias),
		_T("TARGET ALIAS"), _T("The alias for the target"))

		(_T("parent"), sh::wstring_key(&t.parent, _T("default")),
		_T("TARGET PARENT"), _T("The parent the target inherits from"))

		;

	settings.register_all();
	settings.notify();

	if (!t.parent.empty() && t.parent != alias & t.parent != t.alias) {
		target p;
		optarget tmp = find_target(t.parent);
		if (tmp)
			p = *tmp;
		else {
			p = read_target(proxy, path, t.parent, _T(""));
			add_template(p);
		}
		apply_parent(t, p);
	}
	return t;
}

nscapi::target_handler::optarget nscapi::target_handler::find_target(std::wstring alias) {
	target_list_type::const_iterator cit = target_list.find(alias);
	if (cit != target_list.end())
		return optarget(cit->second);
	cit = template_list.find(alias);
	if (cit != template_list.end())
		return optarget(cit->second);
	return optarget();
}

void nscapi::target_handler::apply_parent(target &t, target &p) {
	if (t.host.empty())
		t.host = p.host;
	if (t.host.empty())
		t.host = p.host;
	BOOST_FOREACH(target::options_type::value_type i, p.options) {
		if (t.options.find(i.first) != t.options.end())
			t.options[i.first] = i.second;
	}
}

std::wstring nscapi::target_handler::to_wstring() {
	std::wstringstream ss;
	ss << _T("Targets: ");
	BOOST_FOREACH(target_list_type::value_type t, target_list) {
		ss << _T(", ") << t.first << _T(" = {") << t.second.to_wstring() + _T("} ");
	}
	ss << _T("Templates: ");
	BOOST_FOREACH(target_list_type::value_type t, target_list) {
		ss << _T(", ") << t.first << _T(" = {") << t.second.to_wstring() + _T("} ");
	}
	return ss.str();
}
