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

echo ">> NRDP config file ($CONFIG_FILE):"
cat "$CONFIG_FILE"

echo ">> Starting Apache (NRDP server)..."
exec apache2-foreground
