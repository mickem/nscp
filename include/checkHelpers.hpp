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

#include <nscapi/nscapi_helper.hpp>

#define MAKE_PERFDATA_SIMPLE(alias, value, unit) "'" + alias + "'=" + value + unit
#define MAKE_PERFDATA(alias, value, unit, warn, crit) "'" + alias + "'=" + value + unit + ";" + warn + ";" + crit 
#define MAKE_PERFDATA_EX(alias, value, unit, warn, crit, xmin, xmax) "'" + alias + "'=" + value + unit + ";" + warn + ";" + crit + ";" + xmin + ";" + xmax

namespace checkHolders {



	typedef enum { warning, critical} ResultType;
	typedef enum { above = 1, below = -1, same = 0 } checkResultType;
	class check_exception {
		std::string error_;
	public:
		check_exception(std::string error) : error_(error) {}
		std::string getMessage() {
			return error_;
		}
	};

	struct parse_exception : public check_exception {
		parse_exception(std::string error) : check_exception(error) {}
	};

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
	static std::string formatSame(std::string str, ResultType what) {
		if (what == warning)
			return str + " = warning";
		else if (what == critical)
			return str + " = critical";
		return str + " = unknown";
	}
	static std::string formatNotSame(std::string str, ResultType what) {
		if (what == warning)
			return str + " != warning";
		else if (what == critical)
			return str + " != critical";
		return str + " != unknown";
	}
	static std::string formatState(std::string str, ResultType what) {
		if (what == warning)
			return str + " (warning)";
		else if (what == critical)
			return str + " (critical)";
		return str + " (unknown)";
	}
	static std::string formatNotFound(std::string str, ResultType what) {
		if (what == warning)
			return str + "not found (warning)";
		else if (what == critical)
			return str + "not found (critical)";
		return str + "not found (unknown)";
	}


	typedef enum {showLong, showShort, showProblems, showUnknown} showType;
	template <class TContents>
	struct CheckContainer {
		typedef CheckContainer<TContents> TThisType;
		typedef typename TContents::TValueType TPayloadValueType;
		TContents warn;
		TContents crit;
		std::string data;
		std::string alias;
		std::string perf_unit;


		showType show;
		bool perfData;


		CheckContainer() : show(showUnknown), perfData(true)
		{}
		CheckContainer(std::string data_, TContents warn_, TContents crit_) 
			: data(data_), warn(warn_), crit(crit_), show(showUnknown), perfData(true)
		{}
		CheckContainer(std::string data_, std::string alias_, TContents warn_, TContents crit_) 
			: data(data_), alias(alias_), warn(warn_), crit(crit_), show(showUnknown), perfData(true)
		{}
		CheckContainer(const TThisType &other) 
			: data(other.data), alias(other.alias), warn(other.warn), crit(other.crit), show(other.show), perfData(other.perfData), perf_unit(other.perf_unit)

		{}
		CheckContainer<TContents>& operator =(const CheckContainer<TContents> &other) {
			warn = other.warn;
			crit = other.crit;
			data = other.data;
			alias = other.alias;
			show = other.show;
			perfData = other.perfData;
			perf_unit = other.perf_unit;
			return *this;
		}
		void reset() {
			warn.reset();
			crit.reset();
		}
		std::string getAlias() const {
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
			if (perf_unit.empty())
				perf_unit = def.perf_unit;
		}
		bool showAll() {
			return show != showProblems;
		}
		std::string gatherPerfData(typename TContents::TValueType &value) {
			if (crit.hasBounds())
				return crit.gatherPerfData(getAlias(), perf_unit, value, warn, crit);
			else if (warn.hasBounds())
				return warn.gatherPerfData(getAlias(), perf_unit, value, warn, crit);
			else {
				TContents tmp;
				return tmp.gatherPerfData(getAlias(), perf_unit, value);
			}
		}
		bool hasBounds() {
			return warn.hasBounds() || crit.hasBounds();
		}
		void runCheck(typename TContents::TValueType value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf) {
			std::string tstr;
			if (crit.check(value, getAlias(), tstr, critical)) {
				nscapi::plugin_helper::escalteReturnCodeToCRIT(returnCode);
			} else if (warn.check(value, getAlias(), tstr, warning)) {
				nscapi::plugin_helper::escalteReturnCodeToWARN(returnCode);
			}else if (show == showLong) {
				tstr = getAlias() + ": " + TContents::toStringLong(value);
			}else if (show == showShort) {
				tstr = getAlias() + ": " + TContents::toStringShort(value);
			}
			if (perfData) {
				if (!perf.empty())
					perf += " ";
				perf += gatherPerfData(value);
			}
			if (!message.empty() && !tstr.empty())
				message += ", ";
			if (!tstr.empty())
				message += tstr;
		}
	};

	template <class TContents>
	struct MagicCheckContainer : public CheckContainer<TContents> {
		typedef CheckContainer<TContents> tParent;

		MagicCheckContainer() : tParent() {}
		MagicCheckContainer(std::string data_, TContents warn_, TContents crit_) : tParent(data_, warn_, crit_) {}
		MagicCheckContainer(std::string data_, std::string alias_, TContents warn_, TContents crit_) : tParent(data_, alias_, warn_, crit_) {}
		MagicCheckContainer(const MagicCheckContainer<TContents> &other) : tParent(other) {}

		MagicCheckContainer<TContents>& operator =(const MagicCheckContainer<TContents> &other) {
			tParent::operator =(other);
			return *this;
		}


		void set_magic(double magic) {
			CheckContainer<TContents>::warn.max_.set_magic(magic);
			CheckContainer<TContents>::warn.min_.set_magic(magic);
			CheckContainer<TContents>::crit.max_.set_magic(magic);
			CheckContainer<TContents>::crit.min_.set_magic(magic);
		}

	};

	template <class value_type>
	struct check_proxy_interface {
		virtual bool showAll() = 0;
		virtual std::string gatherPerfData(value_type &value) = 0;
		virtual bool hasBounds() = 0;
		virtual void runCheck(value_type &value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf) = 0;
		virtual void set_warn_bound(std::string value) = 0;
		virtual void set_crit_bound(std::string value) = 0;
		virtual void set_showall(showType show_) = 0;
	};


	//typedef enum {showLong, showShort, showProblems, showUnknown} showType;
	template <class container_value_type, class impl_type>
	class check_proxy_container : public check_proxy_interface<container_value_type> {
		typedef check_proxy_container<container_value_type, impl_type> TThisType;
		impl_type impl_;
	public:
		virtual typename impl_type::TPayloadValueType get_value(container_value_type &value) = 0;

		void set_warn_bound(std::string value) {
			impl_.warn = value;
		}
		void set_crit_bound(std::string value) {
			impl_.crit = value;
		}
		void set_alias(std::string value) {
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
		std::string gatherPerfData(container_value_type &value) {
			typename impl_type::TPayloadValueType real_value = get_value(value);
			return impl_.gatherPerfData(real_value);
		}
		bool hasBounds() {
			return impl_.hasBounds();
		}
		void runCheck(container_value_type &value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf) {
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
		std::string data;
		std::string alias;

		std::string cached_warn_;
		std::string cached_crit_;

		showType show;
		bool perfData;

		void set_warn_bound(std::string value) {
// 			if (checks_.empty())
				cached_warn_ = value;
// 			else
// 				(checks_.back())->set_warn_bound(value);
		}
		void set_crit_bound(std::string value) {
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
			cached_warn_ = "";
			cached_crit_ = "";
		}

		check_multi_container() : show(showUnknown), perfData(true)
		{}
	private:
		check_multi_container(const TThisType &other) 
			: data(other.data), alias(other.alias), checks_(other.checks_), show(other.show) 
		{}
	public:
		~check_multi_container() {
			for (typename check_list_type::iterator it=checks_.begin(); it != checks_.end(); ++it) {
				delete *it;
			}
			checks_.clear();
		}
		std::string getAlias() {
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
		std::string gatherPerfData(value_type &value) {
			std::string ret;
			for (typename check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				ret += (*cit)->gatherPerfData((*cit)->getAlias(), value);
			}
			return ret;
		}
		bool hasBounds() {
			for (typename check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				if ((*cit)->hasBounds())
					return true;
			}
			return false;
		}
		void runCheck(value_type value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf) {
			for (typename check_list_type::const_iterator cit=checks_.begin(); cit != checks_.end(); ++cit) {
				(*cit)->set_showall(show);
				(*cit)->runCheck(value, returnCode, message, perf);
			}
		}
	};

	typedef unsigned long long disk_size_type;
	template <typename TType = disk_size_type>
	class disk_size_handler {
	public:
		static std::string print(TType value) {
			return format::format_byte_units(value);
		}
		static std::string get_perf_unit(TType value) {
			return format::find_proper_unit_BKMG(value);
		}
		static std::string print_perf(TType value, std::string unit) {
			return format::format_byte_units(value, unit);
		}
		static TType parse(std::string s) {
			TType val = format::decode_byte_units(s);
			return val;
		}
		static TType parse_percent(std::string s) {
			return strEx::stoi64(s);
		}
		static std::string print_percent(TType value) {
			return strEx::s::xtos(value) + "%";
		}
		static std::string print_unformated(TType value) {
			return strEx::s::xtos(value);
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

	typedef unsigned long long time_type;
	template <typename TType = time_type>
	class time_handler {
	public:
		static TType parse(std::string s) {
			TType val = strEx::stoi64_as_time(s);
			return val;
		}
		static TType parse_percent(std::string s) {
			return strEx::s::stox<TType>(s);
		}
		static std::string print(TType value) {
			return strEx::itos_as_time(value);
		}
		static std::string print_percent(TType value) {
			return strEx::s::xtos(value) + "%";
		}
		static std::string print_unformated(TType value) {
			return strEx::s::xtos(value);
		}
		static std::string get_perf_unit(TType value) {
			return "";
		}
		static std::string print_perf(TType value, std::string unit) {
			return strEx::s::xtos(value);
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
			int val = strEx::s::stox<int>(s);
			return val;
		}
		static int parse_percent(std::string s) {
			return strEx::s::stox<int>(s);
		}
		static std::string print(int value) {
			return strEx::s::xtos(value);
		}
		static std::string get_perf_unit(int value) {
			return "";
		}
		static std::string print_perf(int value, std::string unit) {
			return strEx::s::xtos(value);
		}
		static std::string print_unformated(int value) {
			return strEx::s::xtos(value);
		}
		static std::string print_percent(int value) {
			return strEx::s::xtos(value) + "%";
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
		static long long parse(std::string s) {
			long long val = strEx::s::stox<long long>(s);
			return val;
		}
		static long long parse_percent(std::string s) {
			return strEx::s::stox<long long>(s);
		}
		static std::string print(long long value) {
			return boost::lexical_cast<std::string>(value);
		}
		static std::string get_perf_unit(long long value) {
			return "";
		}
		static std::string print_perf(long long value, std::string unit) {
			return boost::lexical_cast<std::string>(value);
		}
		static std::string print_unformated(long long value) {
			return boost::lexical_cast<std::string>(value);
		}
		static std::string print_percent(long long value) {
			return boost::lexical_cast<std::string>(value) + "%";
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
		static std::string get_perf_unit(double value) {
			return "";
		}
		static std::string print_perf(double value, std::string unit) {
			return strEx::itos_non_sci(value);
		}
		static std::string print(double value) {
			return strEx::s::xtos(value);
		}
		static std::string print_unformated(double value) {
			return strEx::s::xtos(value);
		}
		static std::string print_percent(double value) {
			return strEx::s::xtos(value) + "%";
		}
		static std::string key_prefix() {
			return "";
		}
		static std::string key_postfix() {
			return "";
		}
	};

	typedef unsigned long state_type;
	const int state_none	  = 0x00;
	const int state_started   = 0x01;
	const int state_stopped   = 0x02;
	const int state_not_found = 0x06;
	const int state_hung      = 0x0e;
	const int state_pending_other   = 0x80;

	class state_handler {
	public:
		static state_type parse(std::string str) {
			state_type ret = state_none;
			BOOST_FOREACH(const std::string &s, strEx::s::splitEx(str, std::string(","))) {
				if (s == "started")
					ret |= state_started;
				else if (s == "stopped")
					ret |= state_stopped;
				else if (s == "ignored")
					ret |= state_none;
				else if (s == "not found")
					ret |= state_not_found;
				else if (s == "pending")
					ret |= state_pending_other;
				else if (s == "hung")
					ret |= state_hung;
			}
			return ret;
		}
		static std::string print(state_type value) {
			if (value == state_started)
				return "started";
			else if (value == state_stopped)
				return "stopped";
			else if (value == state_none)
				return "none";
			else if (value == state_not_found)
				return "not found";
			else if (value == state_pending_other)
				return "pending(other)";
			else if (value == state_hung)
				return "hung";
			return "unknown";
		}
		static std::string print_unformated(state_type value) {
			return strEx::s::xtos(value);
		}
	};


	template <typename TType = int, class THandler = int_handler >
	class NumericBounds {
	public:

		bool bHasBounds_;
		TType value_;
		typedef TType TValueType;
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
		static std::string gatherPerfData(std::string alias, std::string unit, TType &value, TType warn, TType crit) {
			if (unit.empty())
				unit = THandler::get_perf_unit(min(warn, min(crit, value)));
			return MAKE_PERFDATA(alias, THandler::print_perf(value, unit), unit, THandler::print_perf(warn, unit), THandler::print_perf(crit, unit));
		}
		static std::string gatherPerfData(std::string alias, std::string unit, TType &value) {
			if (unit.empty())
				unit = THandler::get_perf_unit(value);
			return MAKE_PERFDATA_SIMPLE(alias, THandler::print_perf(value, unit), unit);
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

	template <typename TType, class THandler>
	class NumericPercentageBounds {
	public:
		typedef enum {
			none = 0,
			percentage_upper = 1,
			percentage_lower = 2,
			value_upper = 3,
			value_lower = 4
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

		typedef TType TValueType;
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

		std::string gatherPerfData(std::string alias, std::string unit, TType &value, typename TType::TValueType warn, typename TType::TValueType crit) {
			unsigned int value_p, warn_p, crit_p;
			typename TType::TValueType warn_v, crit_v;
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
			if (unit.empty())
				unit = THandler::get_perf_unit(min_no_zero(warn_v, crit_v, value.value));
			return 
				MAKE_PERFDATA(alias + " %", THandler::print_unformated(value_p), "%", THandler::print_unformated(warn_p), THandler::print_unformated(crit_p))
				+ " " +
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
		std::string gatherPerfData(std::string alias, std::string unit, TType &value) {
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
			if (unit.empty())
				unit = THandler::get_perf_unit(value.value);
			return 
				MAKE_PERFDATA_SIMPLE(alias + " %", THandler::print_unformated(value_p), "%")
				+ " " +
				MAKE_PERFDATA_SIMPLE(alias, THandler::print_perf(value.value, unit), unit)
				;
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
		typedef TType TValueType;
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
		std::string gatherPerfData(std::string alias, std::string unit, TType &value, TType warn, TType crit) {
			return "";
		}
		std::string gatherPerfData(std::string alias, std::string unit, TType &value) {
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


	template <class TValueType = MaxMinStateValueType<>, class TNumericHolder = NumericBounds<int, int_handler>, class TStateHolder = StateBounds<state_type, state_handler> >
	class MaxMinStateBounds {
	public:
		TNumericHolder max_;
		TNumericHolder min_;
		TStateHolder state;
		typedef MaxMinStateBounds<TValueType, TNumericHolder, TStateHolder > TMyType;

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

		static std::string toStringLong(TValueType &value) {
			return TNumericHolder::toStringLong(value.count) + ", " + TStateHolder::toStringLong(value.state);
		}
		static std::string toStringShort(TValueType &value) {
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
		std::string gatherPerfData(std::string alias, std::string unit, TValueType &value, TMyType &warn, TMyType &crit) {
			if (max_.hasBounds()) {
				return max_.gatherPerfData(alias, unit, value.count, warn.max_.getPerfBound(value.count), crit.max_.getPerfBound(value.count));
			} else if (min_.hasBounds()) {
				return min_.gatherPerfData(alias, unit, value.count, warn.min_.getPerfBound(value.count), crit.min_.getPerfBound(value.count));
			} else if (state.hasBounds()) {
				return min_.gatherPerfData(alias, unit, value.count, 0, 0);
			}
			return "";
		}
		std::string gatherPerfData(std::string alias, std::string unit, TValueType &value) {
			return "";
		}
		bool check(TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value.state))) {
				message = lable + ": " + formatState(TStateHolder::toStringShort(value.state), type);
				return true;
			} else if ((max_.hasBounds())&&(max_.check(value.count) != below)) {
				message = lable + ": " + formatAbove(TNumericHolder::toStringShort(value.count), type);
				return true;
			} else if ((min_.hasBounds())&&(min_.check(value.count) != above)) {
				message = lable + ": " + formatBelow(TNumericHolder::toStringShort(value.count), type);
				return true;
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
		void reset() {
			state.reset();
		}
		bool hasBounds() {
			return state.hasBounds();
		}
		static std::string toStringLong(TValueType &value) {
			return TStateHolder::toStringLong(value);
		}
		static std::string toStringShort(TValueType &value) {
			return TStateHolder::toStringShort(value);
		}
		std::string gatherPerfData(std::string alias, std::string unit, TValueType &value, TMyType &warn, TMyType &crit) {
			if (state.hasBounds()) {
				// @todo
			}
			return "";
		}
		std::string gatherPerfData(std::string alias, std::string unit, TValueType &value) {
			return "";
		}
		bool check(TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value))) {
				message = lable + ": " + formatState(TStateHolder::toStringLong(value), type);
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
		static std::string toStringLong(typename THolder::TValueType &value) {
			return THolder::toStringLong(value);
		}
		static std::string toStringShort(typename THolder::TValueType &value) {
			return THolder::toStringShort(value);
		}
		std::string gatherPerfData(std::string alias, std::string unit, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max_.hasBounds()) {
				return max_.gatherPerfData(alias, unit, value, warn.max_.getPerfBound(value), crit.max_.getPerfBound(value));
			} else if (min_.hasBounds()) {
				return min_.gatherPerfData(alias, unit, value, warn.min_.getPerfBound(value), crit.min_.getPerfBound(value));
			} else {
				return min_.gatherPerfData(alias, unit, value, 0, 0);
			}
		}
		std::string gatherPerfData(std::string alias, std::string unit, typename THolder::TValueType &value) {
			THolder tmp;
			return tmp.gatherPerfData(alias, unit, value);
		}
		bool check(typename THolder::TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((max_.hasBounds())&&(max_.check(value) != below)) {
				message = lable + ": " + formatAbove(THolder::toStringLong(value), type);
				return true;
			} else if ((min_.hasBounds())&&(min_.check(value) != above)) {
				message = lable + ": " + formatBelow(THolder::toStringLong(value), type);
				return true;
			} else {
				//std::cout << "No bounds specified..." << std::endl;
			}
			return false;
		}

	};
	typedef MaxMinBounds<NumericBounds<double, double_handler> > MaxMinBoundsDouble;
	typedef MaxMinBounds<NumericBounds<long long, int64_handler> > MaxMinBoundsInt64;
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

		const TMyType& operator=(std::string value) {
			//value_ = value;
			if (value.substr(0,1) == ">") {
				max = value.substr(1);
			} else if (value.substr(0,2) == "<>") {
				neq = value.substr(2);
			} else if (value.substr(0,1) == "<") {
				min = value.substr(1);
			} else if (value.substr(0,1) == "=") {
				eq = value.substr(1);
			} else if (value.substr(0,2) == "!=") {
				neq = value.substr(2);
			} else if (value.substr(0,1) == "!") {
				neq = value.substr(1);
			} else if (value.substr(0,3) == "gt:") {
				max = value.substr(3);
			} else if (value.substr(0,3) == "lt:") {
				min = value.substr(3);
			} else if (value.substr(0,3) == "ne:") {
				neq = value.substr(3);
			} else if (value.substr(0,3) == "eq:") {
				eq = value.substr(3);
			} else {
				throw parse_exception("Unknown filter key: " + value + " (numeric filters have to have an operator as well ie. foo=>5 or bar==5 foo=gt:6)");
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
		static std::string toStringLong(typename THolder::TValueType &value) {
			return THolder::toStringLong(value);
		}
		static std::string toStringShort(typename THolder::TValueType &value) {
			return THolder::toStringShort(value);
		}
		std::string gatherPerfData(std::string alias, std::string unit, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max.hasBounds()) {
				return max.gatherPerfData(alias, unit, value, warn.max.getPerfBound(value), crit.max.getPerfBound(value));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, unit, value, warn.min.getPerfBound(value), crit.min.getPerfBound(value));
			} else if (neq.hasBounds()) {
				return neq.gatherPerfData(alias, unit, value, warn.neq.getPerfBound(value), crit.neq.getPerfBound(value));
			} else if (eq.hasBounds()) {
				return eq.gatherPerfData(alias, unit, value, warn.eq.getPerfBound(value), crit.eq.getPerfBound(value));
			} else {
				return "";
			}
		}
		std::string gatherPerfData(std::string alias, std::string unit, typename THolder::TValueType &value) {
			THolder tmp;
			return tmp.gatherPerfData(alias, unit, value);
		}
		bool check(typename THolder::TValueType &value, std::string lable, std::string &message, ResultType type) {
			return check_preformatted(value, THolder::toStringLong(value), lable, message, type);
		}
		bool check_preformatted(typename THolder::TValueType &value, std::string formatted_value, std::string lable, std::string &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) == above)) {
				message = lable + ": " + formatAbove(formatted_value, type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) == below)) {
				message = lable + ": " + formatBelow(formatted_value, type);
				return true;
			} else if ((eq.hasBounds())&&(eq.check(value) == same)) {
				message = lable + ": " + formatSame(formatted_value, type);
				return true;
			} else if ((neq.hasBounds())&&(neq.check(value) != same)) {
				message = lable + ": " + formatNotSame(formatted_value, type);
				return true;
			}
			return false;
		}
	};
	typedef ExactBounds<NumericBounds<unsigned long int, int_handler> > ExactBoundsULongInteger;
	typedef ExactBounds<NumericBounds<unsigned int, int_handler> > ExactBoundsUInteger;
	typedef ExactBounds<NumericBounds<unsigned long, int_handler> > ExactBoundsULong;
	typedef ExactBounds<NumericBounds<long long, int_handler> > ExactBoundsLongLong;
	typedef ExactBounds<NumericBounds<time_type, time_handler<long long> > > ExactBoundsTime;

	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<disk_size_type, disk_size_type>, disk_size_handler<> > > MaxMinPercentageBoundsDiskSize;
	typedef MaxMinBounds<NumericPercentageBounds<PercentageValueType<long long, long long>, disk_size_handler<long long> > > MaxMinPercentageBoundsDiskSizei64;

	typedef MaxMinStateBounds<MaxMinStateValueType<int, state_type>, NumericBounds<int, int_handler>, StateBounds<state_type, state_handler> > MaxMinStateBoundsStateBoundsInteger;
	typedef SimpleStateBounds<StateBounds<state_type, state_handler> > SimpleBoundsStateBoundsInteger;
}


