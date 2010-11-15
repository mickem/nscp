#pragma once

#include <boost/program_options.hpp>
#include <boost/function/function1.hpp>

template<class charT>
class basic_command_line_parser_ex : public boost::program_options::basic_command_line_parser<charT> {
public:
	static boost::program_options::basic_parsed_options<charT> parse_command_line(const charT* arg0, int argc, charT* argv[], const boost::program_options::options_description& desc,
		int style = 0, boost::function1<std::pair<std::string, std::string>, const std::string&> ext = boost::program_options::ext_parser())
	{
		return basic_command_line_parser_ex<charT>(arg0, argc, argv).options(desc).style(style).extra_parser(ext).run();
	}

	template<class charT, class Iterator>
		std::vector<std::basic_string<charT> > 
			make_vector(const charT *a0, Iterator i, Iterator e)
		{
			std::vector<std::basic_string<charT> > result;
			// Some compilers don't have templated constructor for 
			// vector, so we can't create vector from (argv+1, argv+argc) range
			if (a0 != NULL)
				result.push_back(a0);
			for(; i != e; ++i)
				result.push_back(*i);
			return result;            
		}
		basic_command_line_parser_ex(const charT* arg0, int argc, charT* argv[]) 
			: boost::program_options::basic_command_line_parser<charT>(
			make_vector<charT, charT**>(arg0, argv, argv+argc)
			)
		{}
		basic_command_line_parser_ex(const std::wstring args) 
			: boost::program_options::basic_command_line_parser<charT>(boost::program_options::split_winmain(args))
		{}
		basic_command_line_parser_ex(const std::vector<std::wstring> args) 
			: boost::program_options::basic_command_line_parser<charT>(args)
		{}
		basic_command_line_parser_ex(const std::list<std::wstring> args) 
			: boost::program_options::basic_command_line_parser<charT>(args)
		{}

};
