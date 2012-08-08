/**************************************************************************
*   Copyright (C) 2004-2007 by Michael Medin <michael@medin.name>         *
*                                                                         *
*   This code is part of NSClient++ - http://trac.nakednuns.org/nscp      *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#pragma once

#include <string>
#include <strEx.h>
#include <format.hpp>
#include <math.h>

#define MAKE_PERFDATA_SIMPLE(alias, value, unit) _T("'") + alias + _T("'=") + value + unit
#define MAKE_PERFDATA(alias, value, unit, warn, crit) _T("'") + alias + _T("'=") + value + unit + _T(";") + warn + _T(";") + crit 
#define MAKE_PERFDATA_EX(alias, value, unit, warn, crit, xmin, xmax) _T("'") + alias + _T("'=") + value + unit + _T(";") + warn + _T(";") + crit + _T(";") + xmin + _T(";") + xmax

namespace checkHolders {



	typedef enum { warning, critical} ResultType;
	typedef enum { above = 1, below = -1, same = 0 } checkResultType;
	class check_exception {
		std::wstring error_;
	public:
		check_exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() {
			return error_;
		}
	};

	struct parse_exception : public check_exception {
		parse_exception(std::wstring error) : check_exception(error) {}
	};

	static std::wstring formatAbove(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T(" > warning");
		else if (what == critical)
			return str + _T(" > critical");
		return str + _T(" > unknown");
	}

	static std::wstring formatBelow(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T(" < warning");
		else if (what == critical)
			return str + _T(" < critical");
		return str + _T(" < unknown");
	}
	static std::wstring formatSame(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T(" = warning");
		else if (what == critical)
			return str + _T(" = critical");
		return str + _T(" = unknown");
	}
	static std::wstring formatNotSame(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T(" != warning");
		else if (what == critical)
			return str + _T(" != critical");
		return str + _T(" != unknown");
	}
	static std::wstring formatState(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T(" (warning)");
		else if (what == critical)
			return str + _T(" (critical)");
		return str + _T(" (unknown)");
	}
	static std::wstring formatNotFound(std::wstring str, ResultType what) {
		if (what == warning)
			return str + _T("not found (warning)");
		else if (what == critical)
			return str + _T("not found (critical)");
		return str + _T("not found (unknown)");
	}


	typedef enum {showLong, showShort, showProblems, showUnknown} showType;
	template <class TContents>
	struct CheckContainer {
		typedef CheckContainer<TContents> TThisType;
		typedef typename TContents::TValueType TPayloadValueType;
		TContents warn;
		TContents crit;
		std::wstring data;
		std::wstring alias;

		showType show;
		bool perfData;


		CheckContainer() : show(showUnknown), perfData(true)
		{}
		CheckContainer(std::wstring data_, TContents warn_, TContents crit_) 
			: data(data_), warn(warn_), crit(crit_), show(showUnknown), perfData(true)
		{}
		CheckContainer(std::wstring data_, std::wstring alias_, TContents warn_, TContents crit_) 
			: data(data_), alias(alias_), warn(warn_), crit(crit_), show(showUnknown), perfData(true)
		{}
		CheckContainer(const TThisType &other) 
			: data(other.data), alias(other.alias), warn(other.warn), crit(other.crit), show(other.show), perfData(other.perfData)

		{}
		CheckContainer<TContents>& operator =(const CheckContainer<TContents> &other) {
			warn = other.warn;
			crit = other.crit;
			data = other.data;
			alias = other.alias;
			show = other.show;
			perfData = other.perfData;
			return *this;
		}
		void reset() {
			warn.reset();
			crit.reset();
		}
		std::wstring getAlias() {
			if (alias.empty())
				return data;
			return alias;
		}
		void setDefault(TThisType def) {
			if (!warn.hasBounds())
				warn = def.warn;
			if (!crit.hasBounds())
				crit = def.crit;
			if (show == showUnknown)
				show = def.show;
		}
		bool showAll() {
			return show != showProblems;
		}
		std::wstring gatherPerfData(typename TContents::TValueType &value) {
			if (crit.hasBounds())
				return crit.gatherPerfData(getAlias(), value, warn, crit);
			else if (warn.hasBounds())
				return warn.gatherPerfData(getAlias(), value, warn, crit);
			else {
				TContents tmp;
				return tmp.gatherPerfData(getAlias(), value);
			}
		}
		bool hasBounds() {
			return warn.hasBounds() || crit.hasBounds();
		}
		void runCheck(typename TContents::TValueType value, NSCAPI::nagiosReturn &returnCode, std::wstring &message, std::wstring &perf) {
			std::wstring tstr;
			if (crit.check(value, getAlias(), tstr, critical)) {
				//std::wcout << _T("crit") << std::endl;
				nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
			} else if (warn.check(value, getAlias(), tstr, warning)) {
				//std::wcout << _T("warn") << std::endl;
				nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
			}else if (show == showLong) {
				//std::wcout << _T("long") << std::endl;
				tstr = getAlias() + _T(": ") + TContents::toStringLong(value);
			}else if (show == showShort) {
				//std::wcout << _T("short") << std::endl;
				tstr = getAlias() + _T(": ") + TContents::toStringShort(value);
			}
			if (perfData) {
				if (!perf.empty())
					perf += _T(" ");
				perf += gatherPerfData(value);
			}
			if (!message.empty() && !tstr.empty())
				message += _T(", ");
			if (!tstr.empty())
				message += tstr;
			//std::wcout << _T("result: ") << tstr << _T("--") << std::endl;
		}
	};

	template <class TContents>
	struct MagicCheckContainer : public CheckContainer<TContents> {
		typedef CheckContainer<TContents> tParent;

		MagicCheckContainer() : tParent() {}
		MagicCheckContainer(std::wstring data_, TContents warn_, TContents crit_) : tParent(data_, warn_, crit_) {}
		MagicCheckContainer(std::wstring data_, std::wstring alias_, TContents warn_, TContents crit_) : tParent(data_, alias_, warn_, crit_) {}
		MagicCheckContainer(const TThisType &other) : tParent(other) {}

		MagicCheckContainer<TContents>& operator =(const MagicCheckContainer<TContents> &other) {
			tParent::operator =(other);
			return *this;
		}


		void set_magic(double magic) {
			warn.max_.set_magic(magic);
			warn.min_.set_magic(magic);
			crit.max_.set_magic(magic);
			crit.min_.set_magic(magic);
		}

	};

	template <class value_type>
	struct check_proxy_interface {
		virtual bool showAll() = 0;
		virtual std::wstring gatherPerfData(value_type &value) = 0;
		virtual bool hasBounds() = 0;
		virtual void runCheck(value_type &value, NSCAPI::nagiosReturn &returnCode, std::wstring &message, std::wstring &perf) = 0;
		virtual void set_warn_bound(std::wstring value) = 0;
		virtual void set_crit_bound(std::wstring value) = 0;
		virtual void set_showall(showType show_) = 0;

		//virtual std::wstring get_default_alias() = 0;

	};


	//typedef enum {showLong, showShort, showProblems, showUnknown} showType;
	template <class container_value_type, class impl_type>
	class check_proxy_container : public check_proxy_interface<container_value_type> {
		typedef check_proxy_container<container_value_type, impl_type> TThisType;
		impl_type impl_;
	public:
		virtual typename impl_type::TPayloadValueType get_value(container_value_type &value) = 0;

		void set_warn_bound(std::wstring value) {
			impl_.warn = value;
		}
		void set_crit_bound(std::wstring value) {
			impl_.crit = value;
		}
		void set_alias(std::wstring value) {
			impl_.alias = value;
		}


		check_proxy_container() {}
		/*
		void setDefault(TThisType def) {
			if (!warn.hasBounds())
				warn = def.warn;
			if (!crit.hasBounds())
				crit = def.crit;
			if (show == showUnknown)
				show = def.show;
		}
		*/
		bool showAll() {
			return impl_.showAll();
		}
		void set_showall(showType show_) {
			impl_.show = show_;
		}
		std::wstring gatherPerfData(container_value_type &value) {
			typename impl_type::TPayloadValueType real_value = get_value(value);
			return impl_.gatherPerfData(real_value);
		}
		bool hasBounds() {
			return impl_.hasBounds();
		}
		void runCheck(container_value_type &value, NSCAPI::nagiosReturn &returnCode, std::wstring &message, std::wstring &perf) {
			typename impl_type::TPayloadValueType real_value = get_value(value);
			return impl_.runCheck(real_value, returnCode, message, perf);
		}
	};


	template <class value_type>
	struct check_multi_container {
		typedef check_multi_container<value_type> TThisType;
		typedef check_proxy_interface<value_type> check_type;
		typedef std::list<check_type*> check_list_type;
		check_list_type checks_;
		std::wstring data;
		std::wstring alias;

		std::wstring cached_warn_;
		std::wstring cached_crit_;

		showType show;
		bool perfData;

		void set_warn_bound(std::wstring value) {
// 			if (checks_.empty())
				cached_warn_ = value;
// 			else
// 				(checks_.back())->set_warn_bound(value);
		}
		void set_crit_bound(std::wstring value) {
// 			if (checks_.empty())
				cached_crit_ = value;
// 			else
// 				(checks_.back())->set_crit_bound(value);
		}

		void add_check(check_type *check) {
			if (check != NULL) {
				if (!cached_warn_.empty())
					check->set_warn_bound(cached_warn_);
				if (!cached_crit_.empty())
					check->set_crit_bound(cached_crit_);
				checks_.push_back(check);
			}
			cached_warn_ = _T("");
			cached_crit_ = _T("");
		}

		check_multi_container() : show(showUnknown), perfData(true)
		{}
	private:
		check_multi_container(const TThisType &other) 
			: data(other.data), alias(other.alias), checks_(other.checks_), show(other.show) 
		{}
	public:
		~check_multi_container() {
			for (check_list_type::iterator it=checks_.begin(); it != checks_.end(); ++it) {
				delete *it;
			}
			checks_.clear();
		}
		std::wstring getAlias() {
			if (alias.empty())
				return data;
			return alias;
		}
		void setDefault(TThisType def) {
			if (show == showUnknown)
				show = def.show;
		}
		bool showAll() {
			return show != showProblems;
		}
		std::wstring gatherPerfData(value_type &value) {
			std::wstring ret;
			for (check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				ret += (*cit)->gatherPerfData((*cit)->getAlias(), value);
			}
		}
		bool hasBounds() {
			for (check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				if ((*cit)->hasBounds())
					return true;
			}
			return false;
		}
		void runCheck(value_type value, NSCAPI::nagiosReturn &returnCode, std::wstring &message, std::wstring &perf) {
			for (check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				(*cit)->set_showall(show);
				(*cit)->runCheck(value, returnCode, message, perf);
			}
			std::wcout << _T("result: ") << message << std::endl;
		}
	};

	typedef unsigned __int64 disk_size_type;
	template <typename TType = disk_size_type>
	class disk_size_handler {
	public:
		static std::wstring print(TType value) {
			return format::format_byte_units(value);
		}
		static std::wstring get_perf_unit(TType value) {
			return format::find_proper_unit_BKMG(value);
		}
		static std::wstring print_perf(TType value, std::wstring unit) {
			return format::format_byte_units(value, unit);
		}
		static TType parse(std::wstring s) {
			TType val = format::decode_byte_units(s);
			if (val == 0 && s.length() > 1 && s[0] != L'0')
				NSC_LOG_MESSAGE_STD(_T("Maybe this is not what you want: ") + s + _T(" = ") + strEx::itos(val));
			return val;
		}
		static TType parse_percent(std::wstring s) {
			return strEx::stoi64(s);
		}
		static std::wstring print_percent(TType value) {
			return strEx::itos(value) + _T("%");
		}
		static std::wstring print_unformated(TType value) {
			return strEx::itos(value);
		}

		static std::wstring key_total() {
			return _T("Total: ");
		}
		static std::wstring key_lower() {
			return _T("Used: ");
		}
		static std::wstring key_upper() {
			return _T("Free: ");
		}
		static std::wstring key_prefix() {
			return _T("");
		}
		static std::wstring key_postfix() {
			return _T("");
		}

	};

	typedef unsigned __int64 time_type;
	template <typename TType = time_type>
	class time_handler {
	public:
		static TType parse(std::wstring s) {
			TType val = strEx::stoi64_as_time(s);
			if (val == 0 && s.length() > 1 && s[0] != L'0')
				NSC_LOG_MESSAGE_STD(_T("Maybe this is not what you want: ") + s + _T(" = 0"));
			return val;
		}
		static TType parse_percent(std::wstring s) {
			return strEx::stoi(s);
		}
		static std::wstring print(TType value) {
			return strEx::itos_as_time(value);
		}
		static std::wstring print_percent(TType value) {
			return strEx::itos(value) + _T("%");
		}
		static std::wstring print_unformated(TType value) {
			return strEx::itos(value);
		}
		static std::wstring get_perf_unit(TType value) {
			return _T("");
		}
		static std::wstring print_perf(TType value, std::wstring unit) {
			return strEx::itos(value);
		}
		static std::wstring key_prefix() {
			return _T("");
		}
		static std::wstring key_postfix() {
			return _T("");
		}

	};


	class int_handler {
	public:
		static int parse(std::wstring s) {
			int val = strEx::stoi(s);
			if (val == 0 && s.length() > 1 && s[0] != L'0')
				NSC_LOG_MESSAGE_STD(_T("Maybe this is not what you want: ") + s + _T(" = 0"));
			return val;
		}
		static int parse_percent(std::wstring s) {
			return strEx::stoi(s);
		}
		static std::wstring print(int value) {
			return strEx::itos(value);
		}
		static std::wstring get_perf_unit(int value) {
			return _T("");
		}
		static std::wstring print_perf(int value, std::wstring unit) {
			return strEx::itos(value);
		}
		static std::wstring print_unformated(int value) {
			return strEx::itos(value);
		}
		static std::wstring print_percent(int value) {
			return strEx::itos(value) + _T("%");
		}
		static std::wstring key_prefix() {
			return _T("");
		}
		static std::wstring key_postfix() {
			return _T("");
		}
	};
	class int64_handler {
	public:
		static __int64 parse(std::wstring s) {
			__int64 val = strEx::stoi64(s);
			if (val == 0 && s.length() > 1 && s[0] != L'0')
				NSC_LOG_MESSAGE_STD(_T("Maybe this is not what you want: ") + s + _T(" = 0"));
			return val;
		}
		static __int64 parse_percent(std::wstring s) {
			return strEx::stoi(s);
		}
		static std::wstring print(__int64 value) {
			return boost::lexical_cast<std::wstring>(value);
		}
		static std::wstring get_perf_unit(__int64 value) {
			return _T("");
		}
		static std::wstring print_perf(__int64 value, std::wstring unit) {
			return boost::lexical_cast<std::wstring>(value);
		}
		static std::wstring print_unformated(__int64 value) {
			return boost::lexical_cast<std::wstring>(value);
		}
		static std::wstring print_percent(__int64 value) {
			return boost::lexical_cast<std::wstring>(value) + _T("%");
		}
		static std::wstring key_prefix() {
			return _T("");
		}
		static std::wstring key_postfix() {
			return _T("");
		}
	};
	class double_handler {
	public:
		static double parse(std::wstring s) {
			return strEx::stod(s);
		}
		static double parse_percent(std::wstring s) {
			return strEx::stod(s);
		}
		static std::wstring get_perf_unit(double value) {
			return _T("");
		}
		static std::wstring print_perf(double value, std::wstring unit) {
			return strEx::itos_non_sci(value);
		}
		static std::wstring print(double value) {
			return strEx::itos(value);
		}
		static std::wstring print_unformated(double value) {
			return strEx::itos(value);
		}
		static std::wstring print_percent(double value) {
			return strEx::itos(value) + _T("%");
		}
		static std::wstring key_prefix() {
			return _T("");
		}
		static std::wstring key_postfix() {
			return _T("");
		}
	};

	typedef unsigned long state_type;
	const int state_none	  = 0x00;
	const int state_started   = 0x01;
	const int state_stopped   = 0x02;
	const int state_not_found = 0x06;
	const int state_hung      = 0x0e;

	class state_handler {
	public:
		static state_type parse(std::wstring s) {
			state_type ret = state_none;
			strEx::splitList lst = strEx::splitEx(s, _T(","));
			for (strEx::splitList::const_iterator it = lst.begin(); it != lst.end(); ++it) {
				if (*it == _T("started"))
					ret |= state_started;
				else if (*it == _T("stopped"))
					ret |= state_stopped;
				else if (*it == _T("ignored"))
					ret |= state_none;
				else if (*it == _T("not found"))
					ret |= state_not_found;
				else if (*it == _T("hung"))
					ret |= state_hung;
			}
			return ret;
		}
		static std::wstring print(state_type value) {
			if (value == state_started)
				return _T("started");
			else if (value == state_stopped)
				return _T("stopped");
			else if (value == state_none)
				return _T("none");
			else if (value == state_not_found)
				return _T("not found");
			else if (value == state_hung)
				return _T("hung");
			return _T("unknown");
		}
		static std::wstring print_unformated(state_type value) {
			return strEx::itos(value);
		}
	};


	template <typename TType = int, class THandler = int_handler >
	class NumericBounds {
	public:

		bool bHasBounds_;
		TType value_;
		typedef typename TType TValueType;
		typedef THandler TFormatType;

		NumericBounds() : bHasBounds_(false), value_(0) {};

		NumericBounds(const NumericBounds & other) {
			bHasBounds_ = other.bHasBounds_;
			value_ = other.value_;
		}

		void reset() {
			bHasBounds_ = false;
		}
		checkResultType check(TType value) const {
			if (value == value_)
				return same;
			else if (value > value_)
				return above;
			return below;
		}

		static std::wstring toStringLong(TType value) {
			return THandler::key_prefix() + THandler::print(value) + THandler::key_postfix();
		}
		static std::wstring toStringShort(TType value) {
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return bHasBounds_;
		}

		const NumericBounds & operator=(std::wstring value) {
			set(value);
			return *this;
		}

		TType getPerfBound(TType value) {
			return value_;
		}
		static std::wstring gatherPerfData(std::wstring alias, TType &value, TType warn, TType crit) {
			std::wstring unit = THandler::get_perf_unit(min(warn, min(crit, value)));
			return MAKE_PERFDATA(alias, THandler::print_perf(value, unit), unit, THandler::print_perf(warn, unit), THandler::print_perf(crit, unit));
		}
		static std::wstring gatherPerfData(std::wstring alias, TType &value) {
			std::wstring unit = THandler::get_perf_unit(value);
			return MAKE_PERFDATA_SIMPLE(alias, THandler::print_perf(value, unit), unit);
		}

	private:
		void set(std::wstring s) {
			value_ = THandler::parse(s);
			bHasBounds_ = true;
		}
	};


	template <typename TTypeValue, typename TTypeTotal = TTypeValue>
	struct PercentageValueType {
		typedef TTypeValue TValueType;
		TTypeValue value;
		TTypeTotal total;

		TTypeValue getUpperPercentage() {
			return 100-(value*100/total);
		}
		TTypeValue getLowerPercentage() {
			return (value*100)/total;
		}

		TTypeValue adjust_upper_magic(TTypeValue percentage, double normal, double magic) {
			if (magic == 0)
				return percentage;
			return 100 - ( (100 - percentage) * pow(total / normal, magic) / (value / normal) );
		}
		TTypeValue adjust_lower_magic(TTypeValue percentage, double normal, double magic) {
			if (magic == 0)
				return percentage;
			return ( (percentage) * pow(total / normal, magic) / (value / normal) );
		}


	};

	template <typename TType = int, class THandler = int_handler >
	class NumericPercentageBounds {
	public:
		typedef enum {
			none = 0,
			percentage_upper = 1,
			percentage_lower = 2,
			value_upper = 3,
			value_lower = 4,
		} checkTypes;

		class InternalValue {
			NumericPercentageBounds *pParent_;
			bool isUpper_;
		public:

			InternalValue(bool isUpper) : pParent_(NULL), isUpper_(isUpper) {}
			void setParent(NumericPercentageBounds *pParent) {
				pParent_ = pParent;
			}
			const InternalValue & operator=(std::wstring value) {
				std::wstring::size_type p = value.find_first_of('%');
				if (p != std::wstring::npos) {
					if (isUpper_)
						pParent_->setPercentageUpper(value.substr(0, p));
					else
						pParent_->setPercentageLower(value.substr(0, p));
				} else {
					if (isUpper_)
						pParent_->setUpper(value);
					else
						pParent_->setLower(value);
				}
				return *this;
			}

		};

		typedef typename TType TValueType;
		typedef THandler TFormatType;
		typedef NumericPercentageBounds<TType, THandler> TMyType;

		checkTypes type_;
		typename TType::TValueType value_;
		InternalValue upper;
		InternalValue lower;
		double normal_size;
		double magic;

		NumericPercentageBounds() : type_(none), value_(0), upper(true), lower(false), normal_size(20*1024*1024), magic(0) {
			upper.setParent(this);
			lower.setParent(this);
		}

		NumericPercentageBounds(const NumericPercentageBounds &other) 
			: type_(other.type_)
			, value_(other.value_)
			, upper(other.upper)
			, lower(other.lower)
			, normal_size(other.normal_size)
			, magic(other.magic)
		{
			upper.setParent(this);
			lower.setParent(this);

		}

		NumericPercentageBounds& operator =(const NumericPercentageBounds &other) {
			type_ = other.type_;
			value_ = other.value_;
			upper = other.upper;
			lower = other.lower;
			normal_size = other.normal_size;
			magic = other.magic;
			upper.setParent(this);
			lower.setParent(this);
			return *this;
		}
		void reset() {
			type_ = none;
		}
		checkResultType check(TType value) const {
			if (type_ == percentage_lower) {
				if (value.getLowerPercentage() == value.adjust_lower_magic(value_, normal_size, magic))
					return same;
				else if (value.getLowerPercentage() > value.adjust_lower_magic(value_, normal_size, magic))
					return above;
			} else if (type_ == percentage_upper) {
				if (value.getUpperPercentage() == value.adjust_upper_magic(value_, normal_size, magic))
					return same;
				else if (value.getUpperPercentage() > value.adjust_upper_magic(value_, normal_size, magic))
					return above;
			} else if (type_ == value_lower) {
				if (value.value == value_)
					return same;
				else if (value.value > value_)
					return above;
			} else if (type_ == value_upper) {
				if ((value.total-value.value) == value_)
					return same;
				else if ((value.total-value.value) > value_)
					return above;
			} else {
				std::cout << _T("Damn...: ") << type_ << std::endl;
				throw _T("Damn...");
			}
			return below;
		}
		static std::wstring toStringShort(TType value) {
			return THandler::print(value.value);

		}
		static std::wstring toStringLong(TType value) {
			return 
				THandler::key_total() + THandler::print(value.total) + 
				_T(" - ") + THandler::key_lower() + THandler::print(value.value) + 
				_T(" (") + THandler::print_percent(value.getLowerPercentage()) + _T(")") +
				_T(" - ") + THandler::key_upper() + THandler::print(value.total-value.value) + 
				_T(" (") + THandler::print_percent(value.getUpperPercentage()) + _T(")");
		}
		inline bool hasBounds() const {
			return type_ != none;
		}
		typename TType::TValueType getPerfBound(TType value) {
			return value_;
		}
		void set_magic(double magic_) {
			magic = magic_;
		}

		template<class T>
		T evaluate_percentage_to_value(T total, T threshold_percentage) {
			return total*threshold_percentage/100.0;
		}
		template<class T>
		T evaluate_value_to_percentage(T total, T threshold_percentage) {
			return 100-(threshold_percentage*100.0/total);
		}

		std::wstring gatherPerfData(std::wstring alias, TType &value, typename TType::TValueType warn, typename TType::TValueType crit) {
			unsigned int value_p, warn_p, crit_p;
			TType::TValueType warn_v, crit_v;
			if (type_ == percentage_upper) {
				value_p = static_cast<unsigned int>(value.getUpperPercentage());
				warn_p = static_cast<unsigned int>(warn);
				crit_p = static_cast<unsigned int>(crit);
				warn_v = evaluate_percentage_to_value(value.total, warn);
				crit_v = evaluate_percentage_to_value(value.total, crit);
			} else if (type_ == percentage_lower) {
				value_p = static_cast<unsigned int>(value.getLowerPercentage());
				warn_p = static_cast<unsigned int>(warn);
				crit_p = static_cast<unsigned int>(crit);
				warn_v = evaluate_percentage_to_value(value.total, warn);
				crit_v = evaluate_percentage_to_value(value.total, crit);
			} else if (type_ == value_upper) {
				value_p = static_cast<unsigned int>(value.getUpperPercentage());
				warn_p = evaluate_value_to_percentage(value.total, warn);
				crit_p = evaluate_value_to_percentage(value.total, crit);
				warn_v = warn;
				crit_v = crit;
			} else {
				value_p = static_cast<unsigned int>(value.getLowerPercentage());
				warn_p = evaluate_value_to_percentage(value.total, warn);
				crit_p = evaluate_value_to_percentage(value.total, crit);
				warn_v = warn;
				crit_v = crit;
			}
			std::wstring unit = THandler::get_perf_unit(min_no_zero(warn_v, crit_v, value.value));
			return 
				MAKE_PERFDATA(alias + _T(" %"), THandler::print_unformated(value_p), _T("%"), THandler::print_unformated(warn_p), THandler::print_unformated(crit_p))
				+ _T(" ") +
				MAKE_PERFDATA_EX(alias, THandler::print_perf(value.value, unit), unit, THandler::print_perf(warn_v, unit), THandler::print_perf(crit_v, unit), 
					THandler::print_perf(0, unit), THandler::print_perf(value.total, unit))
				;
		}
		template<class T>
		T min_no_zero(T v1, T v2, T v3) {
			if (v1 == 0 && v2 == 0 && v3 == 0)
				return 0;
			T maximum = max(v1, max(v2, v3));
			if (v1 == 0)
				v1 = maximum;
			if (v2 == 0)
				v2 = maximum;
			if (v3 == 0)
				v3 = maximum;
			return min(v1, min(v2, v3));
		}
		std::wstring gatherPerfData(std::wstring alias, TType &value) {
			unsigned int value_p;
			if (type_ == percentage_upper) {
				value_p = static_cast<unsigned int>(value.getUpperPercentage());
			} else if (type_ == percentage_lower) {
				value_p = static_cast<unsigned int>(value.getLowerPercentage());
			} else if (type_ == value_upper) {
				value_p = static_cast<unsigned int>(value.getUpperPercentage());
			} else {
				value_p = static_cast<unsigned int>(value.getLowerPercentage());
			}
			std::wstring unit = THandler::get_perf_unit(value.value);
			return 
				MAKE_PERFDATA_SIMPLE(alias + _T(" %"), THandler::print_unformated(value_p), _T("%"))
				+ _T(" ") +
				MAKE_PERFDATA_SIMPLE(alias, THandler::print_perf(value.value, unit), unit)
				;
		}
	private:
		void setUpper(std::wstring s) {
			value_ = THandler::parse(s);
			type_ = value_upper;
		}
		void setLower(std::wstring s) {
			value_ = THandler::parse(s);
			type_ = value_lower;
		}
		void setPercentageUpper(std::wstring s) {
			value_ = THandler::parse_percent(s);
			type_ = percentage_upper;
		}
		void setPercentageLower(std::wstring s) {
			value_ = THandler::parse_percent(s);
			type_ = percentage_lower;
		}
	};

	template <typename TType = state_type, class THandler = state_handler >
	class StateBounds {
	public:
		TType value_;
		typedef typename TType TValueType;
		typedef THandler TFormatType;
		typedef StateBounds<TType, THandler> TMyType;

		StateBounds() : value_(state_none) {}
		StateBounds(const StateBounds &other) : value_(other.value_) {}

		void reset() {
			value_ = state_none;
		}
		bool check(TType value) const {
			return (value & value_) != 0;
		}
		static std::wstring toStringLong(TType value) {
			return THandler::print(value);
		}
		static std::wstring toStringShort(TType value) {
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return value_ != state_none;
		}
		TType getPerfBound(TType value) {
			return value_;
		}
		std::wstring gatherPerfData(std::wstring alias, TType &value, TType warn, TType crit) {
			return "";
		}
		std::wstring gatherPerfData(std::wstring alias, TType &value) {
			return "";
		}
		const StateBounds & operator=(std::wstring value) {
			set(value);
			return *this;
		}
	private:
		void set(std::wstring s) {
			value_ = THandler::parse(s);
		}
	};

	template <typename TMaxMinType = int, typename TStateType = state_type>
	struct MaxMinStateValueType {
		TMaxMinType count;
		TStateType state;
	};


	template <class TValueType = MaxMinStateValueType, class TNumericHolder = NumericBounds<int, int_handler>, class TStateHolder = StateBounds<state_type, state_handler> >
	class MaxMinStateBounds {
	public:
		TNumericHolder max_;
		TNumericHolder min_;
		TStateHolder state;
		typedef MaxMinStateBounds<TValueType, TNumericHolder, TStateHolder > TMyType;

		typedef typename TValueType TValueType;

		MaxMinStateBounds() {}
		MaxMinStateBounds(const MaxMinStateBounds &other) : state(other.state), max_(other.max_), min_(other.min_) {}
		MaxMinStateBounds &	operator =(const MaxMinStateBounds &other) {
			state = other.state;
			max_ = other.max_;
			min_ = other.min_;
			return *this;
		}
		void reset() {
			state.reset();
			max_.reset();
			min_.reset();
		}
		bool hasBounds() {
			return state.hasBounds() ||  max_.hasBounds() || min_.hasBounds();
		}

		static std::wstring toStringLong(typename TValueType &value) {
			return TNumericHolder::toStringLong(value.count) + _T(", ") + TStateHolder::toStringLong(value.state);
		}
		static std::wstring toStringShort(typename TValueType &value) {
			return TNumericHolder::toStringShort(value.count);
		}
/*
		void formatString(std::wstring &message, typename TValueType &value) {
			if (state.hasBounds())
				message = state.toString(value.state);
			else if (max.hasBounds())
				message = max.toString(value.count);
			else if (min.hasBounds())
				message = max.toString(value.count);
		}
		*/
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			if (max_.hasBounds()) {
				return max_.gatherPerfData(alias, value.count, warn.max_.getPerfBound(value.count), crit.max_.getPerfBound(value.count));
			} else if (min_.hasBounds()) {
				return min_.gatherPerfData(alias, value.count, warn.min_.getPerfBound(value.count), crit.min_.getPerfBound(value.count));
			} else if (state.hasBounds()) {
				return min_.gatherPerfData(alias, value.count, 0, 0);
			}
			return _T("");
		}
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value) {
			return _T("");
		}
		bool check(typename TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value.state))) {
				message = lable + _T(": ") + formatState(TStateHolder::toStringShort(value.state), type);
				return true;
			} else if ((max_.hasBounds())&&(max_.check(value.count) != below)) {
				message = lable + _T(": ") + formatAbove(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else if ((min_.hasBounds())&&(min_.check(value.count) != above)) {
				message = lable + _T(": ") + formatBelow(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else {
				NSC_LOG_MESSAGE_STD(_T("Missing bounds for check: ") + lable);
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
		}

	};

	template <class TFilterType>
	class FilterBounds {
	public:
		TFilterType filter;
		typedef typename TFilterType::TValueType TValueType;
		typedef FilterBounds<TFilterType> TMyType;

		FilterBounds() {}
		FilterBounds(const FilterBounds &other) {
			filter = other.filter;
		}
		void reset() {
			filter.reset();
		}
		bool hasBounds() {
			return filter.hasFilter();
		}

		static std::wstring toStringLong(typename TValueType &value) {
			//return filter.to_string() + _T(" matches ") + value;
			// TODO FIx this;
			return value;
			//return TNumericHolder::toStringLong(value.count) + _T(", ") + TStateHolder::toStringLong(value.state);
		}
		static std::wstring toStringShort(typename TValueType &value) {
			// TODO FIx this;
			return value;
			//return TNumericHolder::toStringShort(value.count);
		}
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			return _T("");
		}
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value) {
			return _T("");
		}
		bool check(typename TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if (filter.hasFilter()) {
				if (!filter.matchFilter(value))
					return false;
				message = lable + _T(": ") + filter.to_string() + _T(" matches ") + value;
				return true;
			} else {
				NSC_LOG_MESSAGE_STD(_T("Missing bounds for filter check: ") + lable);
			}
			return false;
		}
		const TMyType & operator=(std::wstring value) {
			filter = value;
			return *this;
		}

	};


	template <class TStateHolder = StateBounds<state_type, state_handler> >
	class SimpleStateBounds {
	public:
		TStateHolder state;
		typedef SimpleStateBounds<TStateHolder > TMyType;

		typedef typename TStateHolder::TValueType TValueType;

		SimpleStateBounds() {}
		SimpleStateBounds(const SimpleStateBounds &other) {
			state = other.state;
		}
		void reset() {
			state.reset();
		}
		bool hasBounds() {
			return state.hasBounds();
		}
		static std::wstring toStringLong(typename TValueType &value) {
			return TStateHolder::toStringLong(value);
		}
		static std::wstring toStringShort(typename TValueType &value) {
			return TStateHolder::toStringShort(value);
		}
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			if (state.hasBounds()) {
				// @todo
			}
			return _T("");
		}
		std::wstring gatherPerfData(std::wstring alias, typename TValueType &value) {
			return _T("");
		}
		bool check(typename TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value))) {
				message = lable + _T(": ") + formatState(TStateHolder::toStringLong(value), type);
				return true;
			}
			return false;
		}

	};

	template <class THolder = NumericBounds<int, int_handler> >
	class MaxMinBounds {
	public:
		THolder max_;
		THolder min_;
		typedef MaxMinBounds<THolder > TMyType;

		typedef typename THolder::TValueType TValueType;
		//		typedef THolder::TFormatType TFormatType;

		MaxMinBounds() {}
		MaxMinBounds(const MaxMinBounds &other) : max_(other.max_), min_(other.min_) {}
		MaxMinBounds& operator=(const MaxMinBounds &other) {
			max_ = other.max_;
			min_ = other.min_;
			return *this;
		}
		void reset() {
			max_.reset();
			min_.reset();
		}
		bool hasBounds() {
			return max_.hasBounds() || min_.hasBounds();
		}
		static std::wstring toStringLong(typename THolder::TValueType &value) {
			return THolder::toStringLong(value);
		}
		static std::wstring toStringShort(typename THolder::TValueType &value) {
			return THolder::toStringShort(value);
		}
		std::wstring gatherPerfData(std::wstring alias, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max_.hasBounds()) {
				return max_.gatherPerfData(alias, value, warn.max_.getPerfBound(value), crit.max_.getPerfBound(value));
			} else if (min_.hasBounds()) {
				return min_.gatherPerfData(alias, value, warn.min_.getPerfBound(value), crit.min_.getPerfBound(value));
			} else {
				NSC_LOG_MESSAGE_STD(_T("Missing bounds for maxmin-bounds check: ") + alias);
				return min_.gatherPerfData(alias, value, 0, 0);
			}
			return _T("");
		}
		std::wstring gatherPerfData(std::wstring alias, typename THolder::TValueType &value) {
			THolder tmp;
			return tmp.gatherPerfData(alias, value);
		}
		bool check(typename THolder::TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((max_.hasBounds())&&(max_.check(value) != below)) {
				message = lable + _T(": ") + formatAbove(THolder::toStringLong(value), type);
				return true;
			} else if ((min_.hasBounds())&&(min_.check(value) != above)) {
				message = lable + _T(": ") + formatBelow(THolder::toStringLong(value), type);
				return true;
			} else {
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
		}

	};
	typedef MaxMinBounds<NumericBounds<double, double_handler> > MaxMinBoundsDouble;
	typedef MaxMinBounds<NumericBounds<__int64, int64_handler> > MaxMinBoundsInt64;
	typedef MaxMinBounds<NumericBounds<int, int_handler> > MaxMinBoundsInteger;
	typedef MaxMinBounds<NumericBounds<unsigned int, int_handler> > MaxMinBoundsUInteger;
	typedef MaxMinBounds<NumericBounds<unsigned long int, int_handler> > MaxMinBoundsULongInteger;
	typedef MaxMinBounds<NumericBounds<disk_size_type, disk_size_handler<disk_size_type> > > MaxMinBoundsDiscSize;
	typedef MaxMinBounds<NumericBounds<time_type, time_handler<time_type> > > MaxMinBoundsTime;


	template <class THolder = NumericBounds<int, int_handler> >
	class ExactBounds {
	public:
		THolder max;
		THolder min;
		THolder eq;
		THolder neq;
		typedef ExactBounds<THolder > TMyType;
		typedef typename THolder::TValueType TValueType;

		ExactBounds() {}
		ExactBounds(const ExactBounds &other) {
			max = other.max;
			min = other.min;
			eq = other.eq;
			neq = other.neq;
		}

		const TMyType& operator=(std::wstring value) {
			//value_ = value;
			if (value.substr(0,1) == _T(">")) {
				max = value.substr(1);
			} else if (value.substr(0,2) == _T("<>")) {
				neq = value.substr(2);
			} else if (value.substr(0,1) == _T("<")) {
				min = value.substr(1);
			} else if (value.substr(0,1) == _T("=")) {
				eq = value.substr(1);
			} else if (value.substr(0,2) == _T("!=")) {
				neq = value.substr(2);
			} else if (value.substr(0,1) == _T("!")) {
				neq = value.substr(1);
				/*
				TODO add support for lists
			} else if (value.substr(0,3) == _T("in:")) {
				inList = value.substr(3);
				*/
			} else if (value.substr(0,3) == _T("gt:")) {
				max = value.substr(3);
			} else if (value.substr(0,3) == _T("lt:")) {
				min = value.substr(3);
			} else if (value.substr(0,3) == _T("ne:")) {
				neq = value.substr(3);
			} else if (value.substr(0,3) == _T("eq:")) {
				eq = value.substr(3);
			} else {
				throw parse_exception(_T("Unknown filter key: ") + value + _T(" (numeric filters have to have an operator as well ie. foo=>5 or bar==5 foo=gt:6)"));
			}
			return *this;
		}

		void reset() {
			max.reset();
			min.reset();
			eq.reset();
			neq.reset();
		}
		bool hasBounds() {
			return max.hasBounds() || min.hasBounds() || eq.hasBounds() || neq.hasBounds();
		}
		static std::wstring toStringLong(typename THolder::TValueType &value) {
			return THolder::toStringLong(value);
		}
		static std::wstring toStringShort(typename THolder::TValueType &value) {
			return THolder::toStringShort(value);
		}
		std::wstring gatherPerfData(std::wstring alias, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max.hasBounds()) {
				return max.gatherPerfData(alias, value, warn.max.getPerfBound(value), crit.max.getPerfBound(value));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, value, warn.min.getPerfBound(value), crit.min.getPerfBound(value));
			} else if (neq.hasBounds()) {
				return neq.gatherPerfData(alias, value, warn.neq.getPerfBound(value), crit.neq.getPerfBound(value));
			} else if (eq.hasBounds()) {
				return eq.gatherPerfData(alias, value, warn.eq.getPerfBound(value), crit.eq.getPerfBound(value));
			} else {
				NSC_LOG_MESSAGE_STD(_T("Missing bounds for: ") + alias);
				return _T("");
			}
		}
		std::wstring gatherPerfData(std::wstring alias, typename THolder::TValueType &value) {
			THolder tmp;
			return tmp.gatherPerfData(alias, value);
		}
		bool check(typename THolder::TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			return check_preformatted(value, THolder::toStringLong(value), lable, message, type);
		}
		bool check_preformatted(typename THolder::TValueType &value, std::wstring formatted_value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) == above)) {
				message = lable + _T(": ") + formatAbove(formatted_value, type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) == below)) {
				message = lable + _T(": ") + formatBelow(formatted_value, type);
				return true;
			} else if ((eq.hasBounds())&&(eq.check(value) == same)) {
				message = lable + _T(": ") + formatSame(formatted_value, type);
				return true;
			} else if ((neq.hasBounds())&&(neq.check(value) != same)) {
				message = lable + _T(": ") + formatNotSame(formatted_value, type);
				return true;
			} else {
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
		}
	};
	typedef ExactBounds<NumericBounds<unsigned long int, int_handler> > ExactBoundsULongInteger;
	typedef ExactBounds<NumericBounds<unsigned int, int_handler> > ExactBoundsUInteger;
	typedef ExactBounds<NumericBounds<unsigned long, int_handler> > ExactBoundsULong;
	typedef ExactBounds<NumericBounds<long long, int_handler> > ExactBoundsLongLong;
	typedef ExactBounds<NumericBounds<time_type, time_handler<__int64> > > ExactBoundsTime;

	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<int ,int>, int_handler> > MaxMinPercentageBoundsInteger;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, int64_handler> > MaxMinPercentageBoundsInt64;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<double, double>, double_handler> > MaxMinPercentageBoundsDouble;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<disk_size_type, disk_size_type>, disk_size_handler<> > > MaxMinPercentageBoundsDiskSize;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, disk_size_handler<__int64> > > MaxMinPercentageBoundsDiskSizei64;

	typedef MaxMinStateBounds<MaxMinStateValueType<int, state_type>, NumericBounds<int, int_handler>, StateBounds<state_type, state_handler> > MaxMinStateBoundsStateBoundsInteger;
	typedef SimpleStateBounds<StateBounds<state_type, state_handler> > SimpleBoundsStateBoundsInteger;
}


