#include <string>
#include <algorithm>
#include <locale>
#include <ctype.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <nscpcrypt/nscpcrypt.hpp>

#ifdef HAVE_LIBCRYPTOPP
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-pedantic"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <cryptlib.h>
#include <modes.h>
#include <des.h>
#include <aes.h>
#include <cast.h>
#include <tea.h>
#include <3way.h>
#include <blowfish.h>
#include <twofish.h>
#include <rc2.h>
#include <arc4.h>
#include <serpent.h>
#include <gost.h>
#include <filters.h>
#include <osrng.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif

#define TRANSMITTED_IV_SIZE     128     /* size of IV to transmit - must be as big as largest IV needed for any crypto algorithm */

/********************* ENCRYPTION TYPES ****************/

#define ENCRYPT_NONE            0       /* no encryption */
#define ENCRYPT_XOR             1       /* not really encrypted, just obfuscated */

#ifdef HAVE_LIBCRYPTOPP
#define ENCRYPT_DES             2       /* DES */
#define ENCRYPT_3DES            3       /* 3DES or Triple DES */
#define ENCRYPT_CAST128         4       /* CAST-128 */
#define ENCRYPT_CAST256         5       /* CAST-256 */
#define ENCRYPT_XTEA            6       /* xTEA */
#define ENCRYPT_3WAY            7       /* 3-WAY */
#define ENCRYPT_BLOWFISH        8       /* SKIPJACK */
#define ENCRYPT_TWOFISH         9       /* TWOFISH */
#define ENCRYPT_LOKI97          10      /* LOKI97 */
#define ENCRYPT_RC2             11      /* RC2 */
#define ENCRYPT_ARCFOUR         12      /* RC4 */
#define ENCRYPT_RC6             13      /* RC6 */            /* UNUSED */
#define ENCRYPT_RIJNDAEL128     14      /* RIJNDAEL-128 */
#define ENCRYPT_RIJNDAEL192     15      /* RIJNDAEL-192 */
#define ENCRYPT_RIJNDAEL256     16      /* RIJNDAEL-256 */
#define ENCRYPT_MARS            17      /* MARS */           /* UNUSED */
#define ENCRYPT_PANAMA          18      /* PANAMA */         /* UNUSED */
#define ENCRYPT_WAKE            19      /* WAKE */
#define ENCRYPT_SERPENT         20      /* SERPENT */
#define ENCRYPT_IDEA            21      /* IDEA */           /* UNUSED */
#define ENCRYPT_ENIGMA          22      /* ENIGMA (Unix crypt) */
#define ENCRYPT_GOST            23      /* GOST */
#define ENCRYPT_SAFER64         24      /* SAFER-sk64 */
#define ENCRYPT_SAFER128        25      /* SAFER-sk128 */
#define ENCRYPT_SAFERPLUS       26      /* SAFER+ */
#endif
#define LAST_ENCRYPTION_ID 26

std::string nscp::encryption::helpers::get_crypto_string(std::string sep) {
	std::string ret;
	for (int i = 0; i < LAST_ENCRYPTION_ID; i++) {
		if (nscp::encryption::engine::hasEncryption(i)) {
			std::string name;
			try {
				nscp::encryption::any_encryption *core = nscp::encryption::engine::get_encryption_core(i);
				if (core) {
					name = core->getName();
					delete core;
				}

				if (ret.size() > 1)
					ret += sep;
				ret += encryption_to_string(i) + " = " + name;
			} catch (const std::exception &e) {
				// Dont print invalid cryptos
			}
		}
	}
	return ret;
}

int nscp::encryption::helpers::encryption_to_int(std::string encryption_raw) {
	std::string encryption = boost::algorithm::to_lower_copy(encryption_raw);
	if (encryption == "xor")
		return ENCRYPT_XOR;
#ifdef HAVE_LIBCRYPTOPP
	if (encryption == "des")
		return ENCRYPT_DES;
	if (encryption == "3des")
		return ENCRYPT_3DES;
	if (encryption == "cast128")
		return ENCRYPT_CAST128;
	if (encryption == "xtea")
		return ENCRYPT_XTEA;
	if (encryption == "3way")
		return ENCRYPT_3WAY;
	if (encryption == "blowfish")
		return ENCRYPT_BLOWFISH;
	if (encryption == "twofish")
		return ENCRYPT_TWOFISH;
	if (encryption == "rc2")
		return ENCRYPT_RC2;
	if (encryption == "rijndael128" || encryption == "aes128")
		return ENCRYPT_RIJNDAEL128;
	if (encryption == "rijndael192" || encryption == "aes192")
		return ENCRYPT_RIJNDAEL192;
	if (encryption == "rijndael256" || encryption == "aes256" || encryption == "aes")
		return ENCRYPT_RIJNDAEL256;
	if (encryption == "serpent")
		return ENCRYPT_SERPENT;
	if (encryption == "gost")
		return ENCRYPT_GOST;
#endif
	if (
		(encryption.size() == 1 && isdigit(encryption[0]))
		|| (encryption.size() > 1 && isdigit(encryption[0]) && isdigit(encryption[1]))
		) {
		int enc = atoi(encryption.c_str());
		if (enc == ENCRYPT_XOR
#ifdef HAVE_LIBCRYPTOPP
			|| enc == ENCRYPT_DES || enc == ENCRYPT_3DES || enc == ENCRYPT_CAST128 || enc == ENCRYPT_XTEA || enc == ENCRYPT_3WAY || enc == ENCRYPT_BLOWFISH || enc == ENCRYPT_TWOFISH || enc == ENCRYPT_RC2 || enc == ENCRYPT_RIJNDAEL128 || enc == ENCRYPT_RIJNDAEL192 || enc == ENCRYPT_RIJNDAEL256 || enc == ENCRYPT_SERPENT || enc == ENCRYPT_GOST
#endif
			)
			return enc;
	}
	return ENCRYPT_NONE;
}
std::string nscp::encryption::helpers::encryption_to_string(int encryption) {
	if (encryption == ENCRYPT_XOR)
		return "xor";
#ifdef HAVE_LIBCRYPTOPP
	if (encryption == ENCRYPT_DES)
		return "des";
	if (encryption == ENCRYPT_3DES)
		return "3des";
	if (encryption == ENCRYPT_CAST128)
		return "cast128";
	if (encryption == ENCRYPT_XTEA)
		return "xtea";
	if (encryption == ENCRYPT_3WAY)
		return "3way";
	if (encryption == ENCRYPT_BLOWFISH)
		return "blowfish";
	if (encryption == ENCRYPT_TWOFISH)
		return "twofish";
	if (encryption == ENCRYPT_RC2)
		return "rc2";
	if (encryption == ENCRYPT_RIJNDAEL128)
		return "aes128";
	if (encryption == ENCRYPT_RIJNDAEL192)
		return "aes192";
	if (encryption == ENCRYPT_RIJNDAEL256)
		return "aes";
	if (encryption == ENCRYPT_SERPENT)
		return "serpent";
	if (encryption == ENCRYPT_GOST)
		return "gost";
#endif
	if (encryption == ENCRYPT_NONE)
		return "none";
	return "unknown";
}

#ifdef HAVE_LIBCRYPTOPP
template <class TMethod>
class cryptopp_encryption : public nscp::encryption::any_encryption {
private:
	typedef CryptoPP::CFB_Mode_ExternalCipher::Encryption TEncryption;
	typedef CryptoPP::CFB_Mode_ExternalCipher::Decryption TDecryption;
	typedef typename TMethod::Encryption TCipher;
	TEncryption crypto_;
	TDecryption decrypto_;
	TCipher cipher_;
	int keysize_;
public:
	cryptopp_encryption() : keysize_(TMethod::DEFAULT_KEYLENGTH) {}
	cryptopp_encryption(int keysize) : keysize_(keysize) {}
	virtual ~cryptopp_encryption() {}
	int get_keySize() {
		return keysize_;
	}
	std::size_t get_blockSize() {
		return TMethod::BLOCKSIZE;
	}

	virtual void init(std::string password, std::string iv) {
		std::size_t blocksize = get_blockSize();
		if (blocksize > iv.size())
			throw nscp::encryption::encryption_exception("IV size for crypto algorithm exceeds limits");

		// Generate key buffer
		std::string::size_type keysize = get_keySize();
		char *key = new char[keysize + 1];
		if (key == NULL)
			throw nscp::encryption::encryption_exception("Could not allocate memory for encryption/decryption key");
		memset(key, 0, keysize);
		using namespace std;
		memcpy(key, password.c_str(), min(keysize, password.length()));
		std::string skey(key, keysize);
		delete[] key;

		try {
			cipher_.SetKey((const byte*)skey.c_str(), keysize);
			crypto_.SetCipherWithIV(cipher_, (const byte*)iv.c_str(), 1);
			decrypto_.SetCipherWithIV(cipher_, (const byte*)iv.c_str(), 1);
		} catch (...) {
			throw nscp::encryption::encryption_exception("Unknown exception when trying to setup crypto");
		}
	}
	void encrypt(std::string &buffer) {
		encrypt((unsigned char*)&*buffer.begin(), buffer.size());
	}
	void encrypt(unsigned char *buffer, std::size_t buffer_size) {
		/* encrypt each byte of buffer, one byte at a time (CFB mode) */
		try {
			for (std::size_t x = 0; x < buffer_size; x++)
				crypto_.ProcessData(&buffer[x], &buffer[x], 1);
		} catch (...) {
			throw nscp::encryption::encryption_exception("Unknown exception when trying to setup crypto");
		}
	}
	void decrypt(std::string &buffer) {
		decrypt((unsigned char*)&*buffer.begin(), buffer.size());
	}
	void decrypt(unsigned char *buffer, std::size_t buffer_size) {
		try {
			for (std::size_t x = 0; x < buffer_size; x++)
				decrypto_.ProcessData(&buffer[x], &buffer[x], 1);
		} catch (...) {
			throw nscp::encryption::encryption_exception("Unknown exception when trying to setup crypto");
		}
	}
	std::string getName() {
		return TMethod::StaticAlgorithmName();
	}
};
#endif
class no_encryption : public nscp::encryption::any_encryption {
public:
	int get_keySize() {
		return 0;
	}
	std::size_t get_blockSize() {
		return 1;
	}
	void init(std::string password, std::string iv) {}
	void encrypt(std::string &buffer) { /* std::cout << "USING NO ENCRYPTION * * * " << std::endl; */ }
	void decrypt(std::string &buffer) {}
	std::string getName() {
		return "No Encryption (not safe)";
	}
};
class xor_encryption : public nscp::encryption::any_encryption {
private:
	std::string iv_;
	std::string password_;
public:
	xor_encryption() {}
	~xor_encryption() {}
	int get_keySize() {
		return 0;
	}
	std::size_t get_blockSize() {
		return 1;
	}
	void init(std::string password, std::string iv) {
		iv_ = iv;
		password_ = password;
	}
	void encrypt(std::string &buffer) {
		/* rotate over IV we received from the server... */
		std::size_t buf_len = buffer.size();
		std::size_t iv_len = iv_.size();
		std::size_t pwd_len = password_.size();
		for (std::size_t y = 0, x = 0, z = 0; y < buf_len; y++, x++, z++) {
			/* keep rotating over IV */
			if (x >= iv_len)
				x = 0;
			buffer[y] ^= iv_[x];
			/* keep rotating over Password */
			if (z >= pwd_len)
				z = 0;
			buffer[y] ^= password_[z];
		}
	}
	void decrypt(std::string &buffer) {
		/* rotate over IV we received from the server... */
		std::size_t buf_len = buffer.size();
		std::size_t iv_len = iv_.size();
		std::size_t pwd_len = password_.size();
		for (std::size_t y = 0, x = 0, z = 0; y < buf_len; y++, x++, z++) {
			/* keep rotating over Password */
			if (z >= pwd_len)
				z = 0;
			buffer[y] ^= password_[z];
			/* keep rotating over IV */
			if (x >= iv_len)
				x = 0;
			buffer[y] ^= iv_[x];
		}
	}
	std::string getName() {
		return "XOR";
	}
};

bool nscp::encryption::engine::hasEncryption(int encryption_method) {
	switch (encryption_method) {
	case ENCRYPT_NONE:
	case ENCRYPT_XOR:
#ifdef HAVE_LIBCRYPTOPP
	case ENCRYPT_DES:
	case ENCRYPT_3DES:
	case ENCRYPT_CAST128:
	case ENCRYPT_XTEA:
	case ENCRYPT_BLOWFISH:
	case ENCRYPT_TWOFISH:
	case ENCRYPT_RC2:
	case ENCRYPT_RIJNDAEL128:
	case ENCRYPT_RIJNDAEL192:
	case ENCRYPT_RIJNDAEL256:
	case ENCRYPT_SERPENT:
	case ENCRYPT_GOST:
#endif
		return true;

		// UNdefined
#ifdef HAVE_LIBCRYPTOPP
	case ENCRYPT_3WAY:
	case ENCRYPT_ARCFOUR:
	case ENCRYPT_CAST256:
	case ENCRYPT_LOKI97:
	case ENCRYPT_WAKE:
	case ENCRYPT_ENIGMA:
	case ENCRYPT_SAFER64:
	case ENCRYPT_SAFER128:
	case ENCRYPT_SAFERPLUS:
#endif
	default:
		return false;
	}
}

nscp::encryption::any_encryption* nscp::encryption::engine::get_encryption_core(int encryption_method) {
	switch (encryption_method) {
	case ENCRYPT_NONE:
		return new no_encryption();
	case ENCRYPT_XOR:
		return new xor_encryption();
#ifdef HAVE_LIBCRYPTOPP
	case ENCRYPT_DES:
		return new cryptopp_encryption<CryptoPP::DES>();
	case ENCRYPT_3DES:
		return new cryptopp_encryption<CryptoPP::DES_EDE3>();
	case ENCRYPT_CAST128:
		return new cryptopp_encryption<CryptoPP::CAST128>();
	case ENCRYPT_XTEA:
		return new cryptopp_encryption<CryptoPP::XTEA>();
	case ENCRYPT_3WAY:
		return new cryptopp_encryption<CryptoPP::ThreeWay>();
	case ENCRYPT_BLOWFISH:
		return new cryptopp_encryption<CryptoPP::Blowfish>(56);
	case ENCRYPT_TWOFISH:
		return new cryptopp_encryption<CryptoPP::Twofish>(32);
	case ENCRYPT_RC2:
		return new cryptopp_encryption<CryptoPP::RC2>(128);
	case ENCRYPT_RIJNDAEL128:
		return new cryptopp_encryption<CryptoPP::AES>(16);
	case ENCRYPT_RIJNDAEL192:
		return new cryptopp_encryption<CryptoPP::AES>(24);
	case ENCRYPT_RIJNDAEL256:
		return new cryptopp_encryption<CryptoPP::AES>(32);
	case ENCRYPT_SERPENT:
		return new cryptopp_encryption<CryptoPP::Serpent>(32);
	case ENCRYPT_GOST:
		return new cryptopp_encryption<CryptoPP::GOST>();
#endif
	default:
		return NULL;
	}
}
std::string nscp::encryption::engine::generate_transmitted_iv(unsigned int len) {
	std::string buffer;
	buffer.resize(len);

	/*********************************************************/
	/* fill IV buffer with data that's as random as possible */
	/*********************************************************/

	/* else fall back to using the current time as the seed */
	int seed = (int)time(NULL);

	/* generate pseudo-random IV */
	srand(seed);
	for (unsigned int x = 0; x < len; x++)
		buffer[x] = (int)((256.0*rand()) / (RAND_MAX + 1.0));
	return buffer;
}

/* initializes encryption routines */
void nscp::encryption::engine::encrypt_init(std::string password, int encryption_method, std::string received_iv) {
	delete core_;
	core_ = get_encryption_core(encryption_method);
	if (core_ == NULL)
		throw encryption_exception("Failed to get encryption module for: " + boost::lexical_cast<std::string>(encryption_method));

	std::string name = core_->getName();
	// server generates IV used for encryption
	if (received_iv.empty()) {
		std::string iv = generate_transmitted_iv();
		core_->init(password, iv);
	} else	// client receives IV from server
		core_->init(password, received_iv);
}

/* encrypt a buffer */
void nscp::encryption::engine::encrypt_buffer(std::string &buffer) {
	if (core_ == NULL)
		throw encryption_exception("No encryption core!");
	core_->encrypt(buffer);
}
/* encrypt a buffer */
void nscp::encryption::engine::decrypt_buffer(std::string &buffer) {
	if (core_ == NULL)
		throw encryption_exception("No encryption core!");
	core_->decrypt(buffer);
}
std::string nscp::encryption::engine::get_rand_buffer(int length) {
	std::string buffer; buffer.resize(length);
	//unsigned char * buffer = new unsigned char[length+1];
#if HAVE_LIBCRYPTOPP
	CryptoPP::AutoSeededRandomPool rng;
	rng.GenerateBlock((byte*)&*buffer.begin(), length);
#endif
	return buffer;
}