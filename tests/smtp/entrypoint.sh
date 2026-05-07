#!/bin/sh
set -e

# Generate a self-signed certificate every container start. The test client
# uses --insecure-skip-verify to ignore the trust chain; this is fine for
# integration testing because the goal is to exercise the STARTTLS / TLS
# code path, not certificate validation.
CERT_DIR=/tmp/smtp-cert
mkdir -p "$CERT_DIR"
if [ ! -f "$CERT_DIR/server.crt" ]; then
    openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
        -keyout "$CERT_DIR/server.key" -out "$CERT_DIR/server.crt" \
        -subj "/CN=smtp-test" >/dev/null 2>&1 || \
    python3 -c "
import datetime, subprocess, ssl, tempfile
# Fallback: generate via Python if openssl CLI is missing in the slim image.
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, u'smtp-test')])
cert = (x509.CertificateBuilder()
    .subject_name(name).issuer_name(name)
    .public_key(key.public_key())
    .serial_number(x509.random_serial_number())
    .not_valid_before(datetime.datetime.utcnow())
    .not_valid_after(datetime.datetime.utcnow() + datetime.timedelta(days=1))
    .sign(key, hashes.SHA256()))
open('$CERT_DIR/server.crt','wb').write(cert.public_bytes(serialization.Encoding.PEM))
open('$CERT_DIR/server.key','wb').write(key.private_bytes(
    serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL,
    serialization.NoEncryption()))
"
fi

# Make sure /inbox exists and is writable. Bind-mounted as a host directory
# in the test runner so results.txt is visible from Windows.
mkdir -p /inbox
: > /inbox/messages.txt
chmod 0666 /inbox/messages.txt

echo ">> Starting test SMTP server"
echo "   plain+STARTTLS on :1025"
echo "   implicit TLS    on :1465"
echo "   credentials: $SMTP_USERNAME / $SMTP_PASSWORD"
echo "   inbox:       /inbox/messages.txt"

exec python3 /app/server.py
