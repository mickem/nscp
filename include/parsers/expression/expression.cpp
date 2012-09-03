#include <parsers/expression/expression.hpp>

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;
namespace phoenix = boost::phoenix;

typedef parsers::simple_expression::entry entry;
struct spirit_expression_parser {

	template<class Iterator>
	bool parse_raw(Iterator first, Iterator last, parsers::simple_expression::result_type& v) {
		using qi::_1;
		using qi::_3;
		using qi::_val;
		using phoenix::push_back;
		using qi::lexeme;
		qi::rule<Iterator, entry()> normal_rule;
		qi::rule<Iterator, entry()> variable_rule;
		normal_rule		= lexeme[+(qi::char_ - "${")]					[_val = phoenix::construct<entry>(false, _1)];
		variable_rule	= ("${" >> lexeme[+(qi::char_ - '}')] >> "}")	[_val = phoenix::construct<entry>(true, _1)];
		return qi::parse(first, last, 
			*(
				normal_rule			[ push_back(phoenix::ref(v), _1 )]
			||	
				variable_rule		[ push_back(phoenix::ref(v), _1 )]
			)
		);
	}
};

bool parsers::simple_expression::parse(const std::string &str, result_type& v) {
	spirit_expression_parser parser;
	return parser.parse_raw(str.begin(), str.end(), v);
}
