#!/bin/sh
set -e

CONFIG_FILE="/var/www/html/nrdp/server/config.inc.php"

# Append overrides at the end of config.inc.php. These re-assignments
# will take effect over the defaults defined earlier in the same file.
{
    echo ""
    echo "// Overrides injected by entrypoint.sh"
    echo "\$cfg[\"authorized_tokens\"] = array(\"${TOKEN}\");"
    echo "\$cfg[\"require_basic_auth\"] = false;"
    echo "\$cfg[\"require_https\"] = false;"
    echo "\$cfg[\"disable_external_commands\"] = true;"
    echo "\$cfg[\"check_results_dir\"] = \"${CHECK_RESULTS_DIR}\";"
} >> "$CONFIG_FILE"

mkdir -p "$CHECK_RESULTS_DIR"
chown -R www-data:www-data "$CHECK_RESULTS_DIR"
chmod -R 0775 "$CHECK_RESULTS_DIR"

# Generate a self-signed certificate for HTTPS if one is not already present.
SSL_CRT="/etc/ssl/certs/nrdp-selfsigned.crt"
SSL_KEY="/etc/ssl/private/nrdp-selfsigned.key"
if [ ! -f "$SSL_CRT" ] || [ ! -f "$SSL_KEY" ]; then
    echo ">> Generating self-signed certificate for HTTPS..."
    openssl req -x509 -nodes -newkey rsa:2048 -days 365 \
        -subj "/CN=localhost" \
        -keyout "$SSL_KEY" \
        -out "$SSL_CRT" >/dev/null 2>&1
    chmod 0644 "$SSL_CRT"
    chmod 0600 "$SSL_KEY"
fi

# Point the default-ssl site at our self-signed certificate.
SSL_SITE="/etc/apache2/sites-available/default-ssl.conf"
if [ -f "$SSL_SITE" ]; then
    sed -i "s|SSLCertificateFile.*|SSLCertificateFile $SSL_CRT|" "$SSL_SITE"
    sed -i "s|SSLCertificateKeyFile.*|SSLCertificateKeyFile $SSL_KEY|" "$SSL_SITE"
fi

echo ">> NRDP config file ($CONFIG_FILE):"
cat "$CONFIG_FILE"

echo ">> Starting Apache (NRDP server, HTTP + HTTPS)..."
exec apache2-foreground
