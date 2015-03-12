#include "nrpe_client.hpp"

#include <nscapi/nscapi_protobuf_functions.hpp>

namespace po = boost::program_options;
namespace sh = nscapi::settings_helper;

namespace nrpe_client {

		void custom_reader::init_default(target_object &target) {
			target.set_property_int("timeout", 30);
			//target.set_property_string("dh", "${certificate-path}/nrpe_dh_512.pem");
			target.set_property_string("certificate", "${certificate-path}/certificate.pem");
			target.set_property_string("certificate key", "");
			target.set_property_string("certificate format", "PEM");
			target.set_property_string("allowed ciphers", "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
			target.set_property_string("verify mode", "none");
			if (!target.has_option("insecure"))
				target.set_property_bool("insecure", false);
			target.set_property_bool("ssl", true);
			target.set_property_int("payload length", 1024);
		}

		void custom_reader::add_custom_keys(sh::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("timeout", sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "timeout", _1), 30),
				"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")

				("dh", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "dh", _1)),
				"DH KEY", "The diffi-hellman perfect forwarded secret to use setting --insecure will override this", true)

				("certificate", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate", _1)),
				"SSL CERTIFICATE", "The ssl certificate to use to encrypt the communication", false)

				("certificate key", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate key", _1)),
				"SSL CERTIFICATE KEY", "Key for the SSL certificate", false)

				("certificate format", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "certificate format", _1), "PEM"),
				"CERTIFICATE FORMAT", "Format of SSL certificate", true)

				("ca", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "ca", _1)),
				"CA", "The certificate authority to use to authenticate remote certificate", true)

				("insecure", sh::path_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "insecure", _1)),
				"Insecure legacy mode", "Use insecure legacy mode to connect to old NRPE server", false)

				("allowed ciphers", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "allowed ciphers", _1), "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"),
				"ALLOWED CIPHERS", "The allowed list of ciphers (setting insecure wil override this to only support ADH", true)

				("verify mode", sh::string_fun_key<std::string>(boost::bind(&object_type::set_property_string, &object, "verify mode", _1), "none"),
				"VERIFY MODE", "What to verify default is non, to validate remote certificate use remote-peer", false)

				("use ssl", sh::bool_fun_key<bool>(boost::bind(&object_type::set_property_bool, &object, "ssl", _1), true),
				"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")

				("payload length",  sh::int_fun_key<int>(boost::bind(&object_type::set_property_int, &object, "payload length", _1), 1024),
				"PAYLOAD LENGTH", "Length of payload to/from the NRPE agent. This is a hard specific value so you have to \"configure\" (read recompile) your NRPE agent to use the same value for it to work.")
				;
		}

		void custom_reader::post_process_target(target_object &target) {
			std::list<std::string> err;
			nscapi::targets::helpers::verify_file(target, "certificate", err);
			nscapi::targets::helpers::verify_file(target, "dh", err);
			nscapi::targets::helpers::verify_file(target, "certificate key", err);
			nscapi::targets::helpers::verify_file(target, "ca", err);
// 			BOOST_FOREACH(const std::string &e, err) {
// 				NSC_LOG_ERROR(e);
// 			}
		}


		std::string get_command(std::string alias, std::string command = "") {
			if (!alias.empty())
				return alias; 
			if (!command.empty())
				return command; 
			return "_NRPE_CHECK";
		}



		nrpe_client::connection_data clp_handler_impl::parse_header(const ::Plugin::Common_Header &header, client::configuration::data_type data) {
			nscapi::protobuf::functions::destination_container recipient;
			nscapi::protobuf::functions::parse_destination(header, header.recipient_id(), recipient, true);
			nrpe_client::connection_data ret = nrpe_client::connection_data(recipient, data->recipient);
			if (data->recipient.get_bool_data("insecure")) {
				ret.ssl.ca_path = "";
				ret.ssl.certificate = "";
				ret.ssl.certificate_key = "";
				ret.ssl.allowed_ciphers = "ADH";
				ret.ssl.dh_key = client_handler->expand_path("${certificate-path}/nrpe_dh_512.pem");
			} else {
				ret.ssl.ca_path = client_handler->expand_path(ret.ssl.ca_path);
				ret.ssl.certificate = client_handler->expand_path(ret.ssl.certificate);
				ret.ssl.certificate_key = client_handler->expand_path(ret.ssl.certificate_key);
			}
			return ret;
		}



		//////////////////////////////////////////////////////////////////////////
		// Parser implementations
		//

		int clp_handler_impl::query(client::configuration::data_type data, const Plugin::QueryRequestMessage &request_message, Plugin::QueryResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con = parse_header(request_header, data);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();i++) {
				std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++) {
					data += "!" + request_message.payload(i).arguments(a);
				}
				boost::tuple<int,std::string> ret = send(con, data);
				strEx::s::token rdata = strEx::s::getToken(ret.get<1>(), '|');
				nscapi::protobuf::functions::append_simple_query_response_payload(response_message.add_payload(), command, ret.get<0>(), rdata.first, rdata.second);
			}
			return NSCAPI::isSuccess;
		}

		int clp_handler_impl::submit(client::configuration::data_type data, const Plugin::SubmitRequestMessage &request_message, Plugin::SubmitResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con = parse_header(request_header, data);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();++i) {
				std::string command = get_command(request_message.payload(i).alias(), request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++) {
					data += "!" + request_message.payload(i).arguments(i);
				}
				boost::tuple<int,std::string> ret = send(con, data);
				nscapi::protobuf::functions::append_simple_submit_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
			}
			return NSCAPI::isSuccess;
		}

		int clp_handler_impl::exec(client::configuration::data_type data, const Plugin::ExecuteRequestMessage &request_message, Plugin::ExecuteResponseMessage &response_message) {
			const ::Plugin::Common_Header& request_header = request_message.header();
			nrpe_client::connection_data con = parse_header(request_header, data);

			nscapi::protobuf::functions::make_return_header(response_message.mutable_header(), request_header);

			for (int i=0;i<request_message.payload_size();i++) {
				std::string command = get_command(request_message.payload(i).command());
				std::string data = command;
				for (int a=0;a<request_message.payload(i).arguments_size();a++)
					data += "!" + request_message.payload(i).arguments(a);
				boost::tuple<int,std::string> ret = send(con, data);
				nscapi::protobuf::functions::append_simple_exec_response_payload(response_message.add_payload(), command, ret.get<0>(), ret.get<1>());
			}
			return NSCAPI::isSuccess;
		}

		//////////////////////////////////////////////////////////////////////////
		// Protocol implementations
		//

		boost::tuple<int,std::string> clp_handler_impl::send(nrpe_client::connection_data con, const std::string data) {
			try {
#ifndef USE_SSL
				if (con.ssl.enabled)
					return boost::make_tuple(NSCAPI::returnUNKNOWN, "SSL support not available (compiled without USE_SSL)");
#endif
				nrpe::packet packet = nrpe::packet::make_request(data, con.buffer_length);
				socket_helpers::client::client<nrpe::client::protocol> client(con, client_handler);
				client.connect();
				std::list<nrpe::packet> responses = client.process_request(packet);
				client.shutdown();
				int result = NSCAPI::returnUNKNOWN;
				std::string payload;
				if (responses.size() > 0)
					result = static_cast<int>(responses.front().getResult());
				BOOST_FOREACH(const nrpe::packet &p, responses) {
 					payload += p.getPayload();
				}
				return boost::make_tuple(result, payload);
			} catch (nrpe::nrpe_exception &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, std::string("NRPE Packet error: ") + e.what());
			} catch (std::runtime_error &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Socket error: " + utf8::utf8_from_native(e.what()));
			} catch (std::exception &e) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Error: " + utf8::utf8_from_native(e.what()));
			} catch (...) {
				return boost::make_tuple(NSCAPI::returnUNKNOWN, "Unknown error -- REPORT THIS!");
			}
		}

		void add_local_options(po::options_description &desc, client::configuration::data_type data) {
			desc.add_options()
				("no-ssl,n", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::nscp_cli_data::set_bool_data, data, "no ssl", _1)), 
				"Do not initial an ssl handshake with the server, talk in plain-text.")

				("certificate", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "certificate", _1)), 
				"Length of payload (has to be same as on the server)")

				("dh", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "dh", _1)), 
				"The pre-generated DH key (if ADH is used this will be your 'key' though it is not a secret key)")

				("certificate-key", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "certificate key", _1)), 
				"Client certificate to use")

				("certificate-format", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "certificate format", _1)), 
				"Client certificate format (default is PEM)")

				("insecure", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::nscp_cli_data::set_bool_data, data, "insecure", _1)), 
				"Use insecure legacy mode")

				("ca", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "ca", _1)), 
				"A file representing the Certificate authority used to validate peer certificates")

				("verify", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "verify mode", _1)), 
				"Which verification mode to use: none: no verification, peer: that peer has a certificate, peer-cert: that peer has a valid certificate, ...")

				("allowed-ciphers", po::value<std::string>()->notifier(boost::bind(&client::nscp_cli_data::set_string_data, data, "allowed ciphers", _1)), 
				"Which ciphers are allowed for legacy reasons this defaults to ADH which is not secure preferably set this to DEFAULT which is better or a an even stronger cipher")

				("payload-length,l", po::value<unsigned int>()->notifier(boost::bind(&client::nscp_cli_data::set_int_data, data, "payload length", _1)), 
				"Length of payload (has to be same as on the server)")

				("buffer-length", po::value<unsigned int>()->notifier(boost::bind(&client::nscp_cli_data::set_int_data, data, "payload length", _1)), 
				"Same as payload-length (used for legacy reasons)")

				("ssl", po::value<bool>()->zero_tokens()->default_value(false)->notifier(boost::bind(&client::nscp_cli_data::set_bool_data, data, "ssl", _1)), 
				"Initial an ssl handshake with the server.")
				;
		}


		nscapi::protobuf::types::destination_container target_handler::lookup_target(std::string &id) const {
			nscapi::targets::optional_target_object opt = targets_.find_object(id);
			if (opt)
				return opt->to_destination_container();
			nscapi::protobuf::types::destination_container ret;
			return ret;
		}

		bool target_handler::has_object(std::string alias) const {
			return targets_.has_object(alias);
		}
		bool target_handler::apply(nscapi::protobuf::types::destination_container &dst, const std::string key) {
			nscapi::targets::optional_target_object opt = targets_.find_object(key);
			if (opt)
				dst.apply(opt->to_destination_container());
			return static_cast<bool>(opt);
		}


		void setup(client::configuration &config, const ::Plugin::Common_Header& header) {
			add_local_options(config.local, config.data);

			config.data->recipient.id = header.recipient_id();
			config.default_command = default_command;
			std::string recipient = config.data->recipient.id;
			if (!config.target_lookup->has_object(recipient)) {
				recipient = "default";
			}
			config.target_lookup->apply(config.data->recipient, recipient);
			config.data->host_self.id = "self";
			//config.data->host_self.host = hostname_;
		}

}

