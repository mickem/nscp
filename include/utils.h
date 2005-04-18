#pragma once

#include <string>
#include <strEx.h>

namespace checkHolders {
	struct Size {
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
		long long value_;

		Size();
		void set(std::string s);
		checkResult check(long long value, long long max) const;
		std::string prettyPrint(std::string name, long long value, long long max) const;

		inline bool hasBounds() const {
			return type_ != none;
		}
		inline bool isPercentage() const {
			return type_ == percentage;
		}
		inline bool checkMAX(long long value, long long max) const {
			return check(value, max)==above;
		}
		inline bool checkMIN(long long value, long long max) const {
			return check(value, max)==below;
		}
		inline std::string toString() const {
			return strEx::itos(value_) + (type_==percentage?"%":"");
		}
		inline long long getValue(long long value, long long max) const {
			if (type_ == percentage) {
				return static_cast<int>((value*100) / max);
			} else if (type_ == size) {
				return value;
			} else {
				return 0;
			}
		}
	};
	struct SizeMaxMin {
		Size max;
		Size min;

		bool isPercentage() {
			if (max.hasBounds())
				return max.isPercentage();
			else if (min.hasBounds())
				return min.isPercentage();
			return false;
		}
		std::string printPerfData(bool percentage, long long value, long long total);
		static std::string printPerfHead(bool percentage, std::string name, long long value, long long total);
		static std::string printPerf(std::string name, long long value, long long total, SizeMaxMin &warn, SizeMaxMin &crit);
	};
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



