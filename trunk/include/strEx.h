#pragma once

#include <sstream>
#include <iomanip>
#include <string>
#include <utility>
#include <list>
#include <functional>
#include <time.h>
#ifdef _DEBUG
#include <iostream>
#endif

namespace strEx {

	inline void append_list(std::string &lst, std::string &append) {
		if (!lst.empty())
			lst += ", ";
		lst += append;
	}

	inline std::string format_date(time_t time, std::string format) {
		char buf[51];
		size_t l = strftime(buf, 50, format.c_str(), gmtime(&time));
		if (l <= 0 || l >= 50)
			return "";
		buf[l] = 0;
		return buf;
	}

	static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
	static const __int64 SECS_TO_100NS = 10000000;
	inline std::string format_filetime(unsigned long long filetime, std::string format) {
		filetime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);
		filetime /= SECS_TO_100NS;
		return format_date(static_cast<time_t>(filetime), format);
	}

	inline void replace(std::string &string, std::string replace, std::string with) {
		std::string::size_type pos = string.find(replace);
		std::string::size_type len = replace.length();
		while (pos != std::string::npos) {
			string = string.substr(0,pos)+with+string.substr(pos+len);
			pos = string.find(replace, pos+1);
		}
	}
	inline std::string itos(unsigned int i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(int i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(unsigned long long i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(__int64 i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(unsigned long i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string itos(double i) {
		std::stringstream ss;
		ss << i;
		return ss.str();
	}
	inline std::string ihextos(unsigned int i) {
		std::stringstream ss;
		ss << std::hex << i;
		return ss.str();
	}
	inline int stoi(std::string s) {
		return atoi(s.c_str());
	}
	inline double stod(std::string s) {
		return atof(s.c_str());
	}
	inline long long stoi64(std::string s) {
		return _atoi64(s.c_str());
	}
	inline unsigned stoui_as_time(std::string time, unsigned int smallest_unit = 1000) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		unsigned int value = atoi(time.c_str());
		if (p == std::string::npos)
			return value * smallest_unit;
		else if ( (time[p] == 's') || (time[p] == 'S') )
			return value * 1000;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60 * 1000;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60 * 1000;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60 * 1000;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}

	inline unsigned long long stoi64_as_time(std::string time, unsigned int smallest_unit = 1000) {
		std::string::size_type p = time.find_first_of("sSmMhHdDwW");
		unsigned long long value = _atoi64(time.c_str());
		if (p == std::string::npos)
			return value * smallest_unit;
		else if ( (time[p] == 's') || (time[p] == 'S') )
			return value * 1000;
		else if ( (time[p] == 'm') || (time[p] == 'M') )
			return value * 60 * 1000;
		else if ( (time[p] == 'h') || (time[p] == 'H') )
			return value * 60 * 60 * 1000;
		else if ( (time[p] == 'd') || (time[p] == 'D') )
			return value * 24 * 60 * 60 * 1000;
		else if ( (time[p] == 'w') || (time[p] == 'W') )
			return value * 7 * 24 * 60 * 60 * 1000;
		return value * smallest_unit;
	}

#define WEEK	(7 * 24 * 60 * 60 * 1000)
#define DAY		(24 * 60 * 60 * 1000)
#define HOUR	(60 * 60 * 1000)
#define MIN		(60 * 1000)
#define SEC		(1000)
	inline std::string itos_as_time(unsigned long long time) {
		if (time > WEEK) {
			unsigned int w = static_cast<unsigned int>(time/WEEK);
			unsigned int d = static_cast<unsigned int>((time-(w*WEEK))/DAY);
			unsigned int h = static_cast<unsigned int>((time-(w*WEEK)-(d*DAY))/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(w*WEEK)-(d*DAY)-(h*HOUR))/MIN);
			return itos(w) + "w " + itos(d) + "d " + itos(h) + ":" + itos(m);
		}
		else if (time > DAY) {
			unsigned int d = static_cast<unsigned int>((time)/DAY);
			unsigned int h = static_cast<unsigned int>((time-(d*DAY))/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(d*DAY)-(h*HOUR))/MIN);
			return itos(d) + "d " + itos(h) + ":" + itos(m);
		}
		else if (time > HOUR) {
			unsigned int h = static_cast<unsigned int>((time)/HOUR);
			unsigned int m = static_cast<unsigned int>((time-(h*HOUR))/MIN);
			return itos(h) + ":" + itos(m);
		} else if (time > MIN) {
			return "0:" + itos(static_cast<unsigned int>(time/(60 * 1000)));
		} else if (time > SEC)
			return itos(static_cast<unsigned int>(time/(1000))) + "s";
		return itos(static_cast<unsigned int>(time));
	}

	inline long long stoi64_as_BKMG(std::string s) {
		std::string::size_type p = s.find_first_of("BMKGT");
		if (p == std::string::npos)
			return _atoi64(s.c_str());
		else if (s[p] == 'B') 
			return _atoi64(s.c_str());
		else if (s[p] == 'K') 
			return _atoi64(s.c_str())*1024;
		else if (s[p] == 'M') 
			return _atoi64(s.c_str())*1024*1024;
		else if (s[p] == 'G') 
			return _atoi64(s.c_str())*1024*1024*1024;
		else if (s[p] == 'T') 
			return _atoi64(s.c_str())*1024*1024*1024*1024;
		else
			return _atoi64(s.c_str());
	}
#define BKMG_RANGE "BKMGTP"
#define BKMG_SIZE 5

	inline std::string itos_as_BKMG(unsigned __int64 i) {
		double cpy = static_cast<double>(i);
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 1024)&&(idx<BKMG_SIZE)) {
			cpy/=1024;
			idx++;
		}
		std::stringstream ss;
		ss << std::setprecision(3);
		ss << cpy;
		std::string ret = ss.str(); // itos(cpy);
		ret += postfix[idx];
		return ret;
	}

	typedef std::list<std::string> splitList;
	inline splitList splitEx(std::string str, std::string key) {
		splitList ret;
		std::string::size_type pos = 0, lpos = 0;
		while ((pos = str.find(key, pos)) !=  std::string::npos) {
			ret.push_back(str.substr(lpos, pos-lpos));
			lpos = ++pos;
		}
		if (lpos < str.size())
			ret.push_back(str.substr(lpos));
		return ret;
	}
	inline std::string joinEx(splitList lst, std::string key) {
		std::string ret;
		for (splitList::const_iterator it = lst.begin(); it != lst.end(); ++it) {
			if (!ret.empty())
				ret += key;
			ret += *it;
		}
		return ret;
	}

	inline std::pair<std::string,std::string> split(std::string str, std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string,std::string>(str, "");
		return std::pair<std::string,std::string>(str.substr(0, pos), str.substr(pos+key.length()));
	}
	typedef std::pair<std::string,std::string> token;
	// foo bar "foo \" bar" foo -> foo, bar "foo \" bar" foo -> bar, "foo \" bar" foo -> 
	// 
	inline token getToken(std::string buffer, char split, bool escape = false) {
		std::string::size_type pos = std::string::npos;
		if ((escape) && (buffer[0] == '\"')) {
			do {
				pos = buffer.find('\"');
			}
			while (((pos != std::string::npos)&&(pos > 1))&&(buffer[pos-1] == '\\'));
		} else
			pos = buffer.find(split);
		if (pos == std::string::npos)
			return token(buffer, "");
		if (pos == buffer.length()-1)
			return token(buffer.substr(0, pos), "");
		return token(buffer.substr(0, pos-1), buffer.substr(++pos));
	}


	template<class _E>
	struct blind_traits : public std::char_traits<_E>
	{
		static bool eq(const _E& x, const _E& y) {
			return tolower( x ) == tolower( y ); 
		}
		static bool lt(const _E& x, const _E& y) {
			return tolower( x ) < tolower( y ); 
		}

		static int compare(const _E *x, const _E *y, size_t n) { 
			return strnicmp( x, y, n );
		}

		//  There's no memichr(), so we roll our own.  It ain't rocket science.
		static const _E * __cdecl find(const _E *buf, size_t len, const _E& ch) {
			//  Jerry says that x86s have special mojo for memchr(), so the 
			//  memchr() calls end up being reasonably efficient in practice.
			const _E *pu = (const _E *)memchr(buf, ch, len);
			const _E *pl = (const _E *)memchr(buf, tolower( ch ), len);
			if ( ! pu )
				return pl;  //  Might be NULL; if so, NULL's the word.
			else if ( ! pl )
				return pu;
			else
				//  If either one was NULL, we return the other; if neither is 
				//  NULL, we return the lesser of the two.
				return ( pu < pl ) ? pu : pl;
		}

		//  I'm reasonably sure that this is eq() for wide characters.  Maybe.
		static bool eq_int_type(const int_type& ch1, const int_type& ch2) { 
			return char_traits<_E>::eq_int_type( tolower( ch1 ), tolower( ch2 ) ); 
		}
	};

	//  And here's our case-blind string class.
	typedef std::basic_string<char, blind_traits<char>, std::allocator<char> >  blindstr;

	struct case_blind_string_compare : public std::binary_function<std::string, std::string, bool>
	{
		bool operator() (const std::string& x, const std::string& y) const {
			return stricmp( x.c_str(), y.c_str() ) < 0;
		}
	};
#ifdef _DEBUG
	inline void test_getToken(std::string in1, char in2, std::string out1, std::string out2) {
		token t = getToken(in1, in2);
		std::cout << "strEx::test_getToken(" << in1 << ", " << in2 << ") : ";
		if ((t.first == out1) && (t.second == out2))
			std::cout << "Succeeded" << std::endl;
		else
			std::cout << "Failed [" << out1 << "=" << t.first << ", " << out2 << "=" << t.second << "]" << std::endl;
	}
	inline void run_test_getToken() {
		test_getToken("", '&', "", "");
		test_getToken("&", '&', "", "");
		test_getToken("&&", '&', "", "&");
		test_getToken("foo", '&', "foo", "");
		test_getToken("foo&", '&', "foo", "");
		test_getToken("foo&bar", '&', "foo", "bar");
		test_getToken("foo&bar&test", '&', "foo", "bar&test");
	}

	inline void test_replace(std::string source, std::string replace, std::string with, std::string out) {
		std::cout << "strEx::test_replace(" << source << ", " << replace << ", " << with << ") : ";
		std::string s = source;
		strEx::replace(s, replace, with);
		if (s == out)
			std::cout << "Succeeded" << std::endl;
		else
			std::cout << "Failed [" << s << "=" << out << "]" << std::endl;
	}
	inline void run_test_replace() {
		test_replace("", "", "", "");
		test_replace("foo", "", "", "foo");
		test_replace("foobar", "foo", "", "bar");
		test_replace("foobar", "foo", "bar", "barbar");
	}

#endif
}