/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2017 Icinga Development Team (https://www.icinga.com/)  *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#ifndef TLSUTILITY_H
#define TLSUTILITY_H

// #include "base/i2-base.hpp"
// #include "base/object.hpp"
// #include "base/string.hpp"
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/comp.h>
#include <openssl/sha.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/exception/info.hpp>

namespace icinga
{

void  InitializeOpenSSL(void);
boost::shared_ptr<SSL_CTX>  MakeSSLContext(const std::string& pubkey = std::string(), const std::string& privkey = std::string(), const std::string& cakey = std::string());
void  AddCRLToSSLContext(const boost::shared_ptr<SSL_CTX>& context, const std::string& crlPath);
void  SetCipherListToSSLContext(const boost::shared_ptr<SSL_CTX>& context, const std::string& cipherList);
void  SetTlsProtocolminToSSLContext(const boost::shared_ptr<SSL_CTX>& context, const std::string& tlsProtocolmin);
String  GetCertificateCN(const boost::shared_ptr<X509>& certificate);
boost::shared_ptr<X509>  GetX509Certificate(const std::string& pemfile);
int  MakeX509CSR(const std::string& cn, const std::string& keyfile, const std::string& csrfile = std::string(), const std::string& certfile = std::string(), bool ca = false);
boost::shared_ptr<X509>  CreateCert(EVP_PKEY *pubkey, X509_NAME *subject, X509_NAME *issuer, EVP_PKEY *cakey, bool ca);
String  GetIcingaCADir(void);
String  CertificateToString(const boost::shared_ptr<X509>& cert);
boost::shared_ptr<X509>  StringToCertificate(const std::string& cert);
boost::shared_ptr<X509>  CreateCertIcingaCA(EVP_PKEY *pubkey, X509_NAME *subject);
boost::shared_ptr<X509>  CreateCertIcingaCA(const boost::shared_ptr<X509>& cert);
String  PBKDF2_SHA1(const std::string& password, const std::string& salt, int iterations);
String  SHA1(const std::string& s, bool binary = false);
String  SHA256(const std::string& s);
String  RandomString(int length);
bool  VerifyCertificate(const boost::shared_ptr<X509>& caCertificate, const boost::shared_ptr<X509>& certificate);

class  openssl_error : virtual public std::exception, virtual public boost::exception { };

struct errinfo_openssl_error_;
typedef boost::error_info<struct errinfo_openssl_error_, unsigned long> errinfo_openssl_error;

inline std::string to_string(const errinfo_openssl_error& e)
{
	std::ostringstream tmp;
	int code = e.value();
	char errbuf[120];

	const char *message = ERR_error_string(code, errbuf);

	if (message == NULL)
		message = "Unknown error.";

	tmp << code << ", \"" << message << "\"";
	return "[errinfo_openssl_error]" + tmp.str() + "\n";
}

}

#endif /* TLSUTILITY_H */
