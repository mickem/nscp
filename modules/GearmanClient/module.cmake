if(OPENSSL_FOUND)
    set(BUILD_MODULE 1)
else(OPENSSL_FOUND)
    # The Mod-Gearman payload envelope is AES-256-ECB from the OpenSSL EVP
    # API; every job and result on the wire is encrypted, so there is nothing
    # left for the module to do without it.
    set(BUILD_MODULE 0)
    set(BUILD_MODULE_SKIP_REASON "OpenSSL missing")
endif(OPENSSL_FOUND)
