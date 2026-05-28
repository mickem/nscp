# SMTP submission to modern relays (Gmail, Microsoft 365, …) requires STARTTLS
# or implicit TLS, so the module is only useful with OpenSSL. Skip it cleanly
# when OpenSSL is unavailable rather than failing the whole build.
if(OPENSSL_FOUND)
    set(BUILD_MODULE 1)
else()
    set(BUILD_MODULE_SKIP_REASON
        "OpenSSL not found (required for SMTP STARTTLS/SMTPS)"
    )
endif()
