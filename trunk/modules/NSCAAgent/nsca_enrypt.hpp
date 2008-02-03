
#define TRANSMITTED_IV_SIZE     128     /* size of IV to transmit - must be as big as largest IV needed for any crypto algorithm */

/********************* ENCRYPTION TYPES ****************/

#define ENCRYPT_NONE            0       /* no encryption */
#define ENCRYPT_XOR             1       /* not really encrypted, just obfuscated */

#ifdef HAVE_LIBMCRYPT
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


class nsca_encrypt {
private:
	char transmitted_iv_[TRANSMITTED_IV_SIZE];
	bool isInialized_;
	std::string password_;
	int encryption_method_;
#ifdef HAVE_LIBMCRYPT
	MCRYPT td_;
	char *key_;
	char *IV_;
	char block_buffer_;
	int blocksize_;
	int keysize_;
	std::string mcrypt_algorithm_;
	std::string mcrypt_mode_;
#endif
public:
	class exception {
		std::wstring error_;
	public:
		exception(std::wstring error) : error_(error) {}
		std::wstring getMessage() const { return error_; }

	};

	nsca_encrypt() : isInialized_(false)
#ifdef HAVE_LIBMCRYPT
		, key_(NULL), IV_(NULL) 
#endif
	{}
	~nsca_encrypt() {
#ifdef HAVE_LIBMCRYPT
		/* mcrypt cleanup */
		if(encryption_method!=ENCRYPT_NONE && encryption_method!=ENCRYPT_XOR){
			mcrypt_generic_end(td);
			delete [] key;
			key=NULL;
			delete [] IV;
			IV=NULL;
		}
#endif
	}


	static bool hasEncryption(int encryption_method) {
		switch(encryption_method) {
			case ENCRYPT_NONE:
			case ENCRYPT_XOR:
#ifdef HAVE_LIBMCRYPT
			case ENCRYPT_DES:
			case ENCRYPT_3DES:
			case ENCRYPT_CAST128:
			case ENCRYPT_CAST256:
			case ENCRYPT_XTEA:
			case ENCRYPT_3WAY:
			case ENCRYPT_BLOWFISH:
			case ENCRYPT_TWOFISH:
			case ENCRYPT_LOKI97:
			case ENCRYPT_RC2:
			case ENCRYPT_ARCFOUR:
			case ENCRYPT_RIJNDAEL128:
			case ENCRYPT_RIJNDAEL192:
			case ENCRYPT_RIJNDAEL256:
			case ENCRYPT_WAKE:
			case ENCRYPT_SERPENT:
			case ENCRYPT_ENIGMA:
			case ENCRYPT_GOST:
			case ENCRYPT_SAFER64:
			case ENCRYPT_SAFER128:
			case ENCRYPT_SAFERPLUS:
#endif
				return true;
			default:
				return false;
		}
	}

	static void generate_transmitted_iv(char *transmitted_iv){
		int x;
		int seed=0;

		/*********************************************************/
		/* fill IV buffer with data that's as random as possible */ 
		/*********************************************************/

		/* else fallback to using the current time as the seed */
		seed=(int)time(NULL);

		/* generate pseudo-random IV */
		srand(seed);
		for(x=0;x<TRANSMITTED_IV_SIZE;x++)
			transmitted_iv[x]=(int)((256.0*rand())/(RAND_MAX+1.0));

		return;
	}



	/* initializes encryption routines */
	void encrypt_init(std::string password, int encryption_method, char *received_iv){
#ifdef HAVE_LIBMCRYPT
		int i;
		int iv_size;
#endif
		if (isInialized_)
			throw exception(_T("already iniatilized!"));
		encryption_method_ = encryption_method;
		password_ = password;
		isInialized_ = true;

		/* server generates IV used for encryption */
		if(received_iv==NULL)
			generate_transmitted_iv(transmitted_iv_);

		/* client receives IV from server */
		else
			memcpy(transmitted_iv_,received_iv,TRANSMITTED_IV_SIZE);

#ifdef HAVE_LIBMCRYPT
		blocksize=1;                        /* block size = 1 byte w/ CFB mode */
		keysize=7;                          /* default to 56 bit key length */
		mcrypt_mode="cfb";                  /* CFB = 8-bit cipher-feedback mode */
		mcrypt_algorithm="unknown";
#endif



		/* get the name of the mcrypt encryption algorithm to use */
		switch(encryption_method){
		/* no encryption */
		case ENCRYPT_NONE:
			return;
			/* XOR or no encryption */
		case ENCRYPT_XOR:
			return;
#ifdef HAVE_LIBMCRYPT
		case ENCRYPT_DES:
			mcrypt_algorithm=MCRYPT_DES;
			break;
		case ENCRYPT_3DES:
			mcrypt_algorithm=MCRYPT_3DES;
			break;
		case ENCRYPT_CAST128:
			mcrypt_algorithm=MCRYPT_CAST_128;
			break;
		case ENCRYPT_CAST256:
			mcrypt_algorithm=MCRYPT_CAST_256;
			break;
		case ENCRYPT_XTEA:
			mcrypt_algorithm=MCRYPT_XTEA;
			break;
		case ENCRYPT_3WAY:
			mcrypt_algorithm=MCRYPT_3WAY;
			break;
		case ENCRYPT_BLOWFISH:
			mcrypt_algorithm=MCRYPT_BLOWFISH;
			break;
		case ENCRYPT_TWOFISH:
			mcrypt_algorithm=MCRYPT_TWOFISH;
			break;
		case ENCRYPT_LOKI97:
			mcrypt_algorithm=MCRYPT_LOKI97;
			break;
		case ENCRYPT_RC2:
			mcrypt_algorithm=MCRYPT_RC2;
			break;
		case ENCRYPT_ARCFOUR:
			mcrypt_algorithm=MCRYPT_ARCFOUR;
			break;
		case ENCRYPT_RIJNDAEL128:
			mcrypt_algorithm=MCRYPT_RIJNDAEL_128;
			break;
		case ENCRYPT_RIJNDAEL192:
			mcrypt_algorithm=MCRYPT_RIJNDAEL_192;
			break;
		case ENCRYPT_RIJNDAEL256:
			mcrypt_algorithm=MCRYPT_RIJNDAEL_256;
			break;
		case ENCRYPT_WAKE:
			mcrypt_algorithm=MCRYPT_WAKE;
			break;
		case ENCRYPT_SERPENT:
			mcrypt_algorithm=MCRYPT_SERPENT;
			break;
		case ENCRYPT_ENIGMA:
			mcrypt_algorithm=MCRYPT_ENIGMA;
			break;
		case ENCRYPT_GOST:
			mcrypt_algorithm=MCRYPT_GOST;
			break;
		case ENCRYPT_SAFER64:
			mcrypt_algorithm=MCRYPT_SAFER_SK64;
			break;
		case ENCRYPT_SAFER128:
			mcrypt_algorithm=MCRYPT_SAFER_SK128;
			break;
		case ENCRYPT_SAFERPLUS:
			mcrypt_algorithm=MCRYPT_SAFERPLUS;
			break;
#endif
		default:
			throw exception(_T("Invalid encryption algorithm!"));
		}

#ifdef HAVE_LIBMCRYPT
		/* open encryption module */
		if((td=mcrypt_module_open(mcrypt_algorithm,NULL,mcrypt_mode,NULL))==MCRYPT_FAILED){
			throw exception(_T("Could not open mcrypt algorithm '") + mcrypt_algorithm + _T("' with mode '") + mcrypt_mode + _T("'"));
		}

		/* determine size of IV buffer for this algorithm */
		iv_size=mcrypt_enc_get_iv_size(td);
		if(iv_size>TRANSMITTED_IV_SIZE){
			throw exception(_T("IV size for crypto algorithm exceeds limits"));
		}

		/* allocate memory for IV buffer */
		if((IV=new char[iv_size])==NULL){
			throw exception(_T("Could not allocate memory for IV buffer"));
		}

		/* fill IV buffer with first bytes of IV that is going to be used to crypt (determined by server) */
		for(i=0;i<iv_size;i++)
			IV[i]=transmitted_iv[i];

		/* get maximum key size for this algorithm */
		keysize=mcrypt_enc_get_key_size(td);

		/* generate an encryption/decription key using the password */
		if((key=new char[keysize])==NULL){
			throw exception(_T("Could not allocate memory for encryption/decryption key"));
			return ERROR;
		}
		ZeroMemory(key,keysize);

		if(keysize<password.length())
			strncpy(key,password.c_str(),keysize);
		else
			strncpy(key,password.c_str(),password.length());

		/* initialize encryption buffers */
		mcrypt_generic_init(td,key,keysize,IV);
#endif
	}

	/* encrypt a buffer */
	void encrypt_buffer(char *buffer,int buffer_size){
		int x;
		int y;
		int password_length;

		/* no crypt instance */
		if (!isInialized_)
			throw new exception(_T("Not initialized!"));

		/* no encryption */
		if(encryption_method_==ENCRYPT_NONE)
			return;

		/* simple XOR "encryption" - not meant for any real security, just obfuscates data, but its fast... */
		else if(encryption_method_==ENCRYPT_XOR){

			/* rotate over IV we received from the server... */
			for(y=0,x=0;y<buffer_size;y++,x++){

				/* keep rotating over IV */
				if(x>=TRANSMITTED_IV_SIZE)
					x=0;

				buffer[y]^=transmitted_iv_[x];
			}

			/* rotate over password... */
			password_length=password_.length();
			for(y=0,x=0;y<buffer_size;y++,x++){
				/* keep rotating over password */
				if(x>=password_length)
					x=0;
				buffer[y]^=password_[x];
			}
			return;
		}

#ifdef HAVE_LIBMCRYPT
		/* use mcrypt routines */
		else{
			/* encrypt each byte of buffer, one byte at a time (CFB mode) */
			for(x=0;x<buffer_size;x++)
				mcrypt_generic(td,&buffer[x],1);
		}
#endif
		return;
	}

};