#pragma once

#define _WIN32_DCOM

#include <map>
#include <string>

//#define _ATL_NTDDI_MIN 
//#define _WIN32_WINNT 0x0403
//#include <Windows.h>
#include <taskschd.h>
//#include <ATLComTime.h>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <error.hpp>

#include <parsers/where/node.hpp>
#include <parsers/where/engine.hpp>
#include <parsers/filter/modern_filter.hpp>
#include <parsers/where/filter_handler_impl.hpp>

namespace tasksched_filter {

	struct filter_obj {

		struct task_sched_date {
			unsigned long long date_;
			bool never_;
			task_sched_date() : never_(false), date_(0) {}
			task_sched_date(bool never) : never_(never), date_(0) {}
			task_sched_date(unsigned long long date) : never_(false), date_(date) {}
			void operator=(unsigned long long date) {
				date_ = date;
				never_ = false;
			}
			operator unsigned long long () {
				if (never_ || date_ == 0)
					return 0;
				return strEx::filetime_to_time(date_);
			}
			operator const unsigned long long () const {
				if (never_ || date_ == 0)
					return 0;
				return strEx::filetime_to_time(date_);
			}
		};
		typedef boost::optional<std::string> op_string;
		typedef boost::optional<unsigned long> op_dword;
		typedef boost::optional<unsigned short> op_word;
		typedef boost::optional<long> op_long;
		typedef boost::optional<task_sched_date> op_date;
		typedef boost::optional<bool> op_bool;

#define DECLARE_GET_STRING(variable) op_string variable; std::string get_ ## variable();
#define DECLARE_GET_DWORD(variable) op_dword variable; unsigned long get_ ## variable();
#define DECLARE_GET_WORD(variable) op_word variable; unsigned short get_ ## variable();
#define DECLARE_GET_WORD(variable) op_word variable; unsigned short get_ ## variable();
#define DECLARE_GET_HRESULT(variable) op_long variable; long get_ ## variable();
#define DECLARE_GET_DATE(variable) op_date variable; task_sched_date get_ ## variable();
#define DECLARE_GET_BOOL(variable) op_bool variable; bool get_ ## variable();

 		DECLARE_GET_STRING(title);
		DECLARE_GET_HRESULT(exit_code);
 		DECLARE_GET_WORD(status);
		DECLARE_GET_DATE(most_recent_run_time);
		DECLARE_GET_BOOL(enabled);

		IRegisteredTask* task;
		std::string folder;
		filter_obj(std::string folder, IRegisteredTask* task) : folder(folder), task(task) {}
		filter_obj() : task(NULL) {}

		std::string get_folder() const {
			return folder;
		}

		static unsigned long long systemtime_to_ullFiletime(SYSTEMTIME time) {
			FILETIME localFileTime, fileTime;
			SystemTimeToFileTime(&time, &localFileTime);
			LocalFileTimeToFileTime(&localFileTime, &fileTime);
			return ((fileTime.dwHighDateTime * ((unsigned long long)MAXDWORD+1)) + (unsigned long long)fileTime.dwLowDateTime);
		}

		template<typename TTarget, typename TReturn>
		struct fetch_traits {
			typedef typename TTarget raw_type;
			typedef typename TReturn data_type;
			typedef typename boost::optional<TReturn> client_type;
			typedef HRESULT (__stdcall IRegisteredTask::*fun_ptr_type)(raw_type*);
		};
		struct string_fetch_traits : public fetch_traits<LPWSTR, std::string> {
			static std::string get_default() { return ""; }
			static void cleanup(LPWSTR obj) {SysFreeString(obj); }
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static std::string convert(HRESULT hr, LPWSTR value) { return utf8::cvt<std::string>(value); }
		};

		template<typename TTarget, typename TReturn>
		struct word_fetch_traits : public fetch_traits<TTarget, TReturn> {
			static TReturn get_default() { return 0; }
			static void cleanup(TTarget obj) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static TReturn convert(HRESULT hr, TTarget value) { return value; }
		};
		template<typename TTarget, typename TReturn>
		struct bool_fetch_traits : public fetch_traits<TTarget, TReturn> {
			static TReturn get_default() { return false; }
			static void cleanup(TTarget obj) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static TReturn convert(HRESULT hr, TTarget value) { return value==VARIANT_TRUE; }
		};
		struct date_fetch_traits : public fetch_traits<DATE, task_sched_date> {
			typedef DATE TTarget;
			typedef task_sched_date TReturn;
			static TReturn get_default() { return task_sched_date(); }
			static void cleanup(TTarget obj) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr) && hr != SCHED_S_TASK_HAS_NOT_RUN; }
			static TReturn convert(HRESULT hr, TTarget &value) {
				// TODO: FIXME: Readd this!
				//COleDateTime date(value);
				SYSTEMTIME tmp;
				//date.GetAsSystemTime(tmp);
				return task_sched_date(systemtime_to_ullFiletime(tmp)); 
			}
		};

		template<typename traits>
		struct fetcher {
			typedef typename traits::fun_ptr_type fun_ptr_type;
			typedef typename traits::data_type data_type;
			typedef typename traits::raw_type raw_type;
			typedef typename traits::client_type client_type;

			bool inline fetch_data(filter_obj *parent, fun_ptr_type f, data_type &data) {
				raw_type tmp;
				HRESULT hr = (parent->task->*f)(&tmp);
				if (traits::has_failed(hr)) {
					throw filter_exception("ERROR: " + ::error::format::from_system(hr));
					data = traits::get_default();
					return false;
				} else {
					data = traits::convert(hr, tmp);
					traits::cleanup(tmp);
					return true;
				}
			}

			data_type inline fetch(filter_obj *parent, fun_ptr_type f, client_type &client) {
				if (!client) {
					client.reset(traits::get_default());
					data_type tmp;
					if (fetch_data(parent, f, tmp))
						client.reset(tmp);
				}
				return *client;
			}
		};

		fetcher<string_fetch_traits> string_fetcher;
		fetcher<word_fetch_traits<HRESULT, long> > hresult_fetcher;
		fetcher<word_fetch_traits<DWORD, unsigned long> > dword_fetcher;
		fetcher<word_fetch_traits<WORD, unsigned short> > word_fetcher;
		fetcher<word_fetch_traits<TASK_STATE, unsigned short> > state_fetcher;
		fetcher<date_fetch_traits> date_fetcher;
		fetcher<bool_fetch_traits<VARIANT_BOOL, bool> > bool_fetcher;


	};

	typedef parsers::where::filter_handler_impl<boost::shared_ptr<filter_obj> > native_context;
	struct filter_obj_handler : public native_context {

		static const parsers::where::value_type type_custom_hresult = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

		filter_obj_handler();
	};


	typedef modern_filter::modern_filters<tasksched_filter::filter_obj, tasksched_filter::filter_obj_handler> filter;
}
