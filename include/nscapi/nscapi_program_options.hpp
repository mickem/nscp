#pragma once

#include <list>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/function/function1.hpp>
#include <nscapi/nscapi_protobuf_functions.hpp>

#include <checkHelpers.hpp>

namespace nscapi {
	namespace program_options {

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


		std::vector<boost::program_options::option> option_parser_kvp(std::vector<std::string> &args) {
			std::vector<boost::program_options::option> result;
			BOOST_FOREACH(const std::string &s, args) {
				boost::program_options::option opt;
				std::string::size_type pos = s.find('=');
				if (pos == std::string::npos) {
					opt.string_key = s;
				} else {
					opt.string_key = s.substr(0, pos);
					opt.value.push_back(s.substr(pos+1));
				}
				opt.original_tokens.push_back(s);
				result.push_back(opt);
			}
			args.clear();
			return result;
		}

		class basic_command_line_parser : public boost::program_options::basic_command_line_parser<char> {
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
				strEx::s::parse_command(arguments, result);
				return result;
			}
			basic_command_line_parser(const Plugin::QueryRequestMessage::Request &request) 
				: boost::program_options::basic_command_line_parser<char>(make_vector(request))
			{}
			basic_command_line_parser(const Plugin::ExecuteRequestMessage::Request &request) 
				: boost::program_options::basic_command_line_parser<char>(make_vector(request))
			{}
			basic_command_line_parser(const std::string &arguments) 
				: boost::program_options::basic_command_line_parser<char>(make_vector(arguments))
			{}
			basic_command_line_parser(const std::vector<std::string> &arguments) 
				: boost::program_options::basic_command_line_parser<char>(arguments)
			{}
		};

		po::options_description create_desc(const std::string command) {
			po::options_description desc("Allowed options for " + command);
			desc.add_options()
				("help",		"Show help screen (this screen)")
				("help-csv",	"Show help screen as a comma separated list. \nThis is useful for parsing the output in scripts and generate documentation etc")
				;
			return desc;
		}
		po::options_description create_desc(const Plugin::QueryRequestMessage::Request &request) {
			return create_desc(request.command());
		}
		po::options_description create_desc(const Plugin::ExecuteRequestMessage::Request &request) {
			return create_desc(request.command());
		}

        /* Given a string 'par', that contains no newline characters
           outputs it to 'os' with wordwrapping, that is, as several
           line.

           Each output line starts with 'indent' space characters, 
           following by characters from 'par'. The total length of
           line is no longer than 'line_length'.
                                      
        */
        void format_paragraph(std::ostream& os,
                              std::string par,
                              unsigned indent,
                              unsigned line_length)
        {                    
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
            {
                par_indent = 0;
            }
            else
            {
                // only one tab per paragraph allowed
                if (count(par.begin(), par.end(), '\t') > 1)
                {
                    boost::throw_exception(boost::program_options::error(
                        "Only one tab per paragraph is allowed in the options description"));
                }
          
                // erase tab from string
                par.erase(par_indent, 1);

                // this assert may fail due to user error or 
                // environment conditions!
                assert(par_indent < line_length);

                // ignore tab if not on first line
                if (par_indent >= line_length)
                {
                    par_indent = 0;
                }            
            }
          
            if (par.size() < line_length)
            {
                os << par;
            }
            else
            {
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
                
                        for(unsigned pad = indent; pad > 0; --pad)
                        {
                            os.put(' ');
                        }                                                        
                    }
              
                    // next line starts after of this line
                    line_begin = line_end;              
                } // paragraph lines
            }          
        }                              

		void format_description(std::ostream& os,
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

					for(unsigned pad = first_column_width; pad > 0; --pad)
					{
						os.put(' ');
					}                    
				}            
			}  // paragraphs
		}

		std::string strip_default_value(const std::string &arg) {
			if (arg.size() > 3) {
				std::string ret;
				if (arg[arg.size()-1] == ')')
					ret = arg.substr(0, arg.size()-1);
				if (arg[arg.size()-1] == ']')
					ret = arg.substr(0, arg.size()-2);
				strEx::replace(ret, "arg (=", "");
				strEx::replace(ret, "[=arg(=", "");
				return ret;
			} else {
				return arg;
			}
		}

		std::string help(const boost::program_options::options_description &desc, const std::string &extra_info = "") {
			std::stringstream main_stream;
			if (!extra_info.empty())
				main_stream << extra_info  << std::endl;
			std::string::size_type opwidth = 23;
			BOOST_FOREACH(const boost::shared_ptr<boost::program_options::option_description> op, desc.options()) {
				if (op->long_name().size() > opwidth)
					opwidth = op->long_name().size();
			}
			BOOST_FOREACH(const boost::shared_ptr<boost::program_options::option_description> op, desc.options()) {
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

		template<class T>
		void invalid_syntax(const boost::program_options::options_description &desc, const std::string &command, const std::string &error, T &response) {
			nscapi::protobuf::functions::set_response_bad(response, help(desc, error));
		}
		std::string make_csv(const std::string s) {
			std::string ret = s;
			strEx::replace(ret, "\n", "\\n");
			if (ret.find(',') != std::string::npos || ret.find('\"') != std::string::npos) {
				strEx::replace(ret, "\"", "\\\"");
				return "\"" + ret + "\"";
			}
			return ret;
		}
		std::string help_csv(const boost::program_options::options_description &desc, const std::string &command) {
			std::stringstream main_stream;
			BOOST_FOREACH(const boost::shared_ptr<boost::program_options::option_description> op, desc.options()) {
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

		typedef std::vector<std::string> unrecognized_map;
		
		template<class T, class U>
		bool process_arguments_unrecognized(boost::program_options::variables_map &vm, unrecognized_map &unrecognized, const boost::program_options::options_description &desc, const T &request, U &response) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(nscapi::program_options::option_parser_kvp);
				}

				po::parsed_options parsed = cmd.allow_unregistered().run();
				po::store(parsed, vm);
				po::notify(vm);
				unrecognized_map un = po::collect_unrecognized(parsed.options, po::include_positional);
				unrecognized.insert(unrecognized.end(), un.begin(), un.end());

				if (vm.count("help-csv")) {
					nscapi::protobuf::functions::set_response_good(response, help_csv(desc, request.command()));
					return false;
				}
				if (vm.count("help")) {
					nscapi::protobuf::functions::set_response_good(response, help(desc));
					return false;
				}
				return true;
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}
		template<class T, class U>
		bool process_arguments_from_request(boost::program_options::variables_map &vm, const boost::program_options::options_description &desc, const T &request, U &response) {
			try {
				basic_command_line_parser cmd(request);
				cmd.options(desc);
				if (request.arguments_size() > 0) {
					std::string a = request.arguments(0);
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(nscapi::program_options::option_parser_kvp);
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				if (vm.count("help-csv")) {
					nscapi::protobuf::functions::set_response_good(response, help_csv(desc, request.command()));
					return false;
				}
				if (vm.count("help")) {
					nscapi::protobuf::functions::set_response_good(response, help(desc));
					return false;
				}
				return true;
			} catch (const std::exception &e) {
				nscapi::program_options::invalid_syntax(desc, request.command(), "Failed to parse command line re-run with help to get help: " + utf8::utf8_from_native(e.what()), response);
				return false;
			}
		}
		template<class T>
		bool process_arguments_from_vector(boost::program_options::variables_map &vm, const boost::program_options::options_description &desc, const std::string &command, const std::vector<std::string> &arguments, T &response) {
			try {
				basic_command_line_parser cmd(arguments);
				cmd.options(desc);
				if (arguments.size() > 0) {
					std::string a = arguments[0];
					if (a.size() <= 2 || (a[0] != '-' && a[1] != '-'))
						cmd.extra_style_parser(nscapi::program_options::option_parser_kvp);
				}

				po::parsed_options parsed = cmd.run();
				po::store(parsed, vm);
				po::notify(vm);

				if (vm.count("help-csv")) {
					nscapi::protobuf::functions::set_response_good(response, help_csv(desc, command));
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
		typedef std::list<alias_option> alias_map;
		alias_map parse_legacy_alias(unrecognized_map unrecognized, std::string default_key) {
			alias_map result;
			BOOST_FOREACH(const std::string &k, unrecognized) {
				std::string::size_type pos = k.find("=");
				alias_option op;
				if (pos == std::string::npos) {
					op.key = default_key;
					op.value = k;
				} else {
					op.key = k.substr(0, pos);
					op.value = k.substr(pos+1);
					pos = op.key.find(':');
					if (pos != std::string::npos) {
						op.alias = op.key.substr(pos+1);
						op.key = op.key.substr(0, pos);
					}
				}
				result.push_back(op);
			}
			return result;
		}
		namespace legacy {



			void add_nsclient(po::options_description &desc, bool &bOption) {
				desc.add_options()
					("nsclient", po::bool_switch(&bOption)->implicit_value(true),"Do not check drives only return data compatible with the check_nt command.");
			}
			void add_ignore_perf_data(po::options_description &desc, bool &bOption) {
				desc.add_options()
					("ignore-perf-data", po::bool_switch(&bOption)->implicit_value(false), "Do not return performance data.")
				;
			}
			void add_perf_unit(po::options_description &desc) {
				desc.add_options()
					("perf-unit", po::value<std::string>(), "Force performance data to use a given unit prevents scaling which can cause problems over time in some graphing solutions.")
					;
			}
			template<class T>
			void collect_perf_unit(const boost::program_options::variables_map &vm, T &obj) {
				if (vm.count("perf-unit"))
					obj.perf_unit = vm["perf-unit"].as<std::string>();
			}
			void add_show_all(po::options_description &desc) {
				desc.add_options()
					("ShowAll", po::value<std::string>()->implicit_value("short"), "Show all values not just problems.\nSome commands support setting this option to long or short to define how much information you want.")
					("ShowFail", po::value<bool>(), "Show all values not just problems.\nSome commands support setting this option to long or short to define how much information you want.")
					;
			}
			template<class T>
			void collect_show_all(const boost::program_options::variables_map &vm, T &obj) {
				if (vm.count("ShowAll")) {
					if (vm["ShowAll"].as<std::string>() == "long") {
						obj.show = checkHolders::showLong;
					} else {
						obj.show = checkHolders::showShort;
					}
				}
				if (vm.count("ShowFail")) {
					obj.show = checkHolders::showProblems;
				}
			}
			void add_disk_check(po::options_description &desc) {
#define ARGTYPES "Possible values include either a percentage or a fixed value with a unit suffix. For instance ARG=23% for 23% or ARG=23M for 23Mega bytes or ARG=23k for 23 kilo bytes."
				desc.add_options()
					("MaxWarnFree", po::value<std::string>(), "Deprecated option.\nMaximum free size before a warning is returned. " ARGTYPES)
					("MaxCritFree", po::value<std::string>(), "Deprecated option.\nMaximum free size before a critical is returned. " ARGTYPES)
					("MinWarnFree", po::value<std::string>(), "Deprecated option.\nMinimum free size before a warning is returned. " ARGTYPES)
					("MinCritFree", po::value<std::string>(), "Deprecated option.\nMinimum free size before a critical is returned. " ARGTYPES)
					("MaxWarnUsed", po::value<std::string>(), "Deprecated option.\nMaximum used size before a warning is returned. " ARGTYPES)
					("MaxCritUsed", po::value<std::string>(), "Deprecated option.\nMaximum used size before a critical is returned. " ARGTYPES)
					("MinWarnUsed", po::value<std::string>(), "Deprecated option.\nMinimum used size before a warning is returned. " ARGTYPES)
					("MinCritUsed", po::value<std::string>(), "Deprecated option.\nMinimum used size before a critical is returned. " ARGTYPES)
					("MaxWarn", po::value<std::string>(), "Maximum used size before a warning is returned.\n" ARGTYPES)
					("MaxCrit", po::value<std::string>(), "Maximum used size before a critical is returned.\n" ARGTYPES)
					("MinWarn", po::value<std::string>(), "Minimum free size before a warning is returned.\n" ARGTYPES)
					("MinCrit", po::value<std::string>(), "Minimum free size before a critical is returned.\n" ARGTYPES)
					;
			}
			template<class T>
			void collect_disk_check(const boost::program_options::variables_map &vm, T &obj) {
				if (vm.count("MaxWarnFree"))
					obj.warn.max_.upper = vm["MaxWarnFree"].as<std::string>();
				if (vm.count("MaxCritFree"))
					obj.crit.max_.upper = vm["MaxCritFree"].as<std::string>();
				if (vm.count("MinWarnFree"))
					obj.warn.min_.upper = vm["MinWarnFree"].as<std::string>();
				if (vm.count("MinCritFree"))
					obj.crit.min_.upper = vm["MinCritFree"].as<std::string>();
				if (vm.count("MaxWarnUsed"))
					obj.warn.max_.lower = vm["MaxWarnUsed"].as<std::string>();
				if (vm.count("MaxCritUsed"))
					obj.crit.max_.lower = vm["MaxCritUsed"].as<std::string>();
				if (vm.count("MinWarnUsed"))
					obj.warn.min_.lower = vm["MinWarnUsed"].as<std::string>();
				if (vm.count("MinCritUsed"))
					obj.crit.min_.lower = vm["MinCritUsed"].as<std::string>();
				if (vm.count("MaxWarn"))
					obj.warn.max_.lower = vm["MaxWarn"].as<std::string>();
				if (vm.count("MaxCrit"))
					obj.crit.max_.lower = vm["MaxCrit"].as<std::string>();
				if (vm.count("MinWarn"))
					obj.warn.min_.upper = vm["MinWarn"].as<std::string>();
				if (vm.count("MinCrit"))
					obj.crit.min_.upper = vm["MinCrit"].as<std::string>();
			}


			void add_numerical_all(po::options_description &desc) {
				desc.add_options()
					("MaxWarn", po::value<std::string>(), "Maximum number of matches before a warning is returned.")
					("MaxCrit", po::value<std::string>(), "Maximum number of matches before a critical is returned.")
					("MinWarn", po::value<std::string>(), "Minimum number of matches before a warning is returned.")
					("MinCrit", po::value<std::string>(), "Minimum number of matches before a critical is returned.")
					;
			}
			template<class T>
			void collect_numerical_all(const boost::program_options::variables_map &vm, T &obj) {
				if (vm.count("MaxWarn"))
					obj.warn.max_ = vm["MaxWarn"].as<std::string>();
				if (vm.count("MaxCrit"))
					obj.crit.max_ = vm["MaxCrit"].as<std::string>();
				if (vm.count("MinWarn"))
					obj.warn.min_ = vm["MinWarn"].as<std::string>();
				if (vm.count("MinCrit"))
					obj.crit.min_ = vm["MinCrit"].as<std::string>();
			}
			void add_exact_numerical_all(po::options_description &desc) {
				desc.add_options()
					("warn", po::value<std::string>(), "Expression which raises a warning status.")
					("crit", po::value<std::string>(), "Expression which raises a critical status.")
					;
			}
			template<class T>
			void collect_exact_numerical_all(const boost::program_options::variables_map &vm, T &obj) {
				if (vm.count("warn"))
					obj.warn = vm["warn"].as<std::string>();
				if (vm.count("crit"))
					obj.crit = vm["crit"].as<std::string>();
			}

		}
	}
}