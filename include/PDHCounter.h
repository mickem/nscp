#pragma once

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <assert.h>
#include <sstream>

namespace PDH {
	class PDHException {
	private:
		std::string str_;
		std::string name_;
		PDH_STATUS pdhStatus_;
	public:
		PDHException(std::string name, std::string str, PDH_STATUS pdhStatus = 0) : name_(name), str_(str), pdhStatus_(pdhStatus) {}
		PDHException(std::string str, PDH_STATUS pdhStatus) : str_(str), pdhStatus_(pdhStatus) {}
		PDHException(std::string str) : str_(str), pdhStatus_(0) {}
		std::string getError() const {
			std::string ret;
			if (!name_.empty())
				ret += name_ + ": ";
			ret += str_;
			if (pdhStatus_ != 0) {
				ret += ": ";
				LPSTR szMessage = NULL;
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_HMODULE,
					GetModuleHandle("PDH.DLL"), pdhStatus_,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					szMessage, 0, NULL);
				if (szMessage)
					ret += ": " + std::string(szMessage);
				else {
					std::stringstream ss;
					ss << pdhStatus_;
					ret += ": " + ss.str();
				}
				LocalFree(szMessage);
			}
			return ret;
		}
	};

	class PDHCounter;
	class PDHCounterListener {
	public:
		virtual void collect(const PDHCounter &counter) = 0;
		virtual void attach(const PDHCounter &counter) = 0;
		virtual void detach(const PDHCounter &counter) = 0;
		virtual DWORD getFormat() const = 0;
	};

	class PDHCounterInfo {
	public:
		DWORD   dwType;
		DWORD   CVersion;
		DWORD   CStatus;
		LONG    lScale;
		LONG    lDefaultScale;
		DWORD_PTR   dwUserData;
		DWORD_PTR   dwQueryUserData;
		std::string  szFullPath;

		std::string   szMachineName;
		std::string   szObjectName;
		std::string   szInstanceName;
		std::string   szParentInstance;
		DWORD    dwInstanceIndex;
		std::string   szCounterName;

		std::string  szExplainText;

		PDHCounterInfo(BYTE *lpBuffer, DWORD dwBufferSize, BOOL explainText) {
			PDH_COUNTER_INFO *info = (PDH_COUNTER_INFO*)lpBuffer;
			dwType = info->dwType;
			CVersion = info->CVersion;
			CStatus = info->CStatus;
			lScale = info->lScale;
			lDefaultScale = info->lDefaultScale;
			dwUserData = info->dwUserData;
			dwQueryUserData = info->dwQueryUserData;
			szFullPath = info->szFullPath;
			if (info->szMachineName)
				szMachineName = info->szMachineName;
			if (info->szObjectName)
				szObjectName = info->szObjectName;
			if (info->szInstanceName)
				szInstanceName = info->szInstanceName;
			if (info->szParentInstance)
				szParentInstance = info->szParentInstance;
			dwInstanceIndex = info->dwInstanceIndex;
			if (info->szCounterName)
				szCounterName = info->szCounterName;
			if (explainText) {
				if (info->szExplainText)
					szExplainText = info->szExplainText;
			}
		}
	};

	class PDHCounter
	{
	private:
		HCOUNTER hCounter_;
		std::string name_;
		PDH_FMT_COUNTERVALUE data_;
		PDHCounterListener *listener_;

	public:

		PDHCounter(std::string name, PDHCounterListener *listener) : name_(name), listener_(listener), hCounter_(NULL){}
		PDHCounter(std::string name) : name_(name), listener_(NULL), hCounter_(NULL){}
		virtual ~PDHCounter(void) {
			if (hCounter_ != NULL)
				remove();
		}

		void setListener(PDHCounterListener *listener) {
			listener_ = listener;
		}

		PDHCounterInfo getCounterInfo(BOOL bExplainText = FALSE) {
			assert(hCounter_ != NULL);
			PDH_STATUS status;
			BYTE *lpBuffer = new BYTE[1025];
			DWORD bufSize = 1024;
			if ((status = PdhGetCounterInfo(hCounter_, bExplainText, &bufSize, (PDH_COUNTER_INFO*)lpBuffer)) != ERROR_SUCCESS) {
				throw PDHException(name_, "getCounterInfo failed (no query)", status);
			}
			return PDHCounterInfo(lpBuffer, bufSize, TRUE);
		}
		const HCOUNTER getCounter() const {
			return hCounter_;
		}
		const std::string getName() const {
			return name_;
		}
		void addToQuery(HQUERY hQuery) {
			PDH_STATUS status;
			if (hQuery == NULL)
				throw PDHException(name_, "addToQuery failed (no query).");
			if (hCounter_ != NULL)
				throw PDHException(name_, "addToQuery failed (already opened).");
			if (listener_)
				listener_->attach(*this);
			if ((status = PdhAddCounter(hQuery, name_.c_str(), 0, &hCounter_)) != ERROR_SUCCESS) {
				hCounter_ = NULL;
				throw PDHException(name_, "PdhOpenQuery failed", status);
			}
			assert(hCounter_ != NULL);
		}
		void remove() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (listener_)
				listener_->detach(*this);
			if ((status = PdhRemoveCounter(hCounter_)) != ERROR_SUCCESS)
				throw PDHException(name_, "PdhRemoveCounter failed", status);
			hCounter_ = NULL;
		}
		void collect() {
			if (hCounter_ == NULL)
				return;
			PDH_STATUS status;
			if (!listener_)
				return;
			if ((status = PdhGetFormattedCounterValue(hCounter_, listener_->getFormat(), NULL, &data_)) != ERROR_SUCCESS)
				throw PDHException(name_, "PdhGetFormattedCounterValue failed", status);
			listener_->collect(*this);
		}
		double getDoubleValue() const {
			return data_.doubleValue;
		}
		__int64 getInt64Value() const {
			return data_.largeValue;
		}
		long getIntValue() const {
			return data_.longValue;
		}
		std::string getStringValue() const {
			return data_.AnsiStringValue;
		}
	};

	class PDHQuery 
	{
	private:
		typedef std::list<PDHCounter*> CounterList;
		CounterList counters_;
		HQUERY hQuery_;
	public:
		PDHQuery() : hQuery_(NULL) {
		}
		virtual ~PDHQuery(void) {
			if (hQuery_)
				close();
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		PDHCounter* addCounter(std::string name, PDHCounterListener *listener) {
			PDHCounter *counter = new PDHCounter(name, listener);
			counters_.push_back(counter);
			return counter;
		}
		PDHCounter* addCounter(std::string name) {
			PDHCounter *counter = new PDHCounter(name);
			counters_.push_back(counter);
			return counter;
		}

		void open() {
			assert(hQuery_ == NULL);
			PDH_STATUS status;
			if( (status = PdhOpenQuery( NULL, 0, &hQuery_ )) != ERROR_SUCCESS)
				throw PDHException("PdhOpenQuery failed", status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->addToQuery(getQueryHandle());
			}
		}

		void close() {
			assert(hQuery_ != NULL);
			DWORD x = PDH_INVALID_HANDLE;
			PDH_STATUS status;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->remove();
			}
			if( (status = PdhCloseQuery(hQuery_)) != ERROR_SUCCESS)
				throw PDHException("PdhCloseQuery failed", status);
			hQuery_ = NULL;
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				delete (*it);
			}
			counters_.clear();
		}

		void gatherData() {
			PDH_STATUS status;
			if ((status = PdhCollectQueryData(hQuery_)) != ERROR_SUCCESS)
				throw PDHException("PdhCollectQueryData failed: ", status);
			for (CounterList::iterator it = counters_.begin(); it != counters_.end(); it++) {
				(*it)->collect();
			}
		}
		void collect() {
			PDH_STATUS status;
			if ((status = PdhCollectQueryData(hQuery_)) != ERROR_SUCCESS)
				throw PDHException("PdhCollectQueryData failed: ", status);
		}

		HQUERY getQueryHandle() const {
			return hQuery_;
		}
	};

	class Enumerations {
	public:

		struct Counter {
			std::string name;
		};
		typedef std::list<Counter> Counters;
		struct Instance {
			std::string name;
		};
		typedef std::list<Instance> Instances;
		struct Object {
			std::string name;
			Instances instances;
			Counters counters;
		};

		typedef std::list<Object> Objects;
		static Objects EnumObjects(DWORD dwDetailLevel = PERF_DETAIL_WIZARD) {
			Objects ret;

			DWORD dwObjectBufLen = 0;
			LPTSTR szObjectBuffer = NULL;
			PDH_STATUS status = PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (status != PDH_MORE_DATA)
				throw PDHException("PdhEnumObjects failed when trying to retrieve size of object buffer", status);

			szObjectBuffer = new char[dwObjectBufLen+1024];
			status = PdhEnumObjects(NULL, NULL, szObjectBuffer, &dwObjectBufLen, dwDetailLevel, FALSE);
			if (status != ERROR_SUCCESS)
				throw PDHException("PdhEnumObjects failed when trying to retrieve object buffer", status);

			char *cp=szObjectBuffer;
			while(*cp != '\0') {
				Object o;
				o.name = cp;
				ret.push_back(o);
				cp += lstrlen(cp)+1;
			}
			delete [] szObjectBuffer;

			for (Objects::iterator it = ret.begin(); it != ret.end(); ++it) {
				DWORD dwCounterBufLen = 0;
				LPTSTR szCounterBuffer = NULL;
				DWORD dwInstanceBufLen = 0;
				LPTSTR szInstanceBuffer = NULL;
				status = PdhEnumObjectItems(NULL, NULL, (*it).name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
				if (status != PDH_MORE_DATA)
					throw PDHException("PdhEnumObjectItems failed when trying to retrieve size for " + (*it).name, status);
				szCounterBuffer = new char[dwCounterBufLen+1024];
				szInstanceBuffer = new char[dwInstanceBufLen+1024];
				status = PdhEnumObjectItems(NULL, NULL, (*it).name.c_str(), szCounterBuffer, &dwCounterBufLen, szInstanceBuffer, &dwInstanceBufLen, dwDetailLevel, 0);
				if (status != ERROR_SUCCESS)
					throw PDHException("PdhEnumObjectItems failed when trying to retrieve buffer for " + (*it).name, status);

				if (dwCounterBufLen > 0) {
					cp=szCounterBuffer;
					while(*cp != '\0') {
						Counter o;
						o.name = cp;
						(*it).counters.push_back(o);
						cp += lstrlen(cp)+1;
					}
				}
				if (dwInstanceBufLen > 0) {
					cp=szInstanceBuffer;
					while(*cp != '\0') {
						Instance o;
						o.name = cp;
						(*it).instances.push_back(o);
						cp += lstrlen(cp)+1;
					}
				}
				delete [] szCounterBuffer;
				delete [] szInstanceBuffer;
			}
			return ret;
		}
		/*
		static str_lst EnumObjectItems(std::string object) {
			str_lst ret;
			DWORD bufLen = 4096;
			DWORD bufLen2 = 0;
			LPTSTR buf = new char[bufLen+1];
			PDH_STATUS status = PdhEnumObjectItems(NULL, NULL, object.c_str(), buf, &bufLen, NULL, &bufLen2, PERF_DETAIL_WIZARD, 0);
			if (status == ERROR_SUCCESS) {
				char *cp=buf;
				while(*cp != '\0') {
					ret.push_back(std::string(cp));
					cp += lstrlen(cp)+1;
				}
			}
			return ret;
		}
		static str_lst EnumObjectInstances(std::string object) {
			str_lst ret;
			DWORD bufLen = 4096;
			DWORD bufLen2 = 0;
			LPTSTR buf = new char[bufLen+1];
			PDH_STATUS status = PdhEnumObjectItems(NULL, NULL, object.c_str(), NULL, &bufLen2, buf, &bufLen, PERF_DETAIL_WIZARD, 0);
			if (status == ERROR_SUCCESS) {
				char *cp=buf;
				while(*cp != '\0') {
					ret.push_back(std::string(cp));
					cp += lstrlen(cp)+1;
				}
			}
			return ret;
		}
		*/
		static bool validate(std::string counter, std::string &error) {
			PDH_STATUS status = PdhValidatePath(counter.c_str());
			switch (status) {
				case ERROR_SUCCESS:
					return true;
				case PDH_CSTATUS_NO_INSTANCE:
					error = "The specified instance of the performance object was not found.";
					break;
				case PDH_CSTATUS_NO_COUNTER:
					error = "The specified counter was not found in the performance object.";
					break;
				case PDH_CSTATUS_NO_MACHINE:
					error = "The specified computer could not be found or connected to.";
					break;
				case PDH_CSTATUS_BAD_COUNTERNAME:
					error = "The counter path string could not be parsed.";
					break;
				case PDH_MEMORY_ALLOCATION_FAILURE:
					error = "The function is unable to allocate a required temporary buffer.";
					break;
			}
			return false;
		}
	};


}