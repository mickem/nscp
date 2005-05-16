#pragma once

#include <string>
#include <strEx.h>

namespace checkHolders {

	typedef unsigned __int64 drive_size;
	template <typename TType = drive_size>
	class drive_size_handler {
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
	};

	template <typename TType = __int64>
	class int64_handler {
	public:
		static TType parse(std::string s) {
			return strEx::stoi64(s);
		}
		static TType parse_percent(std::string s) {
			return strEx::stoi(s);
		}
		static std::string print(TType value) {
			return strEx::itos(value);
		}
		static std::string print_percent(TType value) {
			return strEx::itos(value) + "%";
		}
	};

	template <typename TType = drive_size, class THandler = drive_size_handler<> >
	class Size {
	public:
		typedef enum {
			above = 1,
			below = -1,
			same = 0,
		} checkResult;

		bool bHasBounds_;
		TType value_;

		Size() : bHasBounds_(false), value_(0) {};
		void set(std::string s) {
			value_ = THandler::parse(s);
			bHasBounds_ = true;
		}
		checkResult check(TType value) const {
			if (value == value_)
				return same;
			else if (value > value_)
				return above;
			return below;
		}
		std::string prettyPrint(std::string name, TType value) const {
			return name + ": " + THandler::print(value);
		}

		inline bool hasBounds() const {
			return bHasBounds_;
		}
		inline bool checkMAX(TType value) const {
			return check(value)==above;
		}
		inline bool checkMIN(TType value) const {
			return check(value)==below;
		}
		inline std::string toString() const {
			return strEx::itos(value_) + (type_==percentage?"%":"");
		}
	};

	template <typename TType = drive_size, class THandler = drive_size_handler<> >
	class SizePercentage {
	public:
		typedef enum {
			none,
			percentage,
			size,
		} checkTypes;
		typedef enum {
			above = 1,
			below = -1,
			same = 0,
		} checkResult;

		checkTypes type_;
		TType value_;

		SizePercentage() : type_(none), value_(0) {};
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
		checkResult check(TType value, TType max) const {
			unsigned long long p = getValue(value, max);
			if (p == value_)
				return same;
			else if (p > value_)
				return above;
			return below;
		}
		std::string prettyPrint(std::string name, TType value, TType max) const {
			if (type_ == percentage)
				return name + ": " + THandler::print_percent(getValue(value, max));
			return name + ": " + THandler::print(getValue(value, max));
		}

		inline bool hasBounds() const {
			return type_ != none;
		}
		inline bool isPercentage() const {
			return type_ == percentage;
		}
		inline bool checkMAX(TType value, TType max) const {
			return check(value, max)==above;
		}
		inline bool checkMIN(TType value, TType max) const {
			return check(value, max)==below;
		}
		inline std::string toString() const {
			return strEx::itos(value_) + (type_==percentage?"%":"");
		}
		inline long long getValue(TType value, TType max) const {
			if (type_ == percentage) {
				return static_cast<int>((value*100) / max);
			} else if (type_ == size) {
				return value;
			} else {
				return 0;
			}
		}
	};


	template <typename TType = drive_size, class THandler = drive_size_handler<>, class THolder = Size<TType, THandler> >
	class SizeMaxMin {
	public:
		THolder max;
		THolder min;
		typedef SizeMaxMin<TType, THandler, THolder> TMyType;

		std::string printPerfData()
		{
			if (max.hasBounds()) {
				return THandler::print(max.value_) + ";";
			} else if (min.hasBounds()) {
				return THandler::print(min.value_) + ";";
			}
			return "0;";
		}
		static std::string printPerf(std::string name, TType value, TMyType &warn, TMyType &crit)
		{
			return name + "=" + strEx::itos(value) + ";" + warn.printPerfData() + crit.printPerfData();
		}

	};
	template <typename TType = drive_size, class THandler = drive_size_handler<>, class THolder = SizePercentage<TType, THandler> >
	struct SizeMaxMinPercentage {
	public:
		THolder max;
		THolder min;
		typedef SizeMaxMinPercentage<TType, THandler, THolder> TMyType;

		bool isPercentage() {
			if (max.hasBounds())
				return max.isPercentage();
			else if (min.hasBounds())
				return min.isPercentage();
			return false;
		}
		std::string printPerfData(bool bPercentage, TType value, TType total)
		{
			if (bPercentage) {
				if (max.hasBounds()) {
					if (max.isPercentage()) {
						return THandler::print_percent(max.value_) + ";";
					} else {
						return THandler::print_percent((total*100)/(max.value_==0?1:max.value_)) + ";";
					}
				} else if (min.hasBounds()) {
					if (min.isPercentage()) {
						return THandler::print_percent(min.value_) + ";";
					} else {
						return THandler::print_percent((total*100)/(min.value_==0?1:min.value_)) + ";";
					}
				}
			} else {
				if (max.hasBounds()) {
					if (max.isPercentage()) {
						return THandler::print((max.value_*total)/100) + ";";
					} else {
						return THandler::print(max.value_) + ";";
					}
				} else if (min.hasBounds()) {
					if (min.isPercentage()) {
						return THandler::print((min.value_*total)/100) + ";";
					} else {
						return THandler::print(min.value_) + ";";
					}
				}
			}
			return "0;";
		}
		static std::string printPerf(std::string name, TType value, TType total, TMyType &warn, TMyType &crit)
		{
			std::string s;
			bool percentage = crit.isPercentage()  || warn.isPercentage();
			if (percentage)
				s += name + "=" + strEx::itos(value*100/total)+ "% ";
			else
				s+= name + "=" + strEx::itos(value) + ";";
			s += warn.printPerfData(percentage, value, total);
			s += crit.printPerfData(percentage, value, total);
			s += " ";
			return s;
		}

	};
/*
	template <typename TType = drive_size, class THandler = drive_size_handler<>, class THolder = SizeMaxMinPercentage<> > 
	class PerformancePrinterPercentage {
	public:
		static std::string printPerf(std::string name, TType value, TType total, THolder &warn, THolder &crit)
		{
			std::string s;
			bool percentage = crit.isPercentage()  || warn.isPercentage();
			if (percentage)
				s += name + "=" + strEx::itos(value*100/total)+ "% ";
			else
				s+= name + "=" + strEx::itos(value) + ";";
			s += warn.printPerfData(percentage, value, total);
			s += crit.printPerfData(percentage, value, total);
			s += " ";
			return s;
		}
	};
	template <typename TType = drive_size, class THandler = drive_size_handler<>, class THolder = SizeMaxMin<> > 
	class PerformancePrinter {
	public:
		static std::string printPerf(std::string name, TType value, THolder &warn, THolder &crit)
		{
			return name + "=" + strEx::itos(value) + ";" + warn.printPerfData() + crit.printPerfData();
		}
	};
*/
}
void generate_crc32_table(void);
unsigned long calculate_crc32(const char *buffer, int buffer_size);

namespace socketHelpers {
	class allowedHosts {
	private:
		strEx::splitList allowedHosts_;
	public:
		void setAllowedHosts(strEx::splitList allowedHosts) {
			if ((!allowedHosts.empty()) && (allowedHosts.front() == "") )
				allowedHosts.pop_front();
			allowedHosts_ = allowedHosts;
		}
		bool inAllowedHosts(std::string s) {
			if (allowedHosts_.empty())
				return true;
			strEx::splitList::const_iterator cit;
			for (cit = allowedHosts_.begin();cit!=allowedHosts_.end();++cit) {
				if ( (*cit) == s)
					return true;
			}
			return false;
		}
	};
}



