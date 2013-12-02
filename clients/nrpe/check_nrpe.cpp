#include "check_nrpe.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>


int main(int argc, char* argv[]) { 
	Plugin::QueryResponseMessage::Response response;
	std::vector<std::string> args;
	for (int i=1;i<argc; i++) {
		args.push_back(argv[i]);
	}

	check_nrpe client;
	client.query(args, response);
	std::cout << response.message();
	std::string tmp = nscapi::protobuf::functions::build_performance_data(response);
	if (!tmp.empty())
		std::cout << '|' << tmp;
}


check_nrpe::check_nrpe() {
	targets.ensure_default("/foo/bar");
}

struct client_handler : public socket_helpers::client::client_handler {
	void log_debug(std::string file, int line, std::string msg) const {
		std::cout << msg;
	}
	void log_error(std::string file, int line, std::string msg) const {
		std::cout << msg;
	}
};

void check_nrpe::query(const std::vector<std::string> &args, Plugin::QueryResponseMessage::Response &response) {
	client::configuration config(nrpe_client::command_prefix);
	config.target_lookup = boost::shared_ptr<nrpe_client::target_handler>(new nrpe_client::target_handler(targets)); 
	config.handler = boost::shared_ptr<nrpe_client::clp_handler_impl>(new nrpe_client::clp_handler_impl(boost::shared_ptr<socket_helpers::client::client_handler>(new client_handler())));
	const ::Plugin::Common::Header header;
	nrpe_client::setup(config, header);
	commands.parse_query(config, args, response);
}
