/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#define  BOOST_PROGRAM_OPTIONS_DYN_LINK

#include <boost/program_options.hpp>
#include <boost/function/function1.hpp>
#include <list>

template<class charT>
class basic_command_line_parser_ex : public boost::program_options::basic_command_line_parser<charT> {
public:
	/*
	static boost::program_options::basic_parsed_options<charT> parse_command_line(int argc, charT* argv[], const boost::program_options::options_description& desc,
		int style = 0, boost::function1<std::pair<std::string, std::string>, const std::string&> ext = boost::program_options::ext_parser())
	{
		return basic_command_line_parser_ex<charT>(argc, argv).options(desc).style(style).extra_parser(ext).run();
	}
	*/

	template<class charTx, class Iterator>
		std::vector<std::basic_string<charTx> > 
			make_vector(Iterator i, Iterator e)
		{
			std::vector<std::basic_string<charTx> > result;
			// Some compilers don't have templated constructor for 
			// vector, so we can't create vector from (argv+1, argv+argc) range
			//if (a0 != NULL)
			//	result.push_back(a0);
			for(; i != e; ++i)
				result.push_back(*i);
			return result;            
		}
		basic_command_line_parser_ex(int argc, charT* argv[]) 
			: boost::program_options::basic_command_line_parser<charT>(
			make_vector<charT, charT**>(argv, argv+argc)
			)
		{}
		basic_command_line_parser_ex(std::vector<charT> v) 
			: boost::program_options::basic_command_line_parser<charT>(v)
		{}

		basic_command_line_parser_ex(std::list<std::basic_string<charT> > &v) 
			: boost::program_options::basic_command_line_parser<charT>(
			make_vector<std::list<std::basic_string<charT> >::iterator, std::list<std::basic_string<charT> >::iterator>(v.begin(), v.end())
			)
		{}

};
