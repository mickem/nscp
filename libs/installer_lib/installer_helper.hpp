#pragma once

#include <error/error.hpp>
#include <char_buffer.hpp>
#include <str/wstring.hpp>
#include <utf8.hpp>

#include <msi.h>
#include <MsiQuery.h>
#include <windows.h>

#include <vector>

class installer_exception {
	std::wstring error_;
public:
	installer_exception(std::wstring error) : error_(error) {}

	std::wstring what() const { return error_; }
};

#define MY_EMPTY L"$EMPTY$"
#define KEY_DEF L"_DEFAULT"
class msi_helper {
	MSIHANDLE hInstall_;
	std::wstring action_;
	PMSIHANDLE hProgressPos;
	PMSIHANDLE hActionRec;
	PMSIHANDLE hDatabase;
public:
	msi_helper(MSIHANDLE hInstall, std::wstring action) : hInstall_(hInstall), hProgressPos(NULL), hActionRec(NULL), action_(action) {
		logMessage(L"installer_lib::" + action);
	}
	~msi_helper() {
		MsiCloseHandle(hInstall_);
		hInstall_ = NULL;
	}

	static std::wstring last_werror(int err = -1) {
		return utf8::cvt<std::wstring>(error::lookup::last_error(err));
	}

	std::wstring getTargetPath(std::wstring path) {
		wchar_t tmpBuf[MAX_PATH];
		DWORD len = 0;
		if (MsiGetTargetPath(hInstall_, path.c_str(), tmpBuf, &len) != ERROR_MORE_DATA)
			throw installer_exception(L"Failed to get size for target path '" + path + L"': " + last_werror());
		len++;
		hlp::tchar_buffer buffer(len);
		if (MsiGetTargetPath(hInstall_, path.c_str(), buffer, &len) != ERROR_SUCCESS) {
			throw installer_exception(L"Failed to get target path '" + path + L"': " + last_werror());
		}
		std::wstring value = buffer;
		return value;
	}

	inline std::wstring trim_right(const std::wstring &source, const std::wstring& t = L" ") {
		std::wstring str = source;
		return str.erase(str.find_last_not_of(t) + 1);
	}

	inline std::wstring trim_left(const std::wstring& source, const std::wstring& t = L" ") {
		std::wstring str = source;
		return str.erase(0, source.find_first_not_of(t));
	}

	inline std::wstring trim(const std::wstring& source, const std::wstring& t = L" ") {
		std::wstring str = source;
		return trim_left(trim_right(str, t), t);
	}
	bool propertyNotDefault(std::wstring path) {
		std::wstring old = getPropery(path + KEY_DEF);
		std::wstring cur = getPropery(path);
		return old != cur;
	}
	std::wstring getPropery(std::wstring path) {
		wchar_t tmpBuf[MAX_PATH];
		DWORD len = 0;
		if (MsiGetProperty(hInstall_, path.c_str(), tmpBuf, &len) != ERROR_MORE_DATA)
			throw installer_exception(L"Failed to get size for property '" + path + L"': " + last_werror());
		len++;
		hlp::tchar_buffer buffer(len);
		if (MsiGetProperty(hInstall_, path.c_str(), buffer, &len) != ERROR_SUCCESS) {
			throw installer_exception(L"Failed to get property '" + path + L"': " + last_werror());
		}
		std::wstring value = buffer;
		return value;
	}
	hlp::tchar_buffer getProperyRAW(std::wstring path) {
		wchar_t emptyString[MAX_PATH];
		DWORD len = 0;
		UINT er;
		if ((er = MsiGetProperty(hInstall_, path.c_str(), emptyString, &len)) != ERROR_MORE_DATA)
			throw installer_exception(L"Failed to get size for property '" + path + L"': " + last_werror(er));
		len += 2;
		hlp::tchar_buffer buffer(len);
		if ((er = MsiGetProperty(hInstall_, path.c_str(), buffer, &len)) != ERROR_SUCCESS) {
			throw installer_exception(L"Failed to get property '" + path + L"': " + last_werror(er));
		}
		return buffer;
	}
	void setFeatureLocal(std::wstring feature) {
		MsiSetFeatureState(hInstall_, feature.c_str(), INSTALLSTATE_LOCAL);
	}
	void setFeatureAbsent(std::wstring feature) {
		MsiSetFeatureState(hInstall_, feature.c_str(), INSTALLSTATE_ABSENT);
	}
	void setPropertyIfEmpty(std::wstring key, std::wstring val) {
		std::wstring old = getPropery(key);
		if (old.empty()) {
			logMessage(L"Setting (empty) " + key + L" to " + val + L" if empty");
			setProperty(key, val);
		} else {
			logMessage(L"Not setting (not empty) " + key + L" to " + val + L" if empty");
		}
	}
	void setPropertyAndDefault(std::wstring key, std::wstring value) {
		logMessage(L"Setting " + key + L"=" + value);
		logMessage(L"Setting " + key + KEY_DEF + L"=" + value);
		MsiSetProperty(hInstall_, key.c_str(), L"");
		MsiSetProperty(hInstall_, key.c_str(), value.c_str());
		MsiSetProperty(hInstall_, (key + KEY_DEF).c_str(), L"");
		MsiSetProperty(hInstall_, (key + KEY_DEF).c_str(), value.c_str());
	}
	void setPropertyAndDefault(std::wstring key, std::wstring value, std::wstring old_value) {
		logMessage(L"Setting " + key + L"=" + value);
		logMessage(L"Setting " + key + KEY_DEF + L"=" + old_value);
		MsiSetProperty(hInstall_, key.c_str(), value.c_str());
		MsiSetProperty(hInstall_, (key + KEY_DEF).c_str(), old_value.c_str());
	}
	void setPropertyAndDefaultBool(std::wstring key, bool value) {
		std::wstring v = value ? L"1" : L"";
		setPropertyAndDefault(key, v);
	}
	void setProperty(std::wstring key, std::wstring value) {
		std::wstring old = getPropery(key);
		logMessage(L"Setting " + key + L"=" + value + L" previous value was: " + old);
		MsiSetProperty(hInstall_, key.c_str(), value.c_str());
	}
	void applyProperty(std::wstring key, std::wstring value_key) {
		std::wstring current_value = getPropery(key);
		std::wstring applied_value = getPropery(value_key);
		logMessage(L"Applying value from " + value_key + L" to " + key);
		if (!applied_value.empty()) {
			logMessage(L"  + " + key + L" goes from " + current_value + L" to " + applied_value);
			setProperty(key, applied_value);
		} else {
			logMessage(L"  + " + key + L" unchanged as overriden value was empty: keeping " + current_value + L" over " + applied_value);
		}
	}

	void dumpReason(std::wstring desc) {
		logMessage(L" : NSCP Dumping properties: " + desc);

	}
	void dumpProperty(std::wstring key) {
		std::wstring value = getPropery(key);
		logMessage(L" : +++ " + key + L"=" + value);
	}


	MSIHANDLE createSimpleString(std::wstring msg) {
		MSIHANDLE hRecord = ::MsiCreateRecord(1);
		::MsiRecordSetString(hRecord, 1, msg.c_str());
		return hRecord;
	}
	MSIHANDLE createSimpleString(std::string msg) {
		MSIHANDLE hRecord = ::MsiCreateRecord(1);
		::MsiRecordSetString(hRecord, 1, utf8::cvt<std::wstring>(msg).c_str());
		return hRecord;
	}
	MSIHANDLE create3Int(int i1, int i2, int i3) {
		MSIHANDLE hRecord = ::MsiCreateRecord(1);
		MsiRecordSetInteger(hRecord, 1, i1);
		MsiRecordSetInteger(hRecord, 2, i2);
		MsiRecordSetInteger(hRecord, 3, i3);
		return hRecord;
	}
	MSIHANDLE create4Int(int i1, int i2, int i3, int i4) {
		MSIHANDLE hRecord = ::MsiCreateRecord(1);
		MsiRecordSetInteger(hRecord, 1, i1);
		MsiRecordSetInteger(hRecord, 2, i2);
		MsiRecordSetInteger(hRecord, 3, i3);
		MsiRecordSetInteger(hRecord, 4, i4);
		return hRecord;
	}
	void set3Int(MSIHANDLE hRecord, int i1, int i2, int i3) {
		MsiRecordSetInteger(hRecord, 1, i1);
		MsiRecordSetInteger(hRecord, 2, i2);
		MsiRecordSetInteger(hRecord, 3, i3);
	}
	void errorMessage(std::wstring msg) {
		PMSIHANDLE hRecord = createSimpleString(msg);
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_ERROR | MB_OK | MB_ICONERROR), hRecord);
	}
	void logMessage(std::wstring msg) {
		PMSIHANDLE hRecord = createSimpleString(msg);
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_INFO), hRecord);
	}
	void logMessage(std::string msg) {
		PMSIHANDLE hRecord = createSimpleString(msg);
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_INFO), hRecord);
	}

	std::wstring getTempPath() {
		return getPropery(L"TempFolder");
	}

	MSIHANDLE getActiveDatabase() {
		if (isNull(hDatabase))
			hDatabase = ::MsiGetActiveDatabase(hInstall_); // may return null if deferred CustomAction
		return hDatabase;
	}

	inline bool isNull(MSIHANDLE h) {
		return h == NULL;
	}

	bool table_exists(std::wstring table) {
		UINT er = ERROR_SUCCESS;

		// NOTE:  The following line of commented out code should work in a
		//        CustomAction but does not in Windows Installer v1.1
		// er = ::MsiDatabaseIsTablePersistentW(hDatabase, wzTable);

		// a "most elegant" workaround a Darwin v1.1 bug
		PMSIHANDLE hRec;
		er = ::MsiDatabaseGetPrimaryKeysW(getActiveDatabase(), table.c_str(), &hRec);

		if (ERROR_SUCCESS == er)
			return true;
		else if (ERROR_INVALID_TABLE == er)
			return false;
		else
			return false;
		return true;
	}

	/********************************************************************
	WcaOpenExecuteView() - opens and executes a view on the installing database
	********************************************************************/
	MSIHANDLE open_execute_view(__in_z LPCWSTR wzSql) {
		MSIHANDLE phView;
		if (!wzSql || !*wzSql)
			throw installer_exception(L"Invalid arguments!");

		UINT er = ::MsiDatabaseOpenViewW(getActiveDatabase(), wzSql, &phView);
		if (er != ERROR_SUCCESS)
			throw installer_exception(L"failed to open view on database: " + last_werror(er));
		er = ::MsiViewExecute(phView, NULL);
		if (er != ERROR_SUCCESS) {
			MsiCloseHandle(phView);
			throw installer_exception(L"failed to open view on database: " + last_werror(er));
		}
		return phView;
	}

	/********************************************************************
	WcaFetchRecord() - gets the next record from a view on the installing database
	********************************************************************/
	MSIHANDLE fetch_record(__in MSIHANDLE hView) {
		MSIHANDLE phRec;
		if (!hView)
			throw installer_exception(L"Invalid arguments!");

		UINT er = ::MsiViewFetch(hView, &phRec);
		if (er == ERROR_NO_MORE_ITEMS)
			return NULL;
		if (er != ERROR_SUCCESS)
			throw installer_exception(L"failed to return rows from database: " + last_werror(er));
		return phRec;
	}

	/********************************************************************
	WcaGetRecordString() - gets a string field out of a record
	********************************************************************/
	std::wstring get_record_string(__in MSIHANDLE hRec, __in UINT uiField) {
		if (!hRec)
			throw installer_exception(L"Invalid arguments!");

		UINT er;
		DWORD_PTR cch = 0;

		WCHAR szEmpty[1] = L"";
		er = ::MsiRecordGetStringW(hRec, uiField, szEmpty, (DWORD*)&cch);
		if (ERROR_MORE_DATA != er && ERROR_SUCCESS != er) {
			throw installer_exception(L"get_record_string:: Failed to get length of string: " + last_werror(er));
		}
		hlp::tchar_buffer buffer(++cch);

		er = ::MsiRecordGetStringW(hRec, uiField, buffer, (DWORD*)&cch);
		if (er != ERROR_SUCCESS)
			if (ERROR_MORE_DATA == er) {
				throw installer_exception(L"get_record_string:: Failed to get length of string: " + last_werror(er));
			}
		std::wstring string = buffer;
		return string;
	}

	/********************************************************************
WcaGetRecordString() - gets a string field out of a record
********************************************************************/
	std::wstring get_record_blob(__in MSIHANDLE hRec, __in UINT uiField) {
		if (!hRec)
			throw installer_exception(L"Invalid arguments!");

		UINT er;

		unsigned int size = MsiRecordDataSize(hRec, uiField);
		hlp::char_buffer buffer(size + 5);

		er = ::MsiRecordReadStream(hRec, uiField, buffer, (DWORD*)&size);
		if (er != ERROR_SUCCESS)
			if (ERROR_MORE_DATA == er) {
				throw installer_exception(L"get_record_string:: Failed to get length of string: " + last_werror(er));
			}
		std::string string = buffer;
		return utf8::cvt<std::wstring>(string);
	}

	/********************************************************************
	HideNulls() - internal helper function to escape [~] in formatted strings
	********************************************************************/
	void _hide_nulls(hlp::tchar_buffer wzData) {
		LPWSTR pwz = wzData;
		while (*pwz) {
			if (pwz[0] == L'[' && pwz[1] == L'~' && pwz[2] == L']') // found a null [~]
			{
				pwz[0] = L'!'; // turn it into !$!
				pwz[1] = L'$';
				pwz[2] = L'!';
				pwz += 3;
			} else
				pwz++;
		}
	}
	/********************************************************************
	RevealNulls() - internal helper function to unescape !$! in formatted strings
	********************************************************************/
	void _reveal_nulls(hlp::tchar_buffer wzData) {
		LPWSTR pwz = wzData;
		while (*pwz) {
			if (pwz[0] == L'!' && pwz[1] == L'$' && pwz[2] == L'!') // found the fake null !$!
			{
				pwz[0] = L'['; // turn it back into [~]
				pwz[1] = L'~';
				pwz[2] = L']';
				pwz += 3;
			} else
				pwz++;
		}
	}

	/********************************************************************
	WcaGetRecordInteger() - gets an integer field out of a record
	********************************************************************/
	int get_record_integer(MSIHANDLE hRec, UINT uiField) {
		if (!hRec)
			throw installer_exception(L"Invalid arguments!");
		return ::MsiRecordGetInteger(hRec, uiField);
	}

	/********************************************************************
	WcaSetRecordString() - set a string field in record
	********************************************************************/
	void set_record_string(__in MSIHANDLE hRec, __in UINT uiField, __in_z LPCWSTR wzData) {
		if (!hRec || !wzData)
			throw installer_exception(L"Invalid arguments!");

		UINT er = ::MsiRecordSetStringW(hRec, uiField, wzData);
		if (er != ERROR_SUCCESS)
			throw installer_exception(L"Failed to set filed as string!");
	}

	/********************************************************************
	WcaGetRecordFormattedString() - gets formatted string filed from record
	********************************************************************/
	std::wstring get_record_formatted_string(__in MSIHANDLE hRec, __in UINT uiField) {
		if (!hRec)
			throw installer_exception(L"Invalid arguments!");

		UINT er;

		// get the format string
		hlp::tchar_buffer tmp = get_record_string(hRec, uiField);
		std::wstring t = tmp;
		if (t.length() == 0)
			return L" ";

		// hide the nulls '[~]' so we can get them back after formatting
		//TODO: _hide_nulls(tmp);

		// set up the format record
		PMSIHANDLE hRecFormat = ::MsiCreateRecord(1);
		if (hRecFormat == NULL)
			throw installer_exception(L"Record was NULL!");
		set_record_string(hRecFormat, 0, tmp);

		WCHAR szEmpty[1] = L"";
		DWORD_PTR cch = 0;
		er = ::MsiFormatRecordW(hInstall_, hRecFormat, szEmpty, (DWORD*)&cch);
		if (ERROR_MORE_DATA != er && ERROR_SUCCESS != er) {
			throw installer_exception(L"get_record_formatted_string:: Failed to get length of string: " + last_werror(er));
		}
		hlp::tchar_buffer buffer(++cch);

		er = ::MsiFormatRecordW(hInstall_, hRecFormat, buffer, (DWORD*)&cch);
		if (er != ERROR_SUCCESS) {
			throw installer_exception(L"get_record_formatted_string:: Failed to format string: " + last_werror(er));
		}
		//logMessage(L"FMT Record: '" + std::wstring(buffer) + L"'");
		// put the nulls back
		//TODO: _reveal_nulls(buffer);
		std::wstring str = buffer;
		return str;
	}

	/********************************************************************
	WcaGetRecordFormattedInteger() - gets formatted integer from record
	********************************************************************/
	int get_record_formatted_integer(MSIHANDLE hRec, UINT uiField) {
		if (!hRec)
			throw installer_exception(L"Invalid arguments!");
		std::wstring str = get_record_formatted_string(hRec, uiField);
		if (str.length() > 0)
			return strEx::stox<int>(str);
		else
			return MSI_NULL_INTEGER;
	}

	enum WCA_TODO {
		WCA_TODO_UNKNOWN,
		WCA_TODO_INSTALL,
		WCA_TODO_UNINSTALL,
		WCA_TODO_REINSTALL,
	};

	/********************************************************************
	WcaGetComponentToDo() - gets a component's install states and
	determines if they mean install, uninstall, or reinstall.
	********************************************************************/
	WCA_TODO get_component_todo(std::wstring wzComponentId) {
		INSTALLSTATE isInstalled = INSTALLSTATE_UNKNOWN;
		INSTALLSTATE isAction = INSTALLSTATE_UNKNOWN;
		UINT er = ::MsiGetComponentStateW(hInstall_, wzComponentId.c_str(), &isInstalled, &isAction);
		if (ERROR_SUCCESS != er) {
			logMessage(L"State for : " + wzComponentId + L" was unknown due to: " + last_werror(er));
			return WCA_TODO_UNKNOWN;
		}

		if (WcaIsReInstalling(isInstalled, isAction)) {
			return WCA_TODO_REINSTALL;
		} else if (WcaIsUninstalling(isInstalled, isAction)) {
			return WCA_TODO_UNINSTALL;
		} else if (WcaIsInstalling(isInstalled, isAction)) {
			return WCA_TODO_INSTALL;
		} else {
			logMessage(L"State for : " + wzComponentId + L" was unknown due to; isInstalled: " + strEx::xtos(isInstalled) + L", isAction: " + strEx::xtos(isAction));
			return WCA_TODO_UNKNOWN;
		}
	}
	bool is_installed(std::wstring wzComponentId) {
		INSTALLSTATE isInstalled = INSTALLSTATE_UNKNOWN;
		INSTALLSTATE isAction = INSTALLSTATE_UNKNOWN;
		UINT er = ::MsiGetComponentStateW(hInstall_, wzComponentId.c_str(), &isInstalled, &isAction);
		if (ERROR_SUCCESS != er) {
			logMessage(L"State for : " + wzComponentId + L" was unknown due to: " + last_werror(er));
			return WCA_TODO_UNKNOWN;
		}
		return (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled);
	}
	/********************************************************************
	WcaIsInstalling() - determines if a pair of installstates means install
	********************************************************************/
	bool WcaIsInstalling(INSTALLSTATE isInstalled, INSTALLSTATE isAction) {
		return (INSTALLSTATE_LOCAL == isAction || INSTALLSTATE_SOURCE == isAction || (INSTALLSTATE_DEFAULT == isAction && (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled)));
	}

	/********************************************************************
	WcaIsReInstalling() - determines if a pair of installstates means reinstall
	********************************************************************/
	bool WcaIsReInstalling(INSTALLSTATE isInstalled, INSTALLSTATE isAction) {
		return ((INSTALLSTATE_LOCAL == isAction || INSTALLSTATE_SOURCE == isAction || INSTALLSTATE_DEFAULT == isAction) && (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled));
	}

	/********************************************************************
	WcaIsUninstalling() - determines if a pair of installstates means uninstall
	********************************************************************/
	BOOL WcaIsUninstalling(INSTALLSTATE isInstalled, INSTALLSTATE isAction) {
		// TODO: Added || INSTALLSTATE_UNKNOWN == isAction since it seems broken (but this should not be required!!!)
		return ((INSTALLSTATE_ABSENT == isAction || INSTALLSTATE_REMOVED == isAction || INSTALLSTATE_UNKNOWN == isAction) && (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled));
	}

	/********************************************************************
	StrMaxLength - returns maximum number of characters that can be stored in dynamic string p
	NOTE:  assumes Unicode string
	********************************************************************/
	HRESULT __StrMaxLength(LPVOID p, DWORD_PTR* pcch) {
		if (p) {
			*pcch = ::HeapSize(::GetProcessHeap(), 0, p); // get size of entire buffer
			if (-1 == *pcch)
				return E_FAIL;
			*pcch /= sizeof(WCHAR);   // reduce to count of characters
		} else
			*pcch = 0;
		return S_OK;
	}

	static const WCHAR MAGIC_MULTISZ_DELIM = 128;

	class custom_action_data_r {
	private:
		unsigned int position_;
		typedef std::vector<std::wstring> list_t;
		list_t list_;

	public:
		custom_action_data_r(std::wstring data) : position_(0) {
			std::list<std::wstring> list;
			std::wstring::size_type pos = 0;
			for (unsigned int i = 0; i < data.length(); i++) {
				if (data[i] == MAGIC_MULTISZ_DELIM) {
					list_.push_back(data.substr(pos, i - pos));
					pos = i + 1;
				}
			}
			if (pos < data.length())
				list_.push_back(data.substr(pos));
		}
		~custom_action_data_r() {}

		bool has_more() {
			return position_ < list_.size();
		}
		std::wstring get_next_string() {
			if (!has_more())
				return L" ";
			return list_[position_++];
		}
		unsigned int get_next_int() {
			return strEx::stox<unsigned int>(get_next_string());
		}
		std::list<std::wstring> get_next_list() {
			std::list<std::wstring> list;
			unsigned int count = get_next_int();
			for (unsigned int i = 0; i < count; ++i) {
				list.push_back(get_next_string());
			}
			return list;
		}

		std::wstring to_string() {
			std::wstring str;
			for (list_t::const_iterator cit = list_.begin(); cit != list_.end(); ++cit) {
				str += (*cit) + L"|";
			}
			return str;
		}
	};

	class custom_action_data_w {
	private:
		std::wstring buf_;
	public:
		custom_action_data_w() {}
		~custom_action_data_w() {}

		void insert_string(std::wstring str) {
			WCHAR delim[] = { MAGIC_MULTISZ_DELIM, 0 }; // magic char followed by NULL terminator
			if (!buf_.empty())
				buf_ = str + delim + buf_;
			else
				buf_ = str;
		}
		void insert_int(int i) {
			insert_string(strEx::xtos(i));
		}
		void write_string(std::wstring str) {
			WCHAR delim[] = { MAGIC_MULTISZ_DELIM, 0 }; // magic char followed by NULL terminator
			if (!buf_.empty())
				buf_ += delim;
			buf_ += str;
		}
		void write_int(int i) {
			write_string(strEx::xtos(i));
		}
		void write_list(std::list<std::wstring> list) {
			write_int(list.size());
			for (std::list<std::wstring>::const_iterator cit = list.begin(); cit != list.end(); ++cit) {
				write_string(*cit);
			}
		}
		std::wstring to_string() const {
			return buf_; //return std::wstring(data_);
		}
		bool has_data(const int size = 0) const {
			return used_size() > size;
		}
		operator const wchar_t* () const {
			return buf_.c_str();
		}
		std::size_t used_size() const {
			return buf_.length();
		}
	};

	/********************************************************************
	WcaDoDeferredAction() - schedules an action at this point in the script
	********************************************************************/
	HRESULT do_deferred_action(LPCWSTR wzAction, LPCWSTR wzCustomActionData, UINT uiCost) {
		HRESULT hr = S_OK;
		UINT er;

		if (wzCustomActionData && *wzCustomActionData) {
			er = ::MsiSetPropertyW(hInstall_, wzAction, wzCustomActionData);
			if (er != ERROR_SUCCESS)
				throw installer_exception(L"Failed to set CustomActionData for deferred action" + last_werror(er));
		}
		/*
				if (0 < uiCost) {
					hr = WcaProgressMessage(uiCost, TRUE);  // add ticks to the progress bar
					// TODO: handle the return codes correctly
				}
		*/
		er = ::MsiDoActionW(hInstall_, wzAction);
		if (ERROR_INSTALL_USEREXIT == er)
			return er;
		else if (er != ERROR_SUCCESS)
			throw installer_exception(L"Failed MsiDoAction on deferred action" + last_werror(er));
		return S_OK;
	}
	/*
	std::wstring get_target_for_file(std::wstring file) {
		// turn that into the path to the target file
		hr = StrAllocFormatted(&pwzFormattedFile, L"[#%s]", pwzFileId);
		ExitOnFailure1(hr, "failed to format file string for file: %S", pwzFileId);
		hr = WcaGetFormattedString(pwzFormattedFile, &pwzGamePath);
		ExitOnFailure1(hr, "failed to get formatted string for file: %S", pwzFileId);

		// and then get the directory part of the path
		hr = PathGetDirectory(pwzGamePath, &pwzGameDir);
		ExitOnFailure1(hr, "failed to get path for file: %S", pwzGamePath);

		// get component and its install/action states
		hr = WcaGetRecordString(hRec, egqComponent, &pwzComponentId);
		ExitOnFailure(hr, "failed to get game component id");
	}
	*/

	std::list<std::wstring> enumProducts() {
		WCHAR buffer[40];
		DWORD id = 0;
		std::list<std::wstring> ret;
		for (int i = 0; ::MsiEnumProducts(i, reinterpret_cast<wchar_t*>(&buffer)) == ERROR_SUCCESS; i++) {
			std::wstring name = getProductName(buffer);
			ret.push_back(buffer);
		}
		return ret;
	}
	std::wstring getProductName(std::wstring code) {
		DWORD size = 0;
		MsiGetProductInfo(code.c_str(), INSTALLPROPERTY_INSTALLEDPRODUCTNAME, NULL, &size);
		size++;
		wchar_t *buffer = new wchar_t[size + 4];
		MsiGetProductInfo(code.c_str(), INSTALLPROPERTY_INSTALLEDPRODUCTNAME, buffer, &size);
		std::wstring ret = buffer;
		delete[] buffer;
		return ret;
	}
};