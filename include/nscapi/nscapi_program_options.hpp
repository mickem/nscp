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

#include <nscapi/nscapi_protobuf_functions.hpp>
#include <nscapi/nscapi_protobuf.hpp>

#include <str/utils.hpp>
#include <utf8.hpp>

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable:4512)
#pragma warning(disable:4100)
#include <boost/program_options.hpp>
#pragma warning(pop)
#else
#include <boost/program_options.hpp>
#endif
#include <boost/function/function1.hpp>
#include <boost/bind.hpp>

#include <list>
#include <vector>

namespace nscapi {
	namespace program_options {

		typedef std::map<std::string, std::string> field_map;

		namespace po = boost::program_options;

		class program_options_exception : public std::exception {
			std::string error;
		public:
			//////////////////////////////////////////////////////////////////////////
			/// Constructor takes an error message.
			/// @param error the error message
			///
			/// @author mickem
			program_options_exception(std::string error) : error(error) {}
			~program_options_exception() throw() {}

			//////////////////////////////////////////////////////////////////////////
			/// Retrieve the error message from the exception.
			/// @return the error message
			///
			/// @author mickem
			const char* what() const throw() { return error.c_str(); }

		};


		static std::vector<po::option> option_parser_kvp(std::vector<std::string> &args, const std::string &break_at) {
			std::vector<po::option> result;
			std::vector<std::string>::iterator it;
			for (it = args.begin(); it != args.end(); ++it) {
				po::option opt;
				opt.original_tokens.push_back(*it);
				std::string::size_type pos = (*it).find('=');
				if (pos == std::string::npos) {
					opt.string_key = (*it);
					if (!break_at.empty() && (*it) == break_at) {
						++it;
						for (;it != args.end(); ++it) {
							opt.value.push_back((*it));
						}
						result.push_back(opt);
						break;
					}
				} else {
					opt.string_key = (*it).substr(0, pos);
					opt.value.push_back((*it).substr(pos+1));
				}
				result.push_back(opt);
			}
			args.clear();
			return result;
		}

		class basic_command_line_parser : public po::basic_command_line_parser<char> {
		public:


			std::vector<std::basic_string<char> >make_vector(const Plugin::QueryRequestMessage::Request &request)
			{
				std::vector<std::basic_string<char> > result;
				for (int i=0;i<request.arguments_size();i++) {
					result.push_back(request.arguments(i));
				}
				return result;
			}
			std::vector<std::basic_string<char> >make_vector(const Plugin::ExecuteRequestMessage::Request &request)
			{
				std::vector<std::basic_string<char> > result;
				for (int i=0;i<request.arguments_size();i++) {
					result.push_back(request.arguments(i));
				}
				return result;
			}
			std::vector<std::basic_string<char> >make_vector(const std::string &arguments)
			{
				std::vector<std::basic_string<char> > result;
				str::utils::parse_command(arguments, result);
				return result;
			}
			basic_command_line_parser(const Plugin::QueryRequestMessage::Request &request) 
				: po::basic_command_line_parser<char>(make_vector(request))
			{}
			basic_command_line_parser(const Plugin::ExecuteRequestMessage::Request &request) 
				: po::basic_command_line_parser<char>(make_vector(request))
			{}
			basic_command_line_parser(const std::string &arguments) 
				: po::basic_command_line_parser<char>(make_vector(arguments))
			{}
			basic_command_line_parser(const std::vector<std::string> &arguments) 
				: po::basic_command_line_parser<char>(arguments)
			{}
		};

		static void add_help(po::options_description &desc) {
			desc.add_options()
				("help",		"Show help screen (this screen)")
				("help-pb",		"Show help screen as a protocol buffer payload")
				("show-default","Show default values for a given command")
				("help-short",	"Show help screen (short format).")
				;
		}
		inline po::options_description create_desc(const std::string command) {
			po::options_description desc("Allowed options for " + command);
			add_help(desc);
			return desc;
		}
		inline po::options_description create_desc(const Plugin::QueryRequestMessage::Request &request) {
			return create_desc(request.command());
		}
		inline po::options_description create_desc(const Plugin::ExecuteRequestMessage::Request &request) {
			return create_desc(request.command());
		}

        /* Given a string 'par', that contains no newline characters
           outputs it to 'os' with wordwrapping, that is, as several
           line.

           Each output line starts with 'indent' space characters, 
           following by characters from 'par'. The total length of
           line is no longer than 'line_length'.
                                      
        */
        static void format_paragraph(std::ostream& os,
                              std::string par,
                              std::size_t indent,
                              std::size_t line_length)
        {                    
			bool extra_indent = false; // true if we want to add a tab char
            // Through reminder of this function, 'line_length' will
            // be the length available for characters, not including
            // indent.
            assert(indent < line_length);
            line_length -= indent;

            // index of tab (if present) is used as additional indent relative
            // to first_column_width if paragrapth is spanned over multiple
            // lines if tab is not on first line it is ignored
            std::string::size_type par_indent = par.find('\t');

            if (par_indent == std::string::npos)
                par_indent = 0;
            else {
				extra_indent = true;

                // only one tab per paragraph allowed
                if (count(par.begin(), par.end(), '\t') > 1)
                    boost::throw_exception(po::error(
                        "Only one tab per paragraph is allowed in the options description"));
          
                // erase tab from string
                //par.erase(par_indent, 1);

                // this assert may fail due to user error or 
                // environment conditions!
                assert(par_indent < line_length);

                // ignore tab if not on first line
                if (par_indent >= line_length)
                    par_indent = 0;
            }
          
            if (par.size() < line_length)
                os << par;
            else {
                std::string::const_iterator       line_begin = par.begin();
                const std::string::const_iterator par_end = par.end();

                bool first_line = true; // of current paragraph!  
            
                while (line_begin < par_end)  // paragraph lines
                {
                    if (!first_line)
                    {
                        // If line starts with space, but second character
                        // is not space, remove the leading space.
                        // We don't remove double spaces because those
                        // might be intentianal.
                        if ((*line_begin == ' ') &&
                            ((line_begin + 1 < par_end) &&
                             (*(line_begin + 1) != ' ')))
                        {
                            line_begin += 1;  // line_begin != line_end
                        }
                    }

                    // Take care to never increment the iterator past
                    // the end, since MSVC 8.0 (brokenly), assumes that
                    // doing that, even if no access happens, is a bug.
                    unsigned remaining = static_cast<unsigned>(std::distance(line_begin, par_end));
                    std::string::const_iterator line_end = line_begin + 
                        ((remaining < line_length) ? remaining : line_length);
            
                    // prevent chopped words
                    // Is line_end between two non-space characters?
                    if ((*(line_end - 1) != ' ') &&
                        ((line_end < par_end) && (*line_end != ' ')))
                    {
                        // find last ' ' in the second half of the current paragraph line
                        std::string::const_iterator last_space =
                            std::find(std::reverse_iterator<std::string::const_iterator>(line_end),
                                 std::reverse_iterator<std::string::const_iterator>(line_begin),
                                 ' ')
                            .base();
                
                        if (last_space != line_begin)
                        {                 
                            // is last_space within the second half ot the 
                            // current line
                            if (static_cast<unsigned>(std::distance(last_space, line_end)) < 
                                (line_length / 2))
                            {
                                line_end = last_space;
                            }
                        }
                    } // prevent chopped words
             
                    // write line to stream
                    copy(line_begin, line_end, std::ostream_iterator<char>(os));
              
                    if (first_line)
                    {
                        indent += static_cast<unsigned>(par_indent);
                        line_length -= static_cast<unsigned>(par_indent); // there's less to work with now
                        first_line = false;
                    }

                    // more lines to follow?
                    if (line_end != par_end)
                    {
                        os << '\n';
                
                        for(std::size_t pad = indent; pad > 0; --pad)
                            os.put(' ');
    					if (extra_indent)
							os.put('\t');
                    }
              
                    // next line starts after of this line
                    line_begin = line_end;              
                } // paragraph lines
            }
        }

		static void format_description(std::ostream& os,
			const std::string& desc, 
			std::size_t first_column_width,
			unsigned line_length)
		{
			// we need to use one char less per line to work correctly if actual
			// console has longer lines
			assert(line_length > 1);
			if (line_length > 1)
			{
				--line_length;
			}

			// line_length must be larger than first_column_width
			// this assert may fail due to user error or environment conditions!
			assert(line_length > first_column_width);

			// Note: can't use 'tokenizer' as name of typedef -- borland
			// will consider uses of 'tokenizer' below as uses of
			// boost::tokenizer, not typedef.
			typedef boost::tokenizer<boost::char_separator<char> > tok;

			tok paragraphs(
				desc,
				boost::char_separator<char>("\n", "", boost::keep_empty_tokens));

			tok::const_iterator       par_iter = paragraphs.begin();                
			const tok::const_iterator par_end = paragraphs.end();

			while (par_iter != par_end)  // paragraphs
			{
				format_paragraph(os, *par_iter, first_column_width, 
					line_length);

				++par_iter;

				// prepair next line if any
				if (par_iter != par_end)
				{
					os << '\n';

					for(std::size_t pad = first_column_width; pad > 0; --pad)
					{
						os.put(' ');
					}                    
				}            
			}  // paragraphs
		}

		static std::string strip_default_value(const std::string &arg) {
			if (arg.size() > 3) {
				std::string ret;
				if (arg[arg.size()-1] == ')')
					ret = arg.substr(0, arg.size()-1);
				if (arg[arg.size()-1] == ']')
					ret = arg.substr(0, arg.size()-2);
				str::utils::replace(ret, "arg (=", "");
				str::utils::replace(ret, "[=arg(=", "");
				if (ret == "arg")
					return "";
				return ret;
			} else {
				if (arg == "arg")
					return "";
				return arg;
			}
		}

		static std::string help(const po::options_description &desc, const std::string &extra_info = "") {
			std::stringstream main_stream;
			if (!extra_info.empty())
				main_stream << extra_info  << std::endl;
			std::string::size_type opwidth = 23;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				if (op->long_name().size() > opwidth)
					opwidth = op->long_name().size();
			}
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				std::stringstream ss;
				ss << "  " << op->long_name();
				bool hasargs = op->semantic()->max_tokens() != 0;
				if (hasargs)
					ss << "=ARG";
				main_stream << ss.str();

				if (ss.str().size() >= opwidth) {
					main_stream.put('\n'); // first column is too long, lets put description in new line
					for (std::string::size_type pad = opwidth; pad > 0; --pad)
						main_stream.put(' ');
				} else {
					for(std::string::size_type pad = opwidth - ss.str().size(); pad > 0; --pad)
						main_stream.put(' ');
				}
				format_description(main_stream, op->description(), opwidth, 80);
				main_stream << "\n";

				if (hasargs) {
					std::string arg = op->format_parameter();
					if (arg.size() > 3) {
						for (std::string::size_type pad = opwidth; pad > 0; --pad)
							main_stream.put(' ');
						main_stream << "Default value: " << op->key("") << "=" << strip_default_value(arg) << "\n";
					}
				}
			}
			return main_stream.str();
		}

		static std::string help_short(const po::options_description &desc, const std::string &extra_info = "") {
			std::stringstream main_stream;
			if (!extra_info.empty())
				main_stream << extra_info  << std::endl;
			std::string::size_type opwidth = 0;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				if (op->long_name().size() > opwidth)
					opwidth = op->long_name().size();
				if (op->semantic()->max_tokens() != 0) {
					std::size_t len = op->long_name().size() + strip_default_value(op->format_parameter()).size() + 1;
					if (len > opwidth)
						opwidth = len;
				}
			}
			opwidth++;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				std::stringstream ss;
				ss << op->long_name();
				if (op->semantic()->max_tokens() != 0)
					ss << "=" << strip_default_value(op->format_parameter());
				main_stream << ss.str();

				for(std::string::size_type pad = opwidth - ss.str().size(); pad+8 > 8; pad-=8)
					main_stream.put('\t');
				std::string::size_type pos = op->description().find('\n');
				if (pos == std::string::npos)
					main_stream << op->description();
				else
					main_stream << op->description().substr(0, pos);
				main_stream << "\n";
			}
			return main_stream.str();
		}

		template<class T>
		void invalid_syntax(const po::options_description &desc, const std::string &, const std::string &error, T &response) {
			nscapi::protobuf::functions::set_response_bad(response, help_short(desc, error));
		}
		static std::string make_csv(const std::string s) {
			std::string ret = s;
			str::utils::replace(ret, "\n", "\\n");
			if (ret.find(',') != std::string::npos || ret.find('\"') != std::string::npos) {
				str::utils::replace(ret, "\"", "\\\"");
				return "\"" + ret + "\"";
			}
			return ret;
		}
		inline std::string help_csv(const po::options_description &desc, const std::string &) {
			std::stringstream main_stream;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				main_stream << make_csv(op->long_name()) << ",";
				bool hasargs = op->semantic()->max_tokens() != 0;
				if (hasargs)
					main_stream  << "true," << make_csv(strip_default_value(op->format_parameter())) << ",";
				else
					main_stream  << "false,,";
				main_stream << make_csv(op->description()) << "\n";
			}
			return main_stream.str();
		}


		inline std::string help_pb(const po::options_description &desc, const field_map &fields) {
			::Plugin::Registry::ParameterDetails details;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				::Plugin::Registry::ParameterDetail *detail = details.add_parameter();
				detail->set_name(op->long_name());
				bool hasargs = op->semantic()->max_tokens() != 0;
				if (hasargs) {
					detail->set_content_type(Plugin::Common::STRING);
					detail->set_default_value(strip_default_value(op->format_parameter()));
				} else
					detail->set_content_type(Plugin::Common::BOOL);
				std::string desc =op->description();
				std::string::size_type pos = desc.find("\n");
				if (pos == std::string::npos)
					detail->set_short_description(desc);
				else
					detail->set_short_description(desc.substr(0, pos));
				detail->set_long_description(desc);
			}
			BOOST_FOREACH(const field_map::value_type &v, fields) {
				::Plugin::Registry::FieldDetail *field= details.add_fields();
				field->set_name(v.first);
				field->set_long_description(v.second);
			}
			return details.SerializeAsString();
		}

		inline std::string help_pb(const po::options_description &desc) {
			return help_pb(desc, field_map());
		}


		inline std::string help_show_default(const po::options_description &desc) {
			std::stringstream ret;
			BOOST_FOREACH(const boost::shared_ptr<po::option_description> op, desc.options()) {
				std::string param = strip_default_value(op->format_parameter());
				if (param.empty())
					continue;
				ret << "\"" << op->long_name() << "=";
				ret << param << "\" ";
			}
			return ret.str();
		}

		typedef std::vector<std::string> unrecognized_map;
		


		template<class U>
		bool du_parse(po::variables_map &vm, const po::options_description &desc, U &response) {
			if (vm.count("show-default")) {
				nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
				return false;
			}
			if (vm.count("help-pb")) {
				nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc));
				return false;
			}
			if (vm.count("help-short")) {
				nscapi::protobuf::functions::set_response_good(response, help_short(desc));
				return false;
			}
			if (vm.count("help")) {
				nscapi::protobuf::functions::set_response_good(response, help(desc));
				return false;
			}
			return true;
		}
		template<class U>
		bool du_parse(po::variables_map &vm, const po::options_description &desc, const field_map &fields, U &response) {
			if (vm.count("show-default")) {
				nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
				return false;
			}
			if (vm.count("help-pb")) {
				nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc, fields));
				return false;
			}
			if (vm.count("help-short")) {
				nscapi::protobuf::functions::set_response_good(response, help_short(desc));
				return false;
			}
			if (vm.count("help")) {
				nscapi::protobuf::functions::set_response_good(response, help(desc));
				return false;
			}
			return true;
		}

		template<class T, class U>
		bool process_arguments_unrecognized(po::variables_map &vm, unrecognized_map &unrecognized, const po::options_description &desc, const T &request, U &response) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.allow_unregistered().run();
				po::store(parsed, vm);
				po::notify(vm);
				unrecognized_map un = po::collect_unrecognized(parsed.options, po::include_positional);
				unrecognized.insert(unrecognized.end(), un.begin(), un.end());

				return du_parse<U>(vm, desc, response);
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}
		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				return du_parse<U>(vm, desc, response);
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}

		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map fields, const T &request, U &response) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				return du_parse<U>(vm, desc, fields, response);
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}

		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response, po::positional_options_description p) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				cmd.positional(p);

				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() < 2 || (a[0] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, p.name_for_position(0)));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				return du_parse<U>(vm, desc, response);
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}

		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map &fields, const T &request, U &response, po::positional_options_description p) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				cmd.positional(p);

				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() < 2 || (a[0] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, p.name_for_position(0)));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				return du_parse<U>(vm, desc, fields, response);
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}

		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const T &request, U &response, bool allow_unknown, std::vector<std::string> &extra) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (allow_unknown)
					cmd.allow_unregistered();

				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				if (!du_parse<U>(vm, desc, response)) {
					return false;
				}
				if (allow_unknown) {
					std::vector<std::string> extra2 = po::collect_unrecognized(parsed.options, po::include_positional);
					extra.insert(extra.begin(), extra2.begin(), extra2.end());
				}
				return true;
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}
		template<class T, class U>
		bool process_arguments_from_request(po::variables_map &vm, const po::options_description &desc, const field_map &fields, const T &request, U &response, bool allow_unknown, std::vector<std::string> &extra) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (allow_unknown)
					cmd.allow_unregistered();

				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				if (!du_parse<U>(vm, desc, fields, response)) {
					return false;
				}
				if (allow_unknown) {
					std::vector<std::string> extra2 = po::collect_unrecognized(parsed.options, po::include_positional);
					extra.insert(extra.begin(), extra2.begin(), extra2.end());
				}
				return true;
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}
		template<class T>
		bool process_arguments_from_vector(po::variables_map &vm, const po::options_description &desc, const std::string &command, const std::vector<std::string> &arguments, T &response) {
			try {
				basic_command_line_parser cmd(arguments);
				cmd.options(desc);
				if (arguments.size() > 0) {
					std::string a = arguments[0];
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(boost::bind(nscapi::program_options::option_parser_kvp, _1, ""));
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				if (vm.count("show-default")) {
					nscapi::protobuf::functions::set_response_good(response, help_show_default(desc));
					return false;
				}
				if (vm.count("help-pb")) {
					nscapi::protobuf::functions::set_response_good_wdata(response, help_pb(desc));
					return false;
				}
				if (vm.count("help-short")) {
					nscapi::protobuf::functions::set_response_good(response, help_short(desc));
					return false;
				}
				if (vm.count("help")) {
					nscapi::protobuf::functions::set_response_good(response, help(desc));
					return false;
				}
				return true;
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, command, "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}

		struct alias_option {
			std::string key;
			std::string alias;
			std::string value;
		};
		struct standard_filter_config {
			std::string filter_string;
			std::string warn_string;
			std::string crit_string;
			std::string ok_string;
			std::string syntax_top;
			std::string syntax_ok;
			std::string syntax_empty;
			std::string syntax_detail;
			std::string empty_state;
		};

		inline void add_standard_filter(po::options_description &desc, standard_filter_config &filter, std::string default_top_syntax, std::string top_keylist, std::string default_syntax, std::string keylist) {
			desc.add_options()
				("filter", po::value<std::string>(&filter.filter_string),			"Filter which marks interesting items.\nInteresting items are items which will be included in the check. They do not denote warning or critical state but will be included in performance data and checked for critical and/or warning state. Anything not matching the filter will be ignored. Leaving the filter empty will include all applicable items")
				("warning", po::value<std::string>(&filter.warn_string),			"Filter which marks items which generates a warning state.\nIf anything matches this filter the return status will be escalated to warning.")
				("warn", po::value<std::string>(&filter.warn_string),				"Short alias for warning (see warning)")
				("critical", po::value<std::string>(&filter.crit_string),			"Filter which marks items which generates a critical state.\nIf anything matches this filter the return status will be escalated to critical.")
				("crit", po::value<std::string>(&filter.crit_string),				"Short alias for critical (see critical).")
				("ok", po::value<std::string>(&filter.ok_string),					"Filter which marks items which generates an ok state.\n"
				"If anything matches this any previous state for this item will be reset to ok. Thus this overrides any previous warning or critical state already set (for a specific item)"
				"Consider an item matching the following \"warning=foo > 500\" which escalates the item to warning status."
				"If the same item also matches the following ok filter \"ok=500 > 1000\" this will override the previous escalation and revert the status to ok.")
				("top-syntax", po::value<std::string>(&filter.syntax_top)->default_value(default_top_syntax), (std::string("Top level syntax.\n") + top_keylist).c_str())
				("ok-syntax", po::value<std::string>(&filter.syntax_ok), (std::string("Top level syntax for ok messages.\n") + top_keylist).c_str())
				("detail-syntax", po::value<std::string>(&filter.syntax_detail)->default_value(default_syntax), (std::string("Detail level syntax.\nHow each item in the lists of the top level syntax is rendered.\nAvailable keys are: \n") + keylist).c_str())
				("empty-syntax", po::value<std::string>(&filter.syntax_empty)->default_value("%(status): Nothing found..."), 
				"Message to display when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
				("empty-state", po::value<std::string>(&filter.empty_state)->default_value("ok"), 
				"Return status to use when nothing matched filter.\nIf no filter is specified this will never happen unless the file is empty.")
				;
		}
	}
}
