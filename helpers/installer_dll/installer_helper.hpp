#pragma once

#include <windows.h>
#include <msi.h>
#include <error.hpp>
#include <char_buffer.hpp>
#include <strsafe.h>

class installer_exception {
	std::wstring error_;
public:
	installer_exception(std::wstring error) : error_(error) {}

	std::wstring what() const { return error_; }
};

#define MY_EMPTY _T("$EMPTY$")
class msi_helper {
	MSIHANDLE hInstall_;
	std::wstring action_;
	PMSIHANDLE hProgressPos;
	PMSIHANDLE hActionRec;
	PMSIHANDLE hDatabase;
public:
	msi_helper(MSIHANDLE hInstall, std::wstring action) : hInstall_(hInstall), hProgressPos(NULL), hActionRec(NULL), action_(action) {}
	~msi_helper() {
		MsiCloseHandle(hInstall_);
		hInstall_ = NULL;
	}

	std::wstring getTargetPath(std::wstring path) {
		TCHAR tmpBuf[MAX_PATH];
		DWORD len = 0;
		if (MsiGetTargetPath(hInstall_ ,path.c_str(), tmpBuf, &len) != ERROR_MORE_DATA)
			throw installer_exception(_T("Failed to get size for target path '") + path + _T("': ") + error::lookup::last_error());
		len++;
		char_buffer buffer(len);
		if (MsiGetTargetPath(hInstall_ ,path.c_str(), buffer, &len) != ERROR_SUCCESS) {
			throw installer_exception(_T("Failed to get target path '") + path + _T("': ") + error::lookup::last_error());
		}
		std::wstring value = buffer;
		return value;
	}
	bool isChangedProperyAndOld(std::wstring str) {
		std::wstring cval = strEx::trim(getPropery(str));
		std::wstring oval = strEx::trim(getPropery(str + _T("_OLD")));
		logMessage(_T("Comparing property: ") + str + _T("; ") + cval + _T("=?=") + oval);
		if (oval == MY_EMPTY)
			oval = _T("");
		return cval != oval;
	}

	std::wstring getPropery(std::wstring path) {
		TCHAR tmpBuf[MAX_PATH];
		DWORD len = 0;
		if (MsiGetProperty(hInstall_ ,path.c_str(), tmpBuf, &len) != ERROR_MORE_DATA)
			throw installer_exception(_T("Failed to get size for property '") + path + _T("': ") + error::lookup::last_error());
		len++;
		char_buffer buffer(len);
		if (MsiGetProperty(hInstall_ ,path.c_str(), buffer, &len) != ERROR_SUCCESS) {
			throw installer_exception(_T("Failed to get property '") + path + _T("': ") + error::lookup::last_error());
		}
		std::wstring value = buffer;
		return value;
	}
	char_buffer getProperyRAW(std::wstring path) {
		TCHAR emptyString[MAX_PATH];
		DWORD len = 0;
		UINT er;
		if ((er = MsiGetProperty(hInstall_ ,path.c_str(), emptyString, &len)) != ERROR_MORE_DATA)
			throw installer_exception(_T("Failed to get size for property '") + path + _T("': ") + error::format::from_system(er));
		len+=2;
		char_buffer buffer(len);
		if ((er = MsiGetProperty(hInstall_ ,path.c_str(), buffer, &len)) != ERROR_SUCCESS) {
			throw installer_exception(_T("Failed to get property '") + path + _T("': ") + error::format::from_system(er));
		}
		return buffer;
	}
/*
	void setPropertyAndOld(std::wstring key, std::wstring value) {
		MsiSetProperty (hInstall_, key.c_str(), value.c_str());
		MsiSetProperty (hInstall_, (key+_T("_OLD")).c_str(), value.c_str());
	}
	*/
	void setupMyProperty(std::wstring key, std::wstring val) {
		std::wstring oldDef = getPropery(key+_T("_DEFAULT"));
		if (!oldDef.empty())
			val = oldDef;
		else
			setProperty(key+_T("_DEFAULT"), val);
		setPropertyIfEmpty(key+_T("_OLD"), val);
		if (val == MY_EMPTY)
			val = _T("");
		setPropertyIfEmpty(key, val);
	}
	void setMyProperty(std::wstring key, std::wstring val) {
		setProperty(key, val);
	}
	
	void setPropertyIfEmpty(std::wstring key, std::wstring val) {
		std::wstring old = getPropery(key);
		if (old.empty())
			setProperty(key, val);
	}
	void setPropertyAndOld(std::wstring key, std::wstring value, std::wstring oldvalue) {
		MsiSetProperty (hInstall_, key.c_str(), value.c_str());
		std::wstring old_key = key+_T("_OLD");
		if (getPropery(old_key).empty())
			setProperty(old_key, oldvalue);
		//MsiSetProperty (hInstall_, (key+_T("_OLD")).c_str(), oldvalue.c_str());
	}
	void setPropertyOld(std::wstring key, std::wstring oldvalue) {
		MsiSetProperty (hInstall_, (key+_T("_OLD")).c_str(), oldvalue.c_str());
	}
	void setProperty(std::wstring key, std::wstring value) {
		MsiSetProperty (hInstall_, key.c_str(), value.c_str());
	}
	MSIHANDLE createSimpleString(std::wstring msg) {
		MSIHANDLE hRecord = ::MsiCreateRecord(1);
		::MsiRecordSetString(hRecord, 1, msg.c_str());
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
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_ERROR|MB_OK|MB_ICONERROR), hRecord);
	}
	void logMessage(std::wstring msg) {
		PMSIHANDLE hRecord = createSimpleString(msg);
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_INFO), hRecord);
	}
	void startProgress(int step, int max, std::wstring description, std::wstring tpl) {
		hActionRec = ::MsiCreateRecord(3);
		MsiRecordSetString(hActionRec, 1, action_.c_str());
		MsiRecordSetString(hActionRec, 2, description.c_str());
		MsiRecordSetString(hActionRec, 3, tpl.c_str());
		UINT iResult = MsiProcessMessage(hInstall_, INSTALLMESSAGE_ACTIONSTART, hActionRec);
		if ((iResult == IDCANCEL))
			throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());


		PMSIHANDLE hStart = create4Int(0, max, 0, 1);
		iResult = MsiProcessMessage(hInstall_, INSTALLMESSAGE_PROGRESS, hStart);
		if ((iResult == IDCANCEL))
			throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());

		hProgressPos = create3Int(1,1,0);
		iResult = MsiProcessMessage(hInstall_, INSTALLMESSAGE_PROGRESS, hProgressPos);
		if ((iResult == IDCANCEL))
			throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());

		set3Int(hProgressPos, 2, step, 0);
	}

	std::wstring getTempPath() {
		return getPropery(_T("TempFolder"));
	}

	void updateProgress(std::wstring str1, std::wstring str2) {
		MsiRecordSetString(hActionRec, 1, action_.c_str());
		MsiRecordSetString(hActionRec, 2, str1.c_str());
		MsiRecordSetString(hActionRec, 3, str2.c_str());
		if (MsiProcessMessage(hInstall_, INSTALLMESSAGE_ACTIONSTART, hActionRec) == IDCANCEL)
			throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());
		/*
		if (MsiProcessMessage(hInstall_, INSTALLMESSAGE_ACTIONDATA, hActionRec) == IDCANCEL)
		throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());
		*/
		if (MsiProcessMessage(hInstall_, INSTALLMESSAGE_PROGRESS, hProgressPos) == IDCANCEL)
			throw installer_exception(_T("Failed to update progressbar: ") + error::lookup::last_error());
	}
	MSIHANDLE getActiveDatabase() {
		if (isNull(hDatabase))
			hDatabase = ::MsiGetActiveDatabase(hInstall_); // may return null if deferred CustomAction
		return hDatabase;

	}

	inline boolean isNull(MSIHANDLE h) {
		return h==NULL;
	}


	bool table_exists(std::wstring table) {
		HRESULT hr = S_OK;
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
	MSIHANDLE open_execute_view( __in_z LPCWSTR wzSql)
	{
		MSIHANDLE phView;
		if (!wzSql || !*wzSql)
			throw installer_exception(_T("Invalid arguments!"));

		UINT er = ::MsiDatabaseOpenViewW(getActiveDatabase(), wzSql, &phView);
		if (er != ERROR_SUCCESS)
			throw installer_exception(_T("failed to open view on database: ") + error::format::from_system(er));
		er = ::MsiViewExecute(phView, NULL);
		if (er != ERROR_SUCCESS) {
			MsiCloseHandle(phView);
			throw installer_exception(_T("failed to open view on database: ") + error::format::from_system(er));
		}
		return phView;
	}

	/********************************************************************
	WcaFetchRecord() - gets the next record from a view on the installing database
	********************************************************************/
	MSIHANDLE fetch_record(__in MSIHANDLE hView)
	{
		MSIHANDLE phRec;
		if (!hView)
			throw installer_exception(_T("Invalid arguments!"));

		UINT er = ::MsiViewFetch(hView, &phRec);
		if (er == ERROR_NO_MORE_ITEMS)
			return NULL;
		if (er != ERROR_SUCCESS) 
			throw installer_exception(_T("failed to return rows from database: ") + error::format::from_system(er));
		return phRec;
	}



	/********************************************************************
	WcaGetRecordString() - gets a string field out of a record
	********************************************************************/
	std::wstring get_record_string(__in MSIHANDLE hRec, __in UINT uiField)
	{
		if (!hRec)
			throw installer_exception(_T("Invalid arguments!"));
		char_buffer buffer;

		HRESULT hr = S_OK;
		UINT er;
		DWORD_PTR cch = 0;

		WCHAR szEmpty[1] = L"";
		er = ::MsiRecordGetStringW(hRec, uiField, szEmpty, (DWORD*)&cch);
		if (ERROR_MORE_DATA != er && ERROR_SUCCESS != er) {
			throw installer_exception(_T("get_record_string:: Failed to get length of string: ") + error::format::from_system(er));
		}
		buffer.realloc(++cch);

		er = ::MsiRecordGetStringW(hRec, uiField, buffer, (DWORD*)&cch);
		if (er != ERROR_SUCCESS)
		if (ERROR_MORE_DATA == er){
			throw installer_exception(_T("get_record_string:: Failed to get length of string: ") + error::format::from_system(er));
		}
		std::wstring string = buffer;
		return string;
	}

	/********************************************************************
	HideNulls() - internal helper function to escape [~] in formatted strings
	********************************************************************/
	void _hide_nulls(char_buffer wzData) {
		LPWSTR pwz = wzData;
		while(*pwz) {
			if (pwz[0] == L'[' && pwz[1] == L'~' && pwz[2] == L']') // found a null [~] 
			{
				pwz[0] = L'!'; // turn it into !$!
				pwz[1] = L'$';
				pwz[2] = L'!';
				pwz += 3;
			}
			else
				pwz++;
		}
	}
	/********************************************************************
	RevealNulls() - internal helper function to unescape !$! in formatted strings
	********************************************************************/
	void _reveal_nulls(char_buffer wzData) {
		LPWSTR pwz = wzData;
		while(*pwz)	{
			if (pwz[0] == L'!' && pwz[1] == L'$' && pwz[2] == L'!') // found the fake null !$!
			{
				pwz[0] = L'['; // turn it back into [~]
				pwz[1] = L'~';
				pwz[2] = L']';
				pwz += 3;
			}
			else
				pwz++;
		}
	}

	/********************************************************************
	WcaGetRecordInteger() - gets an integer field out of a record
	********************************************************************/
	int get_record_integer(MSIHANDLE hRec, UINT uiField) {
		if (!hRec)
			throw installer_exception(_T("Invalid arguments!"));
		return ::MsiRecordGetInteger(hRec, uiField);
	}


	/********************************************************************
	WcaSetRecordString() - set a string field in record
	********************************************************************/
	void set_record_string( __in MSIHANDLE hRec, __in UINT uiField, __in_z LPCWSTR wzData ) {
		if (!hRec || !wzData)
			throw installer_exception(_T("Invalid arguments!"));

		HRESULT hr = S_OK;
		UINT er = ::MsiRecordSetStringW(hRec, uiField, wzData);
		if (er != ERROR_SUCCESS)
			throw installer_exception(_T("Failed to set filed as string!"));
	}



	/********************************************************************
	WcaGetRecordFormattedString() - gets formatted string filed from record
	********************************************************************/
	std::wstring get_record_formatted_string( __in MSIHANDLE hRec, __in UINT uiField) {
		if (!hRec)
			throw installer_exception(_T("Invalid arguments!"));

		HRESULT hr = S_OK;
		UINT er;

		// get the format string
		char_buffer tmp = get_record_string(hRec, uiField);
		std::wstring t = tmp;
		if (t.length() == 0)
			return _T("");

		// hide the nulls '[~]' so we can get them back after formatting
		//TODO: _hide_nulls(tmp);

		// set up the format record
		PMSIHANDLE hRecFormat = ::MsiCreateRecord(1);
		if (hRecFormat == NULL)
			throw installer_exception(_T("Record was NULL!"));
		set_record_string(hRecFormat, 0, tmp);

		WCHAR szEmpty[1] = L"";
		DWORD_PTR cch = 0;
		er = ::MsiFormatRecordW(hInstall_, hRecFormat, szEmpty, (DWORD*)&cch);
		if (ERROR_MORE_DATA != er && ERROR_SUCCESS != er) {
			throw installer_exception(_T("get_record_formatted_string:: Failed to get length of string: ") + error::format::from_system(er));
		}
		char_buffer buffer(++cch);

		er = ::MsiFormatRecordW(hInstall_, hRecFormat, buffer, (DWORD*)&cch);
		if (er != ERROR_SUCCESS) {
			throw installer_exception(_T("get_record_formatted_string:: Failed to format string: ") + error::format::from_system(er));
		}
		//logMessage(_T("FMT Record: '") + std::wstring(buffer) + _T("'"));
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
			throw installer_exception(_T("Invalid arguments!"));
		std::wstring str = get_record_formatted_string(hRec, uiField);
		if (str.length() > 0)
			return strEx::stoi(str);
		else
			return MSI_NULL_INTEGER;
	}



	enum WCA_TODO
	{
		WCA_TODO_UNKNOWN,
		WCA_TODO_INSTALL,
		WCA_TODO_UNINSTALL,
		WCA_TODO_REINSTALL,
	};

	/********************************************************************
	WcaGetComponentToDo() - gets a component's install states and 
	determines if they mean install, uninstall, or reinstall.
	********************************************************************/
	WCA_TODO get_component_todo(std::wstring wzComponentId)
	{
		INSTALLSTATE isInstalled = INSTALLSTATE_UNKNOWN;
		INSTALLSTATE isAction = INSTALLSTATE_UNKNOWN;
		UINT er = ::MsiGetComponentStateW(hInstall_, wzComponentId.c_str(), &isInstalled, &isAction);
		if (ERROR_SUCCESS != er) {
			logMessage(_T("State for : ") + wzComponentId + _T(" was unknown due to: ") + error::format::from_system(er));
			return WCA_TODO_UNKNOWN;
		}

		if (WcaIsReInstalling(isInstalled, isAction))
		{
			return WCA_TODO_REINSTALL;
		}
		else if (WcaIsUninstalling(isInstalled, isAction))
		{
			return WCA_TODO_UNINSTALL;
		}
		else if (WcaIsInstalling(isInstalled, isAction))
		{
			return WCA_TODO_INSTALL;
		}
		else
		{
			logMessage(_T("State for : ") + wzComponentId + _T(" was unknown due to; isInstalled: ") + strEx::itos(isInstalled) + _T(", isAction: ")+ strEx::itos(isAction));
			return WCA_TODO_UNKNOWN;
		}
	}
	/********************************************************************
	WcaIsInstalling() - determines if a pair of installstates means install
	********************************************************************/
	boolean WcaIsInstalling(INSTALLSTATE isInstalled, INSTALLSTATE isAction) {
		return (INSTALLSTATE_LOCAL == isAction || INSTALLSTATE_SOURCE == isAction || (INSTALLSTATE_DEFAULT == isAction && (INSTALLSTATE_LOCAL == isInstalled || INSTALLSTATE_SOURCE == isInstalled)));
	}

	/********************************************************************
	WcaIsReInstalling() - determines if a pair of installstates means reinstall
	********************************************************************/
	boolean WcaIsReInstalling(INSTALLSTATE isInstalled, INSTALLSTATE isAction) {
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
			for (unsigned int i =0;i<data.length();i++) {
				if (data[i] == MAGIC_MULTISZ_DELIM) {
					list_.push_back(data.substr(pos, i-pos));
					pos = i+1;
				}
			}
			if (pos < data.length())
				list_.push_back(data.substr(pos));
		}
		~custom_action_data_r() {}

		boolean has_more() {
			return position_ < list_.size();
		}
		std::wstring get_next_string() {
			if (!has_more())
				return _T("");
			return list_[position_++];
		}
		unsigned int get_next_int() {
			return strEx::stoi(get_next_string());
		}
		std::list<std::wstring> get_next_list() {
			std::list<std::wstring> list;
			unsigned int count = get_next_int();
			for (unsigned int i=0;i<count;++i) {
				list.push_back(get_next_string());
			}
			return list;
		}

		std::wstring to_string() {
			std::wstring str;
			for (list_t::const_iterator cit = list_.begin(); cit != list_.end(); ++cit) {
				str += (*cit) + _T("|");
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
			WCHAR delim[] = {MAGIC_MULTISZ_DELIM, 0}; // magic char followed by NULL terminator
			if (!buf_.empty())
				buf_ = str + delim + buf_ ;
			else
				buf_ = str;
		}
		void insert_int(int i) {
			insert_string(strEx::itos(i));
		}
		void write_string(std::wstring str) {
			WCHAR delim[] = {MAGIC_MULTISZ_DELIM, 0}; // magic char followed by NULL terminator
			if (!buf_.empty())
				buf_ += delim;
			buf_ += str;
		}
		void write_int(int i) {
			write_string(strEx::itos(i));
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
		boolean has_data(const int size = 0) const {
			return used_size() > size;
		}
		operator const TCHAR* () const {
			return buf_.c_str();
		}
		unsigned int used_size() const {
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
				throw installer_exception(_T("Failed to set CustomActionData for deferred action") + error::format::from_system(er));
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
			throw installer_exception(_T("Failed MsiDoAction on deferred action") + error::format::from_system(er));
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
		for (int i=0; ::MsiEnumProducts(i, reinterpret_cast<TCHAR*>(&buffer)) == ERROR_SUCCESS; i++) {
			std::wstring name = getProductName(buffer);
			logMessage(_T("Found a product: ") + (std::wstring)buffer + _T("=") + name);
			ret.push_back(buffer);
		}
		return ret;
	}
	std::wstring getProductName(std::wstring code) {
		DWORD size = 0;
		MsiGetProductInfo(code.c_str(), INSTALLPROPERTY_INSTALLEDPRODUCTNAME, NULL, &size);
		size++;
		TCHAR *buffer = new TCHAR[size+4];
		MsiGetProductInfo(code.c_str(), INSTALLPROPERTY_INSTALLEDPRODUCTNAME, buffer, &size);
		std::wstring ret = buffer;
		delete [] buffer;
		return ret;
	}
};