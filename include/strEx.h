#pragma once

#include <sstream>
#include <string>
#include <utility>
#include <list>
#ifdef _DEBUG
#include <iostream>
#endif

namespace strEx {

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
	inline int stoi(std::string s) {
		return atoi(s.c_str());
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
	inline std::string itos_as_time(unsigned long long time) {
		if (time > 7 * 24 * 60 * 60 * 1000)
			return itos(static_cast<unsigned int>(time/(7 * 24 * 60 * 60 * 1000))) + "w";
		else if (time > 24 * 60 * 60 * 1000)
			return itos(static_cast<unsigned int>(time/(24 * 60 * 60 * 1000))) + "d";
		else if (time > 60 * 60 * 1000)
			return itos(static_cast<unsigned int>(time/(60 * 60 * 1000))) + "h";
		else if (time > 60 * 1000)
			return itos(static_cast<unsigned int>(time/(60 * 1000))) + "m";
		else if (time > 1000)
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
		unsigned __int64 cpy = i;
		char postfix[] = BKMG_RANGE;
		int idx = 0;
		while ((cpy > 1024)&&(idx<BKMG_SIZE)) {
			cpy/=1024;
			idx++;
		}
		std::string ret = itos(cpy);
		ret += postfix[idx];
		if (idx > 0) {
			ret += " (" + itos(i) + "B)";
		}
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

	inline std::pair<std::string,std::string> split(std::string str, std::string key) {
		std::string::size_type pos = str.find(key);
		if (pos == std::string::npos)
			return std::pair<std::string,std::string>(str, "");
		return std::pair<std::string,std::string>(str.substr(0, pos), str.substr(pos+key.length()));
	}
	typedef std::pair<std::string,std::string> token;
	inline token getToken(std::string buffer, char split) {
		std::string::size_type pos = buffer.find(split);
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