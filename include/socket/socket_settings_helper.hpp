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

#include <boost/shared_ptr.hpp>

#include <socket/socket_helpers.hpp>
#include <nscapi/nscapi_settings_proxy.hpp>
#include <nscapi/nscapi_settings_helper.hpp>

namespace socket_helpers {
	struct settings_helper {
		static void add_port_server_opts(nscapi::settings_helper::settings_registry &settings, socket_helpers::connection_info &info_, std::string default_port) {
			settings.alias().add_key_to_settings()
				("port", nscapi::settings_helper::string_key(&info_.port_, default_port),
					"PORT NUMBER", "Port to use for check_nt.")
				;
		}

		static void add_ssl_server_opts(nscapi::settings_helper::settings_registry &settings, socket_helpers::connection_info &info_, bool ssl_default, std::string certificate = "${certificate-path}/certificate.pem", std::string key = "", std::string default_cipher = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") {
			settings.alias().add_key_to_settings()

				("use ssl", nscapi::settings_helper::bool_key(&info_.ssl.enabled, ssl_default),
					"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.", false)

				("dh", nscapi::settings_helper::path_key(&info_.ssl.dh_key, "${certificate-path}/nrpe_dh_512.pem"),
					"DH KEY", "", true)

				("certificate", nscapi::settings_helper::path_key(&info_.ssl.certificate, certificate),
					"SSL CERTIFICATE", "", true)

				("certificate key", nscapi::settings_helper::path_key(&info_.ssl.certificate_key, key),
					"SSL CERTIFICATE", "", true)

				("certificate format", nscapi::settings_helper::string_key(&info_.ssl.certificate_format, "PEM"),
					"CERTIFICATE FORMAT", "", true)

				("ca", nscapi::settings_helper::path_key(&info_.ssl.ca_path, "${certificate-path}/ca.pem"),
					"CA", "", true)

				("allowed ciphers", nscapi::settings_helper::string_key(&info_.ssl.allowed_ciphers, default_cipher),
					"ALLOWED CIPHERS", "The chipers which are allowed to be used.\nThe default here will differ is used in \"insecure\" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html", true)

				("verify mode", nscapi::settings_helper::string_key(&info_.ssl.verify_mode, "none"),
					"VERIFY MODE", "Comma separated list of verification flags to set on the SSL socket.\n\n"
					"none\tThe server will not send a client certificate request to the client, so the client will not send a certificate.\n"
					"peer\tThe server sends a client certificate request to the client and the certificate returned (if any) is checked.\n"
					"fail-if-no-cert\tif the client did not return a certificate, the TLS/SSL handshake is immediately terminated. This flag must be used together with peer.\n"
					"peer-cert\tAlias for peer and fail-if-no-cert.\n"
					"workarounds\tVarious bug workarounds.\n"
					"single\tAlways create a new key when using tmp_dh parameters.\n"
					"client-once\tOnly request a client certificate on the initial TLS/SSL handshake. This flag must be used together with verify-peer\n"
					"\n\n", true)

				("ssl options", nscapi::settings_helper::string_key(&info_.ssl.ssl_options, ""),
					"VERIFY MODE", "Comma separated list of verification flags to set on the SSL socket.\n\n"
					"default-workarounds\tVarious workarounds for what I understand to be broken ssl implementations\n"
					"no-sslv2\tDo not use the SSLv2 protocol.\n"
					"no-sslv3\tDo not use the SSLv3 protocol.\n"
					"no-tlsv1\tDo not use the TLSv1 protocol.\n"
					"single-dh-use\tAlways create a new key when using temporary/ephemeral DH parameters. "
					"This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using \"strong\" primes (e.g. when using DSA-parameters).\n"
					"\n\n", true)
				;
		}

		static void add_core_server_opts(nscapi::settings_helper::settings_registry &settings, socket_helpers::connection_info &info_) {
			settings.alias().add_parent("/settings/default").add_key_to_settings()

				("thread pool", nscapi::settings_helper::uint_key(&info_.thread_pool_size, 10),
					"THREAD POOL", "", true)

				("socket queue size", nscapi::settings_helper::int_key(&info_.back_log, 0),
					"LISTEN QUEUE", "Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.", true)

				("bind to", nscapi::settings_helper::string_key(&info_.address),
					"BIND TO ADDRESS", "Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.")

				("allowed hosts", nscapi::settings_helper::string_fun_key(boost::bind(&socket_helpers::allowed_hosts_manager::set_source, &info_.allowed_hosts, _1), "127.0.0.1"),
					"ALLOWED HOSTS", "A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.")

				("cache allowed hosts", nscapi::settings_helper::bool_key(&info_.allowed_hosts.cached, true),
					"CACHE ALLOWED HOSTS", "If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.")

				("timeout", nscapi::settings_helper::uint_key(&info_.timeout, 30),
					"TIMEOUT", "Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.")

				;
		}

		template<class object_type>
		static void add_core_client_opts(nscapi::settings_helper::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("timeout", nscapi::settings_helper::int_fun_key(boost::bind(&object_type::set_property_int, &object, "timeout", _1), 30),
					"TIMEOUT", "Timeout when reading/writing packets to/from sockets.")
				;
		}
		template<class object_type>
		static void add_ssl_client_opts(nscapi::settings_helper::settings_registry &settings, boost::shared_ptr<nscapi::settings_proxy> proxy, object_type &object, bool is_sample) {
			nscapi::settings_helper::path_extension root_path = settings.path(object.tpl.path);
			if (is_sample)
				root_path.set_sample();
			root_path.add_key()

				("dh", nscapi::settings_helper::path_fun_key(boost::bind(&object_type::set_property_string, &object, "dh", _1), "${certificate-path}/nrpe_dh_512.pem"),
					"DH KEY", "", true)

				("certificate", nscapi::settings_helper::path_fun_key(boost::bind(&object_type::set_property_string, &object, "certificate", _1)),
					"SSL CERTIFICATE", "", false)

				("certificate key", nscapi::settings_helper::path_fun_key(boost::bind(&object_type::set_property_string, &object, "certificate key", _1)),
					"SSL CERTIFICATE", "", true)

				("certificate format", nscapi::settings_helper::string_fun_key(boost::bind(&object_type::set_property_string, &object, "certificate format", _1), "PEM"),
					"CERTIFICATE FORMAT", "", true)

				("ca", nscapi::settings_helper::path_fun_key(boost::bind(&object_type::set_property_string, &object, "ca", _1)),
					"CA", "", true)

				("allowed ciphers", nscapi::settings_helper::string_fun_key(boost::bind(&object_type::set_property_string, &object, "allowed ciphers", _1), "ADH"),
					"ALLOWED CIPHERS", "A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH", false)

				("verify mode", nscapi::settings_helper::string_fun_key(boost::bind(&object_type::set_property_string, &object, "verify mode", _1), "none"),
					"VERIFY MODE", "", false)

				("use ssl", nscapi::settings_helper::bool_fun_key(boost::bind(&object_type::set_property_bool, &object, "ssl", _1), true),
					"ENABLE SSL ENCRYPTION", "This option controls if SSL should be enabled.")
				;
		}
	};
}