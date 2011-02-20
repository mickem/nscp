#pragma once

#define _WIN32_DCOM

#include <map>
#include <string>

#include <taskschd.h>
#include <ATLComTime.h>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>

#include <error.hpp>

#include <parsers/where/expression_ast.hpp>
#include <parsers/where/varible_handler.hpp>

#include <parsers/filter/where_filter.hpp>


namespace tasksched_filter {

	struct filter_obj_handler;
	struct filter_obj {

		typedef parsers::where::expression_ast<filter_obj_handler> ast_expr_type;


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
		typedef boost::optional<std::wstring> op_wstring;
		typedef boost::optional<unsigned long> op_dword;
		typedef boost::optional<unsigned short> op_word;
		typedef boost::optional<long> op_long;
		typedef boost::optional<task_sched_date> op_date;

#define DECLARE_GET_STRING(variable) op_wstring variable; std::wstring get_ ## variable();
#define DECLARE_GET_DWORD(variable) op_dword variable; unsigned long get_ ## variable();
#define DECLARE_GET_WORD(variable) op_word variable; unsigned short get_ ## variable();
#define DECLARE_GET_WORD(variable) op_word variable; unsigned short get_ ## variable();
#define DECLARE_GET_HRESULT(variable) op_long variable; long get_ ## variable();
#define DECLARE_GET_DATE(variable) op_date variable; task_sched_date get_ ## variable();

 		DECLARE_GET_STRING(title);
		DECLARE_GET_HRESULT(exit_code);
 		DECLARE_GET_WORD(status);
 		DECLARE_GET_DATE(most_recent_run_time);

		IRegisteredTask* task;
		filter_obj(IRegisteredTask* task) : task(task) {}
		filter_obj() : task(NULL) {}

		ast_expr_type fun_convert_status(parsers::where::value_type target_type, ast_expr_type const& subject);

		static long convert_status(std::wstring key);
		static std::wstring convert_status(long status);

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
		struct string_fetch_traits : public fetch_traits<LPWSTR, std::wstring> {
			static std::wstring get_default() { return _T(""); }
			static void cleanup(LPWSTR obj) {SysFreeString(obj); }
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static std::wstring convert(HRESULT hr, LPWSTR value) { return value; }
		};

		template<typename TTarget, typename TReturn>
		struct word_fetch_traits : public fetch_traits<TTarget, TReturn> {
			static TReturn get_default() { return 0; }
			static void cleanup(TTarget obj) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr); }
			static TReturn convert(HRESULT hr, TTarget value) { return value; }
		};
		struct date_fetch_traits : public fetch_traits<DATE, task_sched_date> {
			typedef DATE TTarget;
			typedef task_sched_date TReturn;
			static TReturn get_default() { return task_sched_date(); }
			static void cleanup(TTarget obj) {}
			static bool has_failed(HRESULT hr) { return FAILED(hr) && hr != SCHED_S_TASK_HAS_NOT_RUN; }
			static TReturn convert(HRESULT hr, TTarget &value) {
				COleDateTime date(value);
				SYSTEMTIME tmp;
				date.GetAsSystemTime(tmp);
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
					parent->error(_T("ERROR: ") + error::format::from_system(hr));
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
		fetcher<date_fetch_traits > date_fetcher;

		std::wstring render(std::wstring format);

	public:
		void error(std::wstring err) { errors.push_back(err); }
		bool has_error() { return !errors.empty(); }
		std::wstring get_error() { return strEx::joinEx(errors, _T(", ")); }
	private:
		std::list<std::wstring> errors;

	};

	typedef filter_obj flyweight_type;
	struct filter_obj_handler : public parsers::where::varible_handler<filter_obj_handler, filter_obj> {

		static const parsers::where::value_type type_custom_hresult = parsers::where::type_custom_int_1;
		static const parsers::where::value_type type_custom_type = parsers::where::type_custom_int_2;

		typedef parsers::where::varible_handler<filter_obj_handler, filter_obj> handler;
		typedef parsers::where::expression_ast<filter_obj_handler> ast_expr_type;
		typedef std::map<std::wstring,parsers::where::value_type> types_type;
		typedef filter_obj object_type;

		filter_obj_handler();

		handler::bound_string_type bind_string(std::wstring key);
		handler::bound_int_type bind_int(std::wstring key);
		bool has_function(parsers::where::value_type to, std::wstring name, ast_expr_type subject);
		handler::bound_function_type bind_function(parsers::where::value_type to, std::wstring name, ast_expr_type subject);

		bool has_variable(std::wstring key);
		parsers::where::value_type get_type(std::wstring key);
		bool can_convert(parsers::where::value_type from, parsers::where::value_type to);

		flyweight_type static_record;
		object_type get_static_object() {
			return object_type(static_record);
		}

	public:
		void error(std::wstring err) { errors.push_back(err); }
		bool has_error() { return !errors.empty(); }
		std::wstring get_error() { return strEx::joinEx(errors, _T(", ")); }
	private:
		std::list<std::wstring> errors;

	private:
		types_type types;

	};

	typedef where_filter::engine_interface<flyweight_type> filter_engine_type;
	typedef where_filter::argument_interface<flyweight_type> filter_argument_type;
	typedef where_filter::result_counter_interface<flyweight_type> filter_result_type;

	typedef boost::shared_ptr<filter_engine_type> filter_engine;
	typedef boost::shared_ptr<filter_argument_type> filter_argument;
	typedef boost::shared_ptr<filter_result_type> filter_result;

	struct factories {
		static filter_engine create_engine(tasksched_filter::filter_argument arg);
		static filter_result create_result(tasksched_filter::filter_argument arg);
		static filter_argument create_argument(std::wstring syntax);

	};
}
