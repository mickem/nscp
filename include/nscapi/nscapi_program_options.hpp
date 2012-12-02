#pragma once

#include <boost/program_options.hpp>
#include <boost/function/function1.hpp>
#include <list>
namespace nscapi {
	namespace program_options {
		std::vector<boost::program_options::option> option_parser(std::vector<std::string> &args) {
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
				//opt.original_tokens.push_back(*i);
				result.push_back(opt);
			}
			args.clear();
			return result;
		}
		class basic_command_line_parser : public boost::program_options::basic_command_line_parser<char> {
		public:

			static void invalid_syntax(const boost::program_options::options_description &desc, const std::string &command, Plugin::QueryResponseMessage::Response *response) {
				std::stringstream ss;
				ss << command << " syntax:" << std::endl;
				ss << desc;
				response->set_message(ss.str());
				response->set_result(Plugin::Common_ResultCode_UNKNOWN);
			}

			static void help_plumbing(const boost::program_options::options_description &desc, const std::string &command, Plugin::QueryResponseMessage::Response *response) {
				std::stringstream ss;
				ss << command << ",help" << std::endl;
				BOOST_FOREACH(const boost::shared_ptr<boost::program_options::option_description> op, desc.options()) {
					ss << op->long_name() << "," << op->description() << std::endl;
				}
				response->set_message(ss.str());
				response->set_result(Plugin::Common_ResultCode_UNKNOWN);
			}
			std::vector<std::basic_string<char> >make_vector(const Plugin::QueryRequestMessage::Request &request)
				{
					std::vector<std::basic_string<char> > result;
					for (int i=0;i<request.arguments_size();i++) {
						result.push_back(request.arguments(i));
					}
					return result;
				}
				basic_command_line_parser(const Plugin::QueryRequestMessage::Request &request) 
					: boost::program_options::basic_command_line_parser<char>(make_vector(request))
				{}
		};
	}
}