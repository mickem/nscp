#pragma once


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
public:
	msi_helper(MSIHANDLE hInstall, std::wstring action) : hInstall_(hInstall), hProgressPos(NULL), hActionRec(NULL), action_(action) {}

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
		MSIHANDLE hRecord = createSimpleString(msg);
		::MsiProcessMessage(hInstall_, INSTALLMESSAGE(INSTALLMESSAGE_ERROR|MB_OK|MB_ICONERROR), hRecord);
	}
	void logMessage(std::wstring msg) {
		MSIHANDLE hRecord = createSimpleString(msg);
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
};