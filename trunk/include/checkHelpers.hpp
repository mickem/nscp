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

#define MAKE_PERFDATA(alias, value, unit, warn, crit) "'" + alias + "'=" + value + unit + ";" + warn + ";" + crit + "; "

namespace checkHolders {


	typedef enum { warning, critical} ResultType;
	typedef enum { above = 1, below = -1, same = 0 } checkResultType;



	static std::string formatAbove(std::string str, ResultType what) {
		if (what == warning)
			return str + " > warning";
		else if (what == critical)
			return str + " > critical";
		return str + " > unknown";
	}

	static std::string formatBelow(std::string str, ResultType what) {
		if (what == warning)
			return str + " < warning";
		else if (what == critical)
			return str + " < critical";
		return str + " < unknown";
	}
	std::string formatState(std::string str, ResultType what) {
		if (what == warning)
			return str + " (warning)";
		else if (what == critical)
			return str + " (critical)";
		return str + " (unknown)";
	}
	std::string formatNotFound(std::string str, ResultType what) {
		if (what == warning)
			return str + "not found (warning)";
		else if (what == critical)
			return str + "not found (critical)";
		return str + "not found (unknown)";
	}

	typedef enum {showLong, showShort, showProblems, showUnknown} showType;
	template <class TContents>
	struct CheckConatiner {
		typedef CheckConatiner<TContents> TThisType;
		TContents warn;
		TContents crit;
		std::string data;
		std::string alias;

		showType show;
		bool perfData;


		CheckConatiner() : show(showUnknown), perfData(true)
		{}
		CheckConatiner(std::string data_, TContents warn_, TContents crit_) 
			: data(data_), warn(warn_), crit(crit_), show(showUnknown) 
		{}
		CheckConatiner(std::string name_, std::string alias_, TContents warn_, TContents crit_) 
			: data(data_), alias(alias_), warn(warn_), crit(crit_), show(showUnknown) 
		{}
		CheckConatiner(const TThisType &other) 
			: data(other.data), alias(other.alias), warn(other.warn), crit(other.crit), show(other.show) 
		{}
		std::string getAlias() {
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
		std::string gatherPerfData(typename TContents::TValueType &value) {
			return crit.gatherPerfData(getAlias(), value, warn, crit);
		}
		void runCheck(typename TContents::TValueType &value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf) {
			std::string tstr;
			if (crit.check(value, getAlias(), tstr, critical)) {
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else if (warn.check(value, getAlias(), tstr, warning)) {
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			}else if (show == showLong) {
				tstr = getAlias() + ": " + TContents::toStringLong(value);
			}else if (show == showShort) {
				tstr = getAlias() + ": " + TContents::toStringShort(value);
			}
			if (perfData)
				perf += gatherPerfData(value);
			if (!message.empty() && !tstr.empty())
				message += ", ";
			if (!tstr.empty())
				message += tstr;
		}
	};

	typedef unsigned __int64 disk_size_type;
	template <typename TType = disk_size_type>
	class disk_size_handler {
	public:
		static std::string print(TType value) {
			return strEx::itos_as_BKMG(value);
		}
		static std::string print_perf(TType value) {
			return strEx::itos_as_BKMG(value);
		}
		static TType parse(std::string s) {
			return strEx::stoi64_as_BKMG(s);
		}
		static TType parse_percent(std::string s) {
			return strEx::stoi64(s);
		}
		static std::string print_percent(TType value) {
			return strEx::itos(value) + "%";
		}
		static std::string print_unformated(TType value) {
			return strEx::itos(value);
		}

		static std::string key_total() {
			return "Total: ";
		}
		static std::string key_lower() {
			return "Used: ";
		}
		static std::string key_upper() {
			return "Free: ";
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}

	};

	typedef unsigned __int64 time_type;
	template <typename TType = time_type>
	class time_handler {
	public:
		static TType parse(std::string s) {
			return strEx::stoi64_as_time(s);
		}
		static TType parse_percent(std::string s) {
			return strEx::stoi(s);
		}
		static std::string print(TType value) {
			return strEx::itos_as_time(value);
		}
		static std::string print_percent(TType value) {
			return strEx::itos(value) + "%";
		}
		static std::string print_unformated(TType value) {
			return strEx::itos(value);
		}
		static std::string print_perf(TType value) {
			return strEx::itos(value);
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}

	};


	class int_handler {
	public:
		static int parse(std::string s) {
			return strEx::stoi(s);
		}
		static int parse_percent(std::string s) {
			return strEx::stoi(s);
		}
		static std::string print(int value) {
			return strEx::itos(value);
		}
		static std::string print_perf(int value) {
			return strEx::itos(value);
		}
		static std::string print_unformated(int value) {
			return strEx::itos(value);
		}
		static std::string print_percent(int value) {
			return strEx::itos(value) + "%";
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}
	};
	class int64_handler {
	public:
		static __int64 parse(std::string s) {
			return strEx::stoi64(s);
		}
		static __int64 parse_percent(std::string s) {
			return strEx::stoi(s);
		}
		static std::string print(__int64 value) {
			return strEx::itos(value);
		}
		static std::string print_perf(__int64 value) {
			return strEx::itos(value);
		}
		static std::string print_unformated(__int64 value) {
			return strEx::itos(value);
		}
		static std::string print_percent(__int64 value) {
			return strEx::itos(value) + "%";
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}
	};
	class double_handler {
	public:
		static double parse(std::string s) {
			return strEx::stod(s);
		}
		static double parse_percent(std::string s) {
			return strEx::stod(s);
		}
		static std::string print_perf(double value) {
			return strEx::itos(value);
		}
		static std::string print(double value) {
			return strEx::itos(value);
		}
		static std::string print_unformated(double value) {
			return strEx::itos(value);
		}
		static std::string print_percent(double value) {
			return strEx::itos(value) + "%";
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}
	};

	typedef unsigned long state_type;
	const int state_none	= 0x00;
	const int state_started = 0x01;
	const int state_stopped = 0x02;

	class state_handler {
	public:
		static state_type parse(std::string s) {
			state_type ret = state_none;
			strEx::splitList lst = strEx::splitEx(s, ",");
			for (strEx::splitList::const_iterator it = lst.begin(); it != lst.end(); ++it) {
				if (*it == "started")
					ret |= state_started;
				else if (*it == "stopped")
					ret |= state_stopped;
			}
			return ret;
		}
		static std::string print(state_type value) {
			if (value == state_started)
				return "started";
			else if (value == state_stopped)
				return "stopped";
			return "unknown";
		}
		static std::string print_unformated(state_type value) {
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

		static std::string toStringLong(TType value) {
			return THandler::key_prefix() + THandler::print(value) + THandler::key_postfix();
		}
		static std::string toStringShort(TType value) {
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return bHasBounds_;
		}

		const NumericBounds & operator=(std::string value) {
			set(value);
			return *this;
		}

		TType getPerfBound(TType value) {
			return value_;
		}
		static std::string gatherPerfData(std::string alias, TType &value, TType warn, TType crit) {
			return MAKE_PERFDATA(alias, THandler::print_perf(value), "", THandler::print_perf(warn), THandler::print_perf(crit));
		}

	private:
		void set(std::string s) {
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
			const InternalValue & operator=(std::string value) {
				std::string::size_type p = value.find_first_of('%');
				if (p != std::string::npos) {
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
				std::cout << "Damn...: " << type_ << std::endl;
				throw "Damn...";
			}
			return below;
		}
		static std::string toStringShort(TType value) {
			return THandler::print(value.value);

		}
		static std::string toStringLong(TType value) {
			return 
				THandler::key_total() + THandler::print(value.total) + 
				" - " + THandler::key_lower() + THandler::print(value.value) + 
					" (" + THandler::print_percent(value.getLowerPercentage()) + ")" +
				" - " + THandler::key_upper() + THandler::print(value.total-value.value) + 
					" (" + THandler::print_percent(value.getUpperPercentage()) + ")";
		}
		inline bool hasBounds() const {
			return type_ != none;
		}
		typename TType::TValueType getPerfBound(TType value) {
			return value_;
		}
		std::string gatherPerfData(std::string alias, TType &value, typename TType::TValueType warn, typename TType::TValueType crit) {
			if (type_ == percentage_upper) {
				return 
					MAKE_PERFDATA(alias, THandler::print_unformated(value.getUpperPercentage()), "%", 
					THandler::print_unformated(warn), THandler::print_unformated(crit));
			} else if (type_ == percentage_lower) {
					return 
						MAKE_PERFDATA(alias, THandler::print_unformated(value.getLowerPercentage()), "%", 
						THandler::print_unformated(warn), THandler::print_unformated(crit));
			} else {
				return 
					MAKE_PERFDATA(alias, THandler::print_perf(value.value), "", 
					THandler::print_perf(warn), THandler::print_perf(crit));
			}
		}
	private:
		void setUpper(std::string s) {
			value_ = THandler::parse(s);
			type_ = value_upper;
		}
		void setLower(std::string s) {
			value_ = THandler::parse(s);
			type_ = value_lower;
		}
		void setPercentageUpper(std::string s) {
			value_ = THandler::parse_percent(s);
			type_ = percentage_upper;
		}
		void setPercentageLower(std::string s) {
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
		static std::string toStringLong(TType value) {
			return THandler::print(value);
		}
		static std::string toStringShort(TType value) {
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return value_ != state_none;
		}
		TType getPerfBound(TType value) {
			return value_;
		}
		std::string gatherPerfData(std::string alias, TType &value, TType warn, TType crit) {
			return "";
		}
		const StateBounds & operator=(std::string value) {
			set(value);
			return *this;
		}
	private:
		void set(std::string s) {
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

		static std::string toStringLong(typename TValueType &value) {
			return TNumericHolder::toStringLong(value.count) + ", " + TStateHolder::toStringLong(value.state);
		}
		static std::string toStringShort(typename TValueType &value) {
			return TNumericHolder::toStringShort(value.count);
		}
/*
		void formatString(std::string &message, typename TValueType &value) {
			if (state.hasBounds())
				message = state.toString(value.state);
			else if (max.hasBounds())
				message = max.toString(value.count);
			else if (min.hasBounds())
				message = max.toString(value.count);
		}
		*/
		std::string gatherPerfData(std::string alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			if (state.hasBounds()) {
				// @todo
			} else if (max.hasBounds()) {
				return max.gatherPerfData(alias, value.count, warn.max.getPerfBound(value.count), crit.max.getPerfBound(value.count));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, value.count, warn.min.getPerfBound(value.count), crit.min.getPerfBound(value.count));
			}
			return "";
		}
		bool check(typename TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value.state))) {
				message = lable + ": " + formatState(TStateHolder::toStringShort(value.state), type);
				return true;
			} else if ((max.hasBounds())&&(max.check(value.count) != below)) {
				message = lable + ": " + formatAbove(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value.count) != above)) {
				message = lable + ": " + formatBelow(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else {
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
		static std::string toStringLong(typename TValueType &value) {
			return TStateHolder::toStringLong(value);
		}
		static std::string toStringShort(typename TValueType &value) {
			return TStateHolder::toStringShort(value);
		}
		std::string gatherPerfData(std::string alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			if (state.hasBounds()) {
				// @todo
			}
			return "";
		}
		bool check(typename TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value))) {
				message = lable + ": " + formatState(TStateHolder::toStringLong(value), type);
				return true;
			} else {
				//std::cout << "No bounds specified..." << std::endl;
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
		static std::string toStringLong(typename THolder::TValueType &value) {
			return THolder::toStringLong(value);
		}
		static std::string toStringShort(typename THolder::TValueType &value) {
			return THolder::toStringShort(value);
		}
		std::string gatherPerfData(std::string alias, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max.hasBounds()) {
				return max.gatherPerfData(alias, value, warn.max.getPerfBound(value), crit.max.getPerfBound(value));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, value, warn.min.getPerfBound(value), crit.min.getPerfBound(value));
			} else {
				NSC_DEBUG_MSG_STD("Missing bounds for maxmin-bounds check: " + alias);
			}
			return "";
		}
		bool check(typename THolder::TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) != below)) {
				message = lable + ": " + formatAbove(THolder::toStringLong(value), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) != above)) {
				message = lable + ": " + formatBelow(THolder::toStringLong(value), type);
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


	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<int ,int>, int_handler> > MaxMinPercentageBoundsInteger;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, int64_handler> > MaxMinPercentageBoundsInt64;
	//typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<double, double>, double_handler> > MaxMinPercentageBoundsDouble;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<disk_size_type, disk_size_type>, disk_size_handler<> > > MaxMinPercentageBoundsDiskSize;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<__int64, __int64>, disk_size_handler<__int64> > > MaxMinPercentageBoundsDiskSizei64;

	typedef MaxMinStateBounds<MaxMinStateValueType<int, state_type>, NumericBounds<int, int_handler>, StateBounds<state_type, state_handler> > MaxMinStateBoundsStateBoundsInteger;
	typedef SimpleStateBounds<StateBounds<state_type, state_handler> > SimpleBoundsStateBoundsInteger;
}

