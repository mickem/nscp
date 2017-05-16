/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <list>
#include <pdh.h>
#include <pdhmsg.h>
#include <sstream>
#include <error/error.hpp>

#include <boost/shared_ptr.hpp>

namespace PDH {
	typedef HANDLE PDH_HCOUNTER;
	typedef HANDLE PDH_HQUERY;
	typedef HANDLE PDH_HLOG;

	class pdh_error {
		PDH_STATUS status_;
	public:
		pdh_error() : status_(ERROR_SUCCESS) {}
		pdh_error(PDH_STATUS status) : status_(status) {}
		pdh_error(const pdh_error &other) : status_(other.status_) {}
		pdh_error& operator=(pdh_error const& other) {
			status_ = other.status_;
			return *this;
		}

		std::string get_message() const {
			if (is_ok())
				return "";
			return error::format::from_module("PDH.DLL", status_);
		}

		bool is_error() const {
			return status_ != ERROR_SUCCESS;
		}
		bool is_ok() const {
			return status_ == ERROR_SUCCESS;
		}
		bool is_more_data() {
			return status_ == PDH_MORE_DATA;
		}
		bool is_invalid_data() {
			return status_ == PDH_INVALID_DATA || status_ == PDH_CSTATUS_INVALID_DATA;
		}
		bool is_not_found() {
			return status_ == PDH_CSTATUS_NO_OBJECT || status_ == PDH_CSTATUS_NO_COUNTER;
		}

		bool is_negative_denominator() {
			return status_ == PDH_CALC_NEGATIVE_DENOMINATOR || status_ == PDH_CALC_NEGATIVE_VALUE;
		}
	};

	class pdh_exception : public std::exception {
	private:
		std::string error_;
	public:
		pdh_exception(std::string name, std::string str) : error_(name + ": " + str) {}
		pdh_exception(std::string str, const pdh_error &error) : error_(str) {
			if (error.is_error()) {
				error_ += ": " + error.get_message();
			}
		}
		pdh_exception(std::string error) : error_(error) {}
		~pdh_exception() throw() {}
		const char* what() const throw() { return error_.c_str(); }

		std::string reason() const {
			return error_;
		}
	};

	namespace types {
		typedef enum data_type_struct {
			type_int64, type_uint64
		};
		typedef enum data_format_struct {
			format_large
		};
		typedef enum collection_strategy_struct {
			rrd, static_value
		};
	}

	struct pdh_object {
		std::string path;
		std::string alias;

		types::data_type_struct type;
		types::data_format_struct format;
		types::collection_strategy_struct strategy_;

		long buffer_size;
		unsigned long flags_;
		std::string instances_;

		static const int format_large = 0x00000400;
		static const int format_long = 0x00000100;
		static const int format_double = 0x00000200;

		typedef enum data_types {
			type_double, type_long, type_large
		};

		void set_counter(std::string counter) {
			path = counter;
		}
		void set_alias(std::string alias_) {
			alias = alias_;
		}

		pdh_object() : buffer_size(0), flags_(format_double), strategy_(types::static_value) {}

		void set_default_buffer_size(std::string buffer_size_);
		void set_buffer_size(std::string buffer_size_);

		void set_type(const std::string &type);
		void set_type(const data_types type);
		data_types get_type();

		void add_flags(const std::string &flag);
		void set_flags(const std::string &flags);
		unsigned long get_flags();

		bool is_static() {
			return strategy_ == types::static_value;
		}
		bool is_rrd() {
			return strategy_ == types::rrd;
		}
		void set_strategy(std::string strategy) {
			if (strategy == "static" || strategy.empty()) {
				strategy_ = types::static_value;
			} else if (strategy == "round robin" || strategy == "rrd") {
				strategy_ = types::rrd;
				set_default_buffer_size("60m");
			} else {
				throw pdh_exception("Invalid strategy: " + strategy);
			}
		}
		void set_strategy_static() {
			strategy_ = types::static_value;
		}
		void set_instances(std::string instances) {
			instances_ = instances;
		}
		bool has_instances();
	};

	struct pdh_instance_interface {
		virtual double get_average(long seconds) = 0;
		virtual double get_value() = 0;
		virtual long long get_int_value() = 0;
		virtual double get_float_value() = 0;
		virtual std::string get_name() const = 0;
		virtual std::string get_counter() const = 0;

		virtual bool has_instances() = 0;
		virtual std::list<boost::shared_ptr<pdh_instance_interface> > get_instances() = 0;

		virtual DWORD get_format() = 0;
		virtual void collect(const PDH_FMT_COUNTERVALUE &value) = 0;
	};
	typedef boost::shared_ptr<pdh_instance_interface> pdh_instance;

	class counter_info {
	public:
		DWORD   dwType;
		DWORD   CVersion;
		DWORD   CStatus;
		LONG    lScale;
		LONG    lDefaultScale;
		DWORD_PTR   dwUserData;
		DWORD_PTR   dwQueryUserData;
		std::wstring  szFullPath;

		std::wstring   szMachineName;
		std::wstring   szObjectName;
		std::wstring   szInstanceName;
		std::wstring   szParentInstance;
		DWORD    dwInstanceIndex;
		std::wstring   szCounterName;

		std::wstring  szExplainText;
		counter_info(BYTE *lpBuffer, DWORD dwBufferSize, BOOL explainText);
	};

	class subscriber {
	public:
		virtual void on_unload() = 0;
		virtual void on_reload() = 0;
	};

	class impl_interface {
	public:
		virtual pdh_error PdhLookupPerfIndexByName(LPCTSTR szMachineName, LPCTSTR szName, DWORD *dwIndex) = 0;
		virtual pdh_error PdhLookupPerfNameByIndex(LPCTSTR szMachineName, DWORD dwNameIndex, LPTSTR szNameBuffer, LPDWORD pcchNameBufferSize) = 0;
		virtual pdh_error PdhExpandCounterPath(LPCTSTR szWildCardPath, LPTSTR mszExpandedPathList, LPDWORD pcchPathListLength) = 0;
		virtual pdh_error PdhGetCounterInfo(PDH::PDH_HCOUNTER hCounter, BOOLEAN bRetrieveExplainText, LPDWORD pdwBufferSize, PDH_COUNTER_INFO* lpBuffer) = 0;
		virtual pdh_error PdhAddCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) = 0;
		virtual pdh_error PdhAddEnglishCounter(PDH::PDH_HQUERY hQuery, LPCWSTR szFullCounterPath, DWORD_PTR dwUserData, PDH::PDH_HCOUNTER * phCounter) = 0;
		virtual pdh_error PdhRemoveCounter(PDH::PDH_HCOUNTER hCounter) = 0;
		virtual pdh_error PdhGetRawCounterValue(PDH::PDH_HCOUNTER hCounter, LPDWORD dwFormat, PPDH_RAW_COUNTER  pValue) = 0;
		virtual pdh_error PdhGetFormattedCounterValue(PDH_HCOUNTER hCounter, DWORD dwFormat, LPDWORD lpdwType, PPDH_FMT_COUNTERVALUE pValue) = 0;
		virtual pdh_error PdhOpenQuery(LPCWSTR szDataSource, DWORD_PTR dwUserData, PDH::PDH_HQUERY * phQuery) = 0;
		virtual pdh_error PdhCloseQuery(PDH_HQUERY hQuery) = 0;
		virtual pdh_error PdhCollectQueryData(PDH_HQUERY hQuery) = 0;
		virtual pdh_error PdhValidatePath(LPCWSTR szFullPathBuffer, bool force_reload) = 0;
		virtual pdh_error PdhEnumObjects(LPCWSTR szDataSource, LPCWSTR szMachineName, LPWSTR mszObjectList, LPDWORD pcchBufferSize, DWORD dwDetailLevel, BOOL bRefresh) = 0;
		virtual pdh_error PdhEnumObjectItems(LPCWSTR szDataSource, LPCWSTR szMachineName, LPCWSTR szObjectName, LPWSTR mszCounterList, LPDWORD pcchCounterListLength, LPWSTR mszInstanceList, LPDWORD pcchInstanceListLength, DWORD dwDetailLevel, DWORD dwFlags) = 0;
		virtual pdh_error PdhExpandWildCardPath(LPCTSTR szDataSource, LPCTSTR szWildCardPath, LPWSTR  mszExpandedPathList, LPDWORD pcchPathListLength, DWORD dwFlags) = 0;

		virtual bool reload() = 0;
		virtual void add_listener(subscriber* sub) = 0;
		virtual void remove_listener(subscriber* sub) = 0;
	};

	class factory {
		static boost::shared_ptr<impl_interface> instance;
	public:
		static boost::shared_ptr<impl_interface> get_impl();
		static void set_thread_safe();
		static void set_native();

		static pdh_instance create(pdh_object object);
		static pdh_instance create(std::string counter);
		static pdh_instance create(std::string counter, pdh_object object);
	};

	class helpers {
	public:
		static std::list<std::string> build_list(TCHAR *buffer, DWORD bufferSize);
	};
}