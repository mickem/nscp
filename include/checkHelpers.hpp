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

#define MAKE_PERFDATA(alias, value, unit, warn, crit) _T("'") + alias + _T("'=") + value + unit + _T(";") + warn + _T(";") + crit + _T("; ")

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
		TContents warn;
		TContents crit;
		std::wstring data;
		std::wstring alias;

		showType show;
		bool perfData;


		CheckContainer() : show(showUnknown), perfData(true)
		{}
		CheckContainer(std::wstring data_, TContents warn_, TContents crit_) 
			: data(data_), warn(warn_), crit(crit_), show(showUnknown) 
		{}
		CheckContainer(std::wstring name_, std::wstring alias_, TContents warn_, TContents crit_) 
			: data(data_), alias(alias_), warn(warn_), crit(crit_), show(showUnknown) 
		{}
		CheckContainer(const TThisType &other) 
			: data(other.data), alias(other.alias), warn(other.warn), crit(other.crit), show(other.show) 
		{}
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
			return crit.gatherPerfData(getAlias(), value, warn, crit);
		}
		bool hasBounds() {
			return warn.hasBounds() || crit.hasBounds();
		}
		void runCheck(typename TContents::TValueType &value, NSCAPI::nagiosReturn &returnCode, std::wstring &message, std::wstring &perf) {
			std::wstring tstr;
			if (crit.check(value, getAlias(), tstr, critical)) {
				//std::wcout << _T("crit") << std::endl;
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else if (warn.check(value, getAlias(), tstr, warning)) {
				//std::wcout << _T("warn") << std::endl;
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			}else if (show == showLong) {
				//std::wcout << _T("long") << std::endl;
				tstr = getAlias() + _T(": ") + TContents::toStringLong(value);
			}else if (show == showShort) {
				//std::wcout << _T("short") << std::endl;
				tstr = getAlias() + _T(": ") + TContents::toStringShort(value);
			}
			if (perfData)
				perf += gatherPerfData(value);
			if (!message.empty() && !tstr.empty())
				message += _T(", ");
			if (!tstr.empty())
				message += tstr;
			//std::wcout << _T("result: ") << tstr << _T("--") << std::endl;
		}
	};

	typedef unsigned __int64 disk_size_type;
	template <typename TType = disk_size_type>
	class disk_size_handler {
	public:
		static std::wstring print(TType value) {
			return strEx::itos_as_BKMG(value);
		}
		static std::wstring get_perf_unit(TType value) {
			return strEx::find_proper_unit_BKMG(value);
		}
		static std::wstring print_perf(TType value, std::wstring unit) {
			return strEx::format_BKMG(value, unit);
		}
		static TType parse(std::wstring s) {
			return strEx::stoi64_as_BKMG(s);
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
			return strEx::stoi64_as_time(s);
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
			return strEx::stoi(s);
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
			return strEx::stoi64(s);
		}
		static __int64 parse_percent(std::wstring s) {
			return strEx::stoi(s);
		}
		static std::wstring print(__int64 value) {
			return strEx::itos(value);
		}
		static std::wstring get_perf_unit(__int64 value) {
			return _T("");
		}
		static std::wstring print_perf(__int64 value, std::wstring unit) {
			return strEx::itos(value);
		}
		static std::wstring print_unformated(__int64 value) {
			return strEx::itos(value);
		}
		static std::wstring print_percent(__int64 value) {
			return strEx::itos(value) + _T("%");
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
						pParent_->setPercentageUpper(value);
					else
						pParent_->setPercentageLower(value);
				} else {
					if (isUpper_)
						pParent_->setUpper(value);
					else
						pParent_->setLower(value);
				}
				return *this;
			}

		};

		checkTypes type_;
		typename TType::TValueType value_;
		typedef typename TType TValueType;
		typedef THandler TFormatType;
		typedef NumericPercentageBounds<TType, THandler> TMyType;
		InternalValue upper;
		InternalValue lower;

		NumericPercentageBounds() : type_(none), upper(true), lower(false) {
			upper.setParent(this);
			lower.setParent(this);
		}

		NumericPercentageBounds(const NumericPercentageBounds &other) {
			type_ = other.type_;
			value_ = other.value_;
		}
		checkResultType check(TType value) const {
			if (type_ == percentage_lower) {
				if (value.getLowerPercentage() == value_)
					return same;
				else if (value.getLowerPercentage() > value_)
					return above;
			} else if (type_ == percentage_upper) {
				if (value.getUpperPercentage() == value_)
					return same;
				else if (value.getUpperPercentage() > value_)
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
		std::wstring gatherPerfData(std::wstring alias, TType &value, typename TType::TValueType warn, typename TType::TValueType crit) {
			if (type_ == percentage_upper) {
				return 
					MAKE_PERFDATA(alias, THandler::print_unformated(value.getUpperPercentage()), _T("%"), 
					THandler::print_unformated(warn), THandler::print_unformated(crit));
			} else if (type_ == percentage_lower) {
					return 
						MAKE_PERFDATA(alias, THandler::print_unformated(value.getLowerPercentage()), _T("%"), 
						THandler::print_unformated(warn), THandler::print_unformated(crit));
			} else if (type_ == value_upper) {
				std::wstring unit = THandler::get_perf_unit(min(warn, min(crit, value.value)));
				return 
					MAKE_PERFDATA(alias, THandler::print_perf((value.value), unit), unit, 
					THandler::print_perf(value.total-warn, unit), THandler::print_perf(value.total-crit, unit));
			} else {
				std::wstring unit = THandler::get_perf_unit(min(warn, min(crit, value.value)));
				return 
					MAKE_PERFDATA(alias, THandler::print_perf(value.value, unit), unit, 
					THandler::print_perf(warn, unit), THandler::print_perf(crit, unit));
			}
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
		TNumericHolder max;
		TNumericHolder min;
		TStateHolder state;
		typedef MaxMinStateBounds<TValueType, TNumericHolder, TStateHolder > TMyType;

		typedef typename TValueType TValueType;

		MaxMinStateBounds() {}
		MaxMinStateBounds(const MaxMinStateBounds &other) {
			state = other.state;
			max = other.max;
			min = other.min;
		}
		bool hasBounds() {
			return state.hasBounds() ||  max.hasBounds() || min.hasBounds();
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
			if (state.hasBounds()) {
				// @todo
			} else if (max.hasBounds()) {
				return max.gatherPerfData(alias, value.count, warn.max.getPerfBound(value.count), crit.max.getPerfBound(value.count));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, value.count, warn.min.getPerfBound(value.count), crit.min.getPerfBound(value.count));
			}
			return _T("");
		}
		bool check(typename TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value.state))) {
				message = lable + _T(": ") + formatState(TStateHolder::toStringShort(value.state), type);
				return true;
			} else if ((max.hasBounds())&&(max.check(value.count) != below)) {
				message = lable + _T(": ") + formatAbove(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value.count) != above)) {
				message = lable + _T(": ") + formatBelow(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else {
				NSC_DEBUG_MSG_STD(_T("Missing bounds for check: ") + lable);
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
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
		THolder max;
		THolder min;
		typedef MaxMinBounds<THolder > TMyType;

		typedef typename THolder::TValueType TValueType;
		//		typedef THolder::TFormatType TFormatType;

		MaxMinBounds() {}
		MaxMinBounds(const MaxMinBounds &other) {
			max = other.max;
			min = other.min;
		}
		bool hasBounds() {
			return max.hasBounds() || min.hasBounds();
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
			} else {
				NSC_DEBUG_MSG_STD(_T("Missing bounds for maxmin-bounds check: ") + alias);
				return min.gatherPerfData(alias, value, 0, 0);
			}
			return _T("");
		}
		bool check(typename THolder::TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) != below)) {
				message = lable + _T(": ") + formatAbove(THolder::toStringLong(value), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) != above)) {
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
				NSC_DEBUG_MSG_STD(_T("Missing bounds for: ") + alias);
			}
		}
		bool check(typename THolder::TValueType &value, std::wstring lable, std::wstring &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) == above)) {
				message = lable + _T(": ") + formatAbove(THolder::toStringLong(value), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) == below)) {
				message = lable + _T(": ") + formatBelow(THolder::toStringLong(value), type);
				return true;
			} else if ((eq.hasBounds())&&(eq.check(value) == same)) {
				message = lable + _T(": ") + formatSame(THolder::toStringLong(value), type);
				return true;
			} else if ((neq.hasBounds())&&(neq.check(value) != same)) {
				message = lable + _T(": ") + formatNotSame(THolder::toStringLong(value), type);
				return true;
			} else {
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
		}

	};
	typedef ExactBounds<NumericBounds<unsigned long int, int_handler> > ExactBoundsULongInteger;

	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<int ,int>, int_handler> > MaxMinPercentageBoundsInteger;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, int64_handler> > MaxMinPercentageBoundsInt64;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<double, double>, double_handler> > MaxMinPercentageBoundsDouble;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<disk_size_type, disk_size_type>, disk_size_handler<> > > MaxMinPercentageBoundsDiskSize;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, disk_size_handler<__int64> > > MaxMinPercentageBoundsDiskSizei64;

	typedef MaxMinStateBounds<MaxMinStateValueType<int, state_type>, NumericBounds<int, int_handler>, StateBounds<state_type, state_handler> > MaxMinStateBoundsStateBoundsInteger;
	typedef SimpleStateBounds<StateBounds<state_type, state_handler> > SimpleBoundsStateBoundsInteger;
}


