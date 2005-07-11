#pragma once

#include <string>
#include <strEx.h>


namespace checkHolders {


	typedef enum { warning, critical} ResultType;
	typedef enum { above = 1, below = -1, same = 0 } checkResultType;



	std::string formatAbove(std::string str, ResultType what) {
		if (what == warning)
			return str + " > warning";
		else if (what == critical)
			return str + " > critical";
		return str + " > unknown";
	}

	std::string formatBelow(std::string str, ResultType what) {
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

	template <class TContents>
	struct CheckConatiner {
		typedef CheckConatiner<TContents> TThisType;
		TContents warn;
		TContents crit;
		std::string data;
		std::string alias;
		CheckConatiner() 
		{}
		CheckConatiner(std::string data_, TContents warn_, TContents crit_) 
			: data(data_), warn(warn_), crit(crit_) 
		{}
		CheckConatiner(std::string name_, std::string alias_, TContents warn_, TContents crit_) 
			: data(data_), alias(alias_), warn(warn_), crit(crit_) 
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
		}
		void formatString(std::string &message, typename TContents::TValueType &value) {
			crit.formatString(message, value);
			message = getAlias() + ": " + message;
		}
		std::string gatherPerfData(typename TContents::TValueType &value) {
			return crit.gatherPerfData(getAlias(), value, warn, crit);
		}
		void runCheck(typename TContents::TValueType &value, NSCAPI::nagiosReturn &returnCode, std::string &message, std::string &perf, bool bShowAll) {
			std::string tstr;
			if (crit.check(value, getAlias(), tstr, critical)) {
				NSCHelper::escalteReturnCodeToCRIT(returnCode);
			} else if (warn.check(value, getAlias(), tstr, warning)) {
				NSCHelper::escalteReturnCodeToWARN(returnCode);
			}else if (bShowAll) {
				formatString(tstr, value);
			}
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
		static TType parse(std::string s) {
			return strEx::stoi64_as_BKMG(s);
		}
		static TType parse_percent(std::string s) {
			return strEx::stoi64(s);
		}
		static std::string print(TType value) {
			return strEx::itos_as_BKMG(value);
		}
		static std::string print_percent(TType value) {
			return strEx::itos(value) + "%";
		}
		static std::string print_unformated(TType value) {
			return strEx::itos(value);
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
		static std::string print_unformated(int value) {
			return strEx::itos(value);
		}
		static std::string print_percent(int value) {
			return strEx::itos(value) + "%";
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
		static std::string print_unformated(__int64 value) {
			return strEx::itos(value);
		}
		static std::string print_percent(__int64 value) {
			return strEx::itos(value) + "%";
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
		static std::string print(double value) {
			return strEx::itos(value);
		}
		static std::string print_unformated(double value) {
			return strEx::itos(value);
		}
		static std::string print_percent(double value) {
			return strEx::itos(value) + "%";
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

		std::string toString(TType value) const {
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return bHasBounds_;
		}

		inline bool isAbove(TType value) const {
			return check(value)==above;
		}
		inline bool isBelow(TType value) const {
			return check(value)==below;
		}
		const NumericBounds & operator=(std::string value) {
			set(value);
			return *this;
		}

		TType getPerfBound(TType value) {
			return value_;
		}
		static std::string gatherPerfData(std::string alias, TType &value, TType warn, TType crit) {
			return alias + ";"
				+ THandler::print_unformated(value) + ";"
				+ THandler::print_unformated(warn) + ";"
				+ THandler::print_unformated(crit) + "; ";
		}

	private:
		void set(std::string s) {
			value_ = THandler::parse(s);
			bHasBounds_ = true;
		}
	};

	template <typename TType = int, class THandler = int_handler >
	class NumericPercentageBounds {
	public:
		typedef enum {
			none,
			percentage,
			size,
		} checkTypes;

		checkTypes type_;
		TType value_;
		TType max_;
		typedef typename TType TValueType;
		typedef THandler TFormatType;
		typedef NumericPercentageBounds<TType, THandler> TMyType;

		NumericPercentageBounds() : type_(none), value_(0), max_(0) {}

		NumericPercentageBounds(const NumericPercentageBounds &other) {
			type_ = other.type_;
			value_ = other.value_;
			max_ = other.max_;
		}
		void setMax(TType max) { max_ = max; }

		checkResultType check(TType value) const {
			if (type_ == percentage) {
				if (((value*100)/max_) == value_)
					return same;
				else if (((value*100)/max_) > value_)
					return above;
				else
					return below;
			} else {
				if (value == value_)
					return same;
				else if (value > value_)
					return above;
				return below;
			}
		}
		std::string toString(TType value) const {
			if (type_ == percentage)
				return THandler::print_percent((value*100)/max_);
			return THandler::print(value);
		}
		inline bool hasBounds() const {
			return type_ != none;
		}
		inline bool isPercentage() const {
			return type_ == percentage;
		}
		inline bool isAbove(TType value, TType max) const {
			return check(value, max)==above;
		}
		inline bool isBelow(TType value, TType max) const {
			return check(value, max)==below;
		}
		TType getPerfBound(TType value) {
			return value_;
		}
		std::string gatherPerfData(std::string alias, TType &value, TType warn, TType crit) {
			if (isPercentage()) {
				return alias + ";"
					+ THandler::print_unformated((value*100)/max_) + "%;"
					+ THandler::print_unformated(warn) + ";"
					+ THandler::print_unformated(crit) + "; ";
			} else {
				return alias + ";"
					+ THandler::print_unformated(value) + ";"
					+ THandler::print_unformated((warn*max_)/100) + ";"
					+ THandler::print_unformated((crit*max_)/100) + "; ";
			}
		}
		const NumericPercentageBounds & operator=(std::string value) {
			set(value);
			return *this;
		}
	private:
		void set(std::string s) {
			std::string::size_type p = s.find_first_of('%');
			if (p == std::string::npos) {
				value_ = THandler::parse(s);
				type_ = size;
			} else {
				value_ = THandler::parse_percent(s);
				type_ = percentage;
			}
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
		std::string toString(TType value) const {
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
		void formatString(std::string &message, typename TValueType &value) {
			if (state.hasBounds())
				message = state.toString(value.state);
			else if (max.hasBounds())
				message = max.toString(value.count);
			else if (min.hasBounds())
				message = max.toString(value.count);
		}
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
				message = lable + ": " + formatState(state.toString(value.state), type);
				return true;
			} else if ((max.hasBounds())&&(max.check(value.count) != below)) {
				message = lable + ": " + formatAbove(max.toString(value.count), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value.count) != above)) {
				message = lable + ": " + formatBelow(min.toString(value.count), type);
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
		void formatString(std::string &message, typename TValueType &value) {
			message = state.toString(value);
		}
		std::string gatherPerfData(std::string alias, typename TValueType &value, TMyType &warn, TMyType &crit) {
			if (state.hasBounds()) {
				// @todo
			}
			return "";
		}
		bool check(typename TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((state.hasBounds())&&(!state.check(value))) {
				message = lable + ": " + formatState(state.toString(value), type);
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
		void formatString(std::string &message, typename THolder::TValueType &value) {
			message = max.toString(value);
		}
		std::string gatherPerfData(std::string alias, typename THolder::TValueType &value, TMyType &warn, TMyType &crit) {
			if (max.hasBounds()) {
				return max.gatherPerfData(alias, value, warn.max.getPerfBound(value), crit.max.getPerfBound(value));
			} else if (min.hasBounds()) {
				return min.gatherPerfData(alias, value, warn.min.getPerfBound(value), crit.min.getPerfBound(value));
			}
			return "";
		}
		bool check(typename THolder::TValueType &value, std::string lable, std::string &message, ResultType type) {
			if ((max.hasBounds())&&(max.check(value) != below)) {
				message = lable + ": " + formatAbove(max.toString(value), type);
				return true;
			} else if ((min.hasBounds())&&(min.check(value) != above)) {
				message = lable + ": " + formatBelow(min.toString(value), type);
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
	typedef MaxMinBounds<NumericBounds<disk_size_type, disk_size_handler<disk_size_type> > > MaxMinBoundsDiscSize;
	typedef MaxMinBounds<NumericBounds<time_type, time_handler<time_type> > > MaxMinBoundsTime;


	typedef MaxMinBounds<NumericPercentageBounds<int, int_handler> > MaxMinPercentageBoundsInteger;
	typedef MaxMinBounds<NumericPercentageBounds<__int64, int64_handler> > MaxMinPercentageBoundsInt64;
	typedef MaxMinBounds<NumericPercentageBounds<double, double_handler> > MaxMinPercentageBoundsDouble;
	typedef MaxMinBounds<NumericPercentageBounds<disk_size_type, disk_size_handler<> > > MaxMinPercentageBoundsDiskSize;

	typedef MaxMinStateBounds<MaxMinStateValueType<int, state_type>, NumericBounds<int, int_handler>, StateBounds<state_type, state_handler> > MaxMinStateBoundsStateBoundsInteger;
	typedef SimpleStateBounds<StateBounds<state_type, state_handler> > SimpleBoundsStateBoundsInteger;
}

