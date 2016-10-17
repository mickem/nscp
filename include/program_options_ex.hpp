/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
